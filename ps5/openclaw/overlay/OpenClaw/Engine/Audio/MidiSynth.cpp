#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#define TSF_IMPLEMENTATION
#include <TinySoundFont/tsf.h>
#define TML_IMPLEMENTATION
#include <TinySoundFont/tml.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "MidiSynth.h"
#include "../GameApp/BaseGameApp.h"
#include "../Logger/Logger.h"

namespace MidiSynth
{
namespace
{
    std::mutex g_Lock;
    tsf* g_Font = nullptr;
    bool g_FontFailed = false;
    tml_message* g_First = nullptr;
    tml_message* g_Cursor = nullptr;
    double g_Msec = 0.0;
    bool g_Playing = false;
    bool g_Paused = false;
    bool g_Loop = false;
    bool g_Suspended = false;
    bool g_Hooked = false;
    int g_Rate = 44100, g_Channels = 2;
    float g_Gain = 0.5f;

    // SDL_mixer's music volume 0..128 -> synth gain. TinySoundFont renders hot (measured: gain 0.023 already peaks at 4000 of 32767),
    // so the factor is low (the game limits the music volume to 20% of the range, about 0.07 here).
    float GainFor(int volume)
    {
        if (volume < 0) volume = 0;
        return (float)volume / 128.0f * 0.35f;
    }

    void ResetChannels()
    {
        tsf_reset(g_Font);
        tsf_channel_set_presetnumber(g_Font, 9, 0, 1);   // drums on channel 10
    }

    void Dispatch(const tml_message* m)
    {
        switch (m->type)
        {
            case TML_PROGRAM_CHANGE: tsf_channel_set_presetnumber(g_Font, m->channel, m->program, m->channel == 9); break;
            case TML_NOTE_ON:        tsf_channel_note_on(g_Font, m->channel, m->key, m->velocity / 127.0f); break;
            case TML_NOTE_OFF:       tsf_channel_note_off(g_Font, m->channel, m->key); break;
            case TML_PITCH_BEND:     tsf_channel_set_pitchwheel(g_Font, m->channel, m->pitch_bend); break;
            case TML_CONTROL_CHANGE: tsf_channel_midi_control(g_Font, m->channel, m->control, m->control_value); break;
        }
    }

    void Hook(void* /*udata*/, Uint8* stream, int len)
    {
        std::lock_guard<std::mutex> guard(g_Lock);
        std::memset(stream, 0, len);
        if (!g_Font || !g_Playing || g_Paused || g_Suspended)
        {
            return;
        }
        int16_t* out = reinterpret_cast<int16_t*>(stream);
        int frames = len / (int)(sizeof(int16_t) * g_Channels);
        float block[64 * 2];
        while (frames > 0)
        {
            const int n = frames < 64 ? frames : 64;
            while (g_Cursor && g_Msec >= g_Cursor->time)
            {
                Dispatch(g_Cursor);
                g_Cursor = g_Cursor->next;
            }
            if (!g_Cursor)
            {
                if (g_Loop && g_First)
                {
                    ResetChannels();
                    g_Cursor = g_First;
                    g_Msec = 0.0;
                }
                else
                {
                    g_Playing = false;
                    break;
                }
            }
            tsf_render_float(g_Font, block, n, 0);
            for (int i = 0; i < n * g_Channels; ++i)
            {
                float v = block[i];
                v = v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
                out[i] = (int16_t)(v * 32767.0f);
            }
            out += n * g_Channels;
            frames -= n;
            g_Msec += n * 1000.0 / g_Rate;
        }
    }

    bool LoadFont()
    {
        if (g_Font) return true;
        if (g_FontFailed) return false;
        Uint16 fmt = 0;
        if (!Mix_QuerySpec(&g_Rate, &fmt, &g_Channels) || fmt != AUDIO_S16SYS || g_Channels < 1 || g_Channels > 2)
        {
            LOG_ERROR("[MidiSynth] unsupported mixer format");
            g_FontFailed = true;
            return false;
        }
        const std::string dir = g_pApp->GetGameConfig()->assetsFolder + "MUSIC/";
        for (const char* name : { "CLAW4.SF2", "CLAW.SF2" })
        {
            g_Font = tsf_load_filename((dir + name).c_str());
            if (g_Font)
            {
                LOG(std::string("[MidiSynth] SoundFont ") + dir + name);
                break;
            }
        }
        if (!g_Font)
        {
            LOG_WARNING("[MidiSynth] no SoundFont found in " + dir + " (CLAW4.SF2 / CLAW.SF2): no music");
            g_FontFailed = true;
            return false;
        }
        tsf_set_output(g_Font, g_Channels == 2 ? TSF_STEREO_INTERLEAVED : TSF_MONO, g_Rate, 0.0f);
        tsf_set_max_voices(g_Font, 64);
        return true;
    }

    void InstallHook()
    {
        if (!g_Hooked)
        {
            Mix_HookMusic(Hook, nullptr);
            g_Hooked = true;
        }
    }
}

bool Play(const char* midiData, size_t size, bool looping, int volume)
{
    if (!LoadFont())
    {
        return false;
    }
    tml_message* song = tml_load_memory(midiData, (int)size);
    if (!song)
    {
        LOG_ERROR("[MidiSynth] not a valid MIDI file");
        return false;
    }
    {
        std::lock_guard<std::mutex> guard(g_Lock);
        tml_message* old = g_First;
        g_Playing = false;
        ResetChannels();
        g_First = g_Cursor = song;
        g_Msec = 0.0;
        g_Loop = looping;
        g_Paused = volume < 0;
        g_Gain = GainFor(volume);
        tsf_set_volume(g_Font, g_Gain);
        g_Playing = true;
        if (old) tml_free(old);
    }
    InstallHook();
    return true;
}

void Pause()  { std::lock_guard<std::mutex> guard(g_Lock); g_Paused = true; }
void Resume() { std::lock_guard<std::mutex> guard(g_Lock); g_Paused = false; }

void Stop()
{
    std::lock_guard<std::mutex> guard(g_Lock);
    g_Playing = false;
    if (g_Font) tsf_note_off_all(g_Font);
}

void SetVolume(int volume)
{
    std::lock_guard<std::mutex> guard(g_Lock);
    g_Gain = GainFor(volume);
    if (g_Font) tsf_set_volume(g_Font, g_Gain);
}

void Suspend()
{
    std::lock_guard<std::mutex> guard(g_Lock);
    g_Suspended = true;
}

void Unsuspend()
{
    {
        std::lock_guard<std::mutex> guard(g_Lock);
        g_Suspended = false;
    }
    if (g_Hooked)
    {
        Mix_HookMusic(Hook, nullptr);
    }
}

} // namespace MidiSynth

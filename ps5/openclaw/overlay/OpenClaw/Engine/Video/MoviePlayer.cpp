#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#include "MoviePlayer.h"
#include "../Audio/MidiSynth.h"
#include "../GameApp/BaseGameApp.h"
#include "../Logger/Logger.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <smacker.h>


namespace Movies
{
namespace
{
    // ---- audio: decoded smk audio goes through a ring that SDL_mixer pulls from (Mix_HookMusic), resampled to the mixer rate.
    struct AudioFeed
    {
        std::mutex lock;
        std::vector<int16_t> samples;   // interleaved stereo, source rate
        size_t readFrame = 0;           // index of the next source frame
        double fraction = 0.0;          // position between readFrame and readFrame+1
        double step = 1.0;              // source frames per output frame
        int outChannels = 2;
        bool enabled = false;

        size_t Available() const { return samples.size() / 2 - readFrame; }
    };

    AudioFeed g_Audio;

    void MixHook(void* /*udata*/, Uint8* stream, int len)
    {
        std::lock_guard<std::mutex> guard(g_Audio.lock);
        std::memset(stream, 0, len);
        if (!g_Audio.enabled)
        {
            return;
        }
        int16_t* out = reinterpret_cast<int16_t*>(stream);
        const int frames = len / (int)(sizeof(int16_t) * g_Audio.outChannels);
        for (int i = 0; i < frames; ++i)
        {
            if (g_Audio.Available() < 2)
            {
                break;
            }
            const int16_t* a = &g_Audio.samples[g_Audio.readFrame * 2];
            const double f = g_Audio.fraction;
            const int l = (int)(a[0] + (a[2] - a[0]) * f);
            const int r = (int)(a[1] + (a[3] - a[1]) * f);
            if (g_Audio.outChannels == 2)
            {
                out[i * 2] = (int16_t)l;
                out[i * 2 + 1] = (int16_t)r;
            }
            else
            {
                out[i] = (int16_t)((l + r) / 2);
            }
            g_Audio.fraction += g_Audio.step;
            const size_t adv = (size_t)g_Audio.fraction;
            g_Audio.fraction -= (double)adv;
            g_Audio.readFrame += adv;
        }
        // Compact the ring once a lot of it has been consumed.
        if (g_Audio.readFrame > 200000)
        {
            g_Audio.samples.erase(g_Audio.samples.begin(), g_Audio.samples.begin() + g_Audio.readFrame * 2);
            g_Audio.readFrame = 0;
        }
    }

    void PushAudio(const unsigned char* data, unsigned long size, unsigned char channels, unsigned char bits)
    {
        if (!data || size == 0)
        {
            return;
        }
        std::lock_guard<std::mutex> guard(g_Audio.lock);
        const size_t bytesPerSample = bits / 8;
        const size_t frames = size / (bytesPerSample * channels);
        for (size_t i = 0; i < frames; ++i)
        {
            int16_t v[2];
            for (int c = 0; c < 2; ++c)
            {
                const size_t idx = (i * channels + (channels == 1 ? 0 : c)) * bytesPerSample;
                if (bits == 16)
                {
                    v[c] = (int16_t)(data[idx] | (data[idx + 1] << 8));
                }
                else
                {
                    v[c] = (int16_t)((data[idx] - 128) << 8);
                }
            }
            g_Audio.samples.push_back(v[0]);
            g_Audio.samples.push_back(v[1]);
        }
    }

    struct DecodedFrame
    {
        unsigned long number;
        std::vector<unsigned char> pixels;      // palette indices, w*h
        unsigned char palette[256 * 3];
    };

    std::map<int, int> g_LevelMovies = { {1, 1}, {3, 2}, {5, 3}, {7, 4}, {9, 5}, {11, 6}, {13, 7} };
    std::vector<int> g_EndMovies = { 8, 9 };
    int g_StartupMovie = 0;
    bool g_ConfigLoaded = false;
    bool g_StartupPlayed = false;

    std::string MoviesDir()
    {
        return g_pApp->GetGameConfig()->assetsFolder + "MOVIES/";
    }

    void LoadConfig()
    {
        if (g_ConfigLoaded)
        {
            return;
        }
        g_ConfigLoaded = true;
        FILE* f = fopen((MoviesDir() + "movies.txt").c_str(), "r");
        if (!f)
        {
            return;
        }
        g_LevelMovies.clear();
        // The ending keeps its built-in list unless the file has "end" lines (an older movies.txt has none)
        bool endListed = false;
        g_StartupMovie = -1;
        char line[256];
        while (fgets(line, sizeof line, f))
        {
            int a = 0, b = 0;
            if (sscanf(line, "startup %d", &a) == 1)
            {
                g_StartupMovie = a;
            }
            else if (sscanf(line, "level %d %d", &a, &b) == 2)
            {
                g_LevelMovies[a] = b;
            }
            else if (sscanf(line, "end %d", &a) == 1)
            {
                if (!endListed)
                {
                    g_EndMovies.clear();
                    endListed = true;
                }
                g_EndMovies.push_back(a);
            }
        }
        fclose(f);
    }
}

bool PlayIndex(int index)
{
    char name[32];
    snprintf(name, sizeof name, "movie%02d.smk", index);
    return PlayFile(MoviesDir() + name);
}

bool PlayFile(const std::string& path)
{
    SDL_Renderer* renderer = g_pApp->GetRenderer();
    smk s = smk_open_file(path.c_str(), SMK_MODE_DISK);
    if (!s)
    {
        LOG_WARNING("[Movies] cannot open " + path);
        return false;
    }

    unsigned long w = 0, h = 0, frameCount = 0;
    unsigned char yScale = 0;
    double usf = 0;
    smk_info_all(s, nullptr, &frameCount, &usf);
    smk_info_video(s, &w, &h, &yScale);
    unsigned char trackMask = 0, channels[7] = {0}, bitdepth[7] = {0};
    unsigned long rate[7] = {0};
    smk_info_audio(s, &trackMask, channels, bitdepth, rate);
    const bool hasAudio = (trackMask & SMK_AUDIO_TRACK_0) && (bitdepth[0] == 8 || bitdepth[0] == 16) && channels[0] >= 1 && channels[0] <= 2;
    smk_enable_video(s, 1);
    if (hasAudio)
    {
        smk_enable_audio(s, 0, 1);
    }
    const unsigned long outH = (yScale == SMK_FLAG_Y_NONE) ? h : h * 2;
    LOG("[Movies] " + path + ": " + std::to_string(w) + "x" + std::to_string(h) + (outH != h ? " (doubled)" : "") + ", " +
        std::to_string(frameCount) + " frames");

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)w, (int)outH);
    if (!texture)
    {
        LOG_ERROR(std::string("[Movies] cannot create texture: ") + SDL_GetError());
        smk_close(s);
        return false;
    }

    // Audio through the mixer.
    int mixRate = 0, mixChannels = 0;
    Uint16 mixFormat = 0;
    bool audioOn = false;
    if (hasAudio && Mix_QuerySpec(&mixRate, &mixFormat, &mixChannels) && mixFormat == AUDIO_S16SYS && mixChannels >= 1 && mixChannels <= 2)
    {
        std::lock_guard<std::mutex> guard(g_Audio.lock);
        g_Audio.samples.clear();
        g_Audio.readFrame = 0;
        g_Audio.fraction = 0.0;
        g_Audio.step = (double)rate[0] / (double)mixRate;
        g_Audio.outChannels = mixChannels;
        g_Audio.enabled = false;
        audioOn = true;
    }
    else if (hasAudio)
    {
        LOG_WARNING("[Movies] mixer format not supported for movie audio, playing silent");
    }

    // The renderer is scaled for the game (640x480 units); movies use the real output size.
    float oldScaleX = 1.0f, oldScaleY = 1.0f;
    SDL_RenderGetScale(renderer, &oldScaleX, &oldScaleY);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    int outW = 0, outHeight = 0;
    SDL_GetRendererOutputSize(renderer, &outW, &outHeight);
    SDL_Rect dst;
    {
        const double aspect = (double)w / (double)outH * (outH == h ? 1.0 : 1.0);
        double dw = outW, dh = outW / aspect;
        if (dh > outHeight)
        {
            dh = outHeight;
            dw = outHeight * aspect;
        }
        dst = { (int)((outW - dw) / 2), (int)((outHeight - dh) / 2), (int)dw, (int)dh };
    }

    std::deque<DecodedFrame> queue;
    bool decodeDone = false;
    unsigned long decoded = 0;
    char status = SMK_MORE;
    bool first = true;
    auto decodeOne = [&]() {
        if (decodeDone)
        {
            return;
        }
        status = first ? smk_first(s) : smk_next(s);
        first = false;
        if (status < 0)
        {
            decodeDone = true;
            return;
        }
        DecodedFrame df;
        df.number = decoded++;
        const unsigned char* video = smk_get_video(s);
        df.pixels.assign(video, video + w * h);
        std::memcpy(df.palette, smk_get_palette(s), sizeof df.palette);
        queue.push_back(std::move(df));
        if (audioOn)
        {
            PushAudio(smk_get_audio(s, 0), smk_get_audio_size(s, 0), channels[0], bitdepth[0]);
        }
        if (status == SMK_DONE || status == SMK_LAST || decoded >= frameCount)
        {
            decodeDone = true;
        }
    };

    // Nothing else plays during a cutscene: stop sound effects / menu music, silence the MIDI synth.
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    MidiSynth::Suspend();

    // Drop key presses that were already pending, then play.
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_KEYDOWN);
    while (queue.size() < 8 && !decodeDone)
    {
        decodeOne();
    }
    if (audioOn)
    {
        std::lock_guard<std::mutex> guard(g_Audio.lock);
        g_Audio.enabled = true;
        Mix_HookMusic(MixHook, nullptr);
    }

    const double frameMs = usf / 1000.0;
    const Uint32 t0 = SDL_GetTicks();
    DecodedFrame current;
    bool haveCurrent = false;
    bool skipped = false;
    std::vector<Uint32> argb(w * outH);

    while (!skipped)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                SDL_PushEvent(&e);
                skipped = true;
            }
            else if (e.type == SDL_KEYDOWN && e.key.repeat == 0 && SDL_GetTicks() - t0 > 400)
            {
                const SDL_Scancode sc = e.key.keysym.scancode;
                if (sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_KP_ENTER)
                {
                    skipped = true;
                }
            }
        }

        while (queue.size() < 8 && !decodeDone)
        {
            decodeOne();
        }

        const double now = (double)(SDL_GetTicks() - t0);
        bool changed = false;
        while (!queue.empty() && queue.front().number * frameMs <= now)
        {
            current = std::move(queue.front());
            queue.pop_front();
            haveCurrent = true;
            changed = true;
        }

        if (changed && haveCurrent)
        {
            for (unsigned long y = 0; y < h; ++y)
            {
                for (unsigned long x = 0; x < w; ++x)
                {
                    const unsigned char* c = &current.palette[current.pixels[y * w + x] * 3];
                    const Uint32 px = 0xFF000000u | (c[0] << 16) | (c[1] << 8) | c[2];
                    argb[y * (outH == h ? 1 : 2) * w + x] = px;
                    if (outH != h)
                    {
                        argb[(y * 2 + 1) * w + x] = px;
                    }
                }
            }
            SDL_UpdateTexture(texture, nullptr, argb.data(), (int)(w * sizeof(Uint32)));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_RenderPresent(renderer);
        }

        if (queue.empty() && decodeDone)
        {
            // Let the audio of the last frames play out.
            std::lock_guard<std::mutex> guard(g_Audio.lock);
            if (!audioOn || g_Audio.Available() < 2)
            {
                break;
            }
        }
        SDL_Delay(2);
    }

    if (audioOn)
    {
        Mix_HookMusic(nullptr, nullptr);
        std::lock_guard<std::mutex> guard(g_Audio.lock);
        g_Audio.enabled = false;
        g_Audio.samples.clear();
        g_Audio.readFrame = 0;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_RenderSetScale(renderer, oldScaleX, oldScaleY);
    MidiSynth::Unsuspend();
    SDL_DestroyTexture(texture);
    smk_close(s);
    SDL_FlushEvent(SDL_KEYDOWN);
    SDL_FlushEvent(SDL_KEYUP);
    return true;
}

void OnStartup()
{
    LoadConfig();
    if (g_StartupPlayed)
    {
        return;
    }
    g_StartupPlayed = true;
    // Test helper ("level 14" in the port's debug.txt): show the ending videos instead of the logo, then the menu
    if (getenv("CLAW_PLAY_ENDING"))
    {
        OnGameEnd();
        return;
    }
    if (g_StartupMovie >= 0)
    {
        PlayIndex(g_StartupMovie);
    }
}

void OnLevelStart(int levelNumber, bool isRestart)
{
    LoadConfig();
    if (isRestart)
    {
        return;
    }
    auto it = g_LevelMovies.find(levelNumber);
    if (it != g_LevelMovies.end())
    {
        PlayIndex(it->second);
    }
}

void OnGameEnd()
{
    LoadConfig();
    LOG("[Movies] ending: " + std::to_string(g_EndMovies.size()) + " videos");
    for (int index : g_EndMovies)
    {
        PlayIndex(index);
    }
}

} // namespace Movies

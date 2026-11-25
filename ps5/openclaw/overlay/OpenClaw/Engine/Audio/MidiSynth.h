#ifndef __MIDI_SYNTH_H__
#define __MIDI_SYNTH_H__

#include <cstddef>

//---------------------------------------------------------------------------------------------------------------------
// Software MIDI player (TinySoundFont + the game's own SoundFonts MUSIC/CLAW4.SF2 or CLAW.SF2), fed to SDL_mixer through
// Mix_HookMusic. Used on platforms without a MIDI backend (consoles). Standard MIDI data in, 16 bit PCM out.
//---------------------------------------------------------------------------------------------------------------------
namespace MidiSynth
{
    // Starts a song; volume is SDL_mixer's 0..128. Returns false when no SoundFont could be loaded (caller may fall back).
    bool Play(const char* midiData, size_t size, bool looping, int volume);
    void Pause();
    void Resume();
    void Stop();
    void SetVolume(int volume);

    // Cutscenes take over the mixer hook: the synth goes quiet and takes it back afterwards.
    void Suspend();
    void Unsuspend();
}

#endif

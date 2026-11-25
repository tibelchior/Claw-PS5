#ifndef __MOVIE_PLAYER_H__
#define __MOVIE_PLAYER_H__

#include <string>

//---------------------------------------------------------------------------------------------------------------------
// Plays the original game's cutscenes (Smacker .smk files extracted from MOVIES/CLAW.FEC, see tools/extract_fec.py).
// Files are looked up as <AssetsFolder>MOVIES/movieNN.smk. Playback is blocking and can be skipped with
// Space / Enter / Escape. Which movie plays when is given by <AssetsFolder>MOVIES/movies.txt:
//     startup <N>          shown once when the game starts
//     level <L> <N>        shown when level L starts from the menu (not after dying); level 1 is the intro
// Without that file the built-in defaults are used. Missing movie files are skipped silently.
//---------------------------------------------------------------------------------------------------------------------
namespace Movies
{
    // Plays one file, returns false when it could not be played at all.
    bool PlayFile(const std::string& path);
    bool PlayIndex(int index);

    void OnStartup();
    void OnLevelStart(int levelNumber, bool isRestart);
}

#endif

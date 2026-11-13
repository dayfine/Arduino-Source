/*  Fade Profiles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Per-game fade thresholds (FadeProfile) and the transition kinds the tracker
 *  reports. Their definitions are in Pokemon_Transitions.cpp.
 *
 */

#ifndef PokemonAutomation_Pokemon_FadeProfiles_H
#define PokemonAutomation_Pokemon_FadeProfiles_H

#include <cstdint>
#include <string>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Whether the screen differs after a transition from before it. The
//  tracker has only frame statistics; the caller compares the frames, or
//  says it did not.
enum class SceneChange{
    UNKNOWN,
    SAME,
    DIFFERENT,
};


//  A game's fades, as data. Two tiers of the same test: a frame is uniform
//  (the fade's hold) when its middle's stddev sum is at most
//  `uniform_max_stddev_sum`, and partial (on the way down or up) when it is
//  at most `partial_max_stddev_sum`. Either way it is dark or white by its
//  rgb sum.
struct FadeProfile{
    double uniform_max_stddev_sum = 10;
    double partial_max_stddev_sum = 60;
    //  Dark at or below this rgb sum; a partial frame up to `partial_dark_max_sum`.
    double dark_max_sum = 225;
    double partial_dark_max_sum = 300;
    //  White at or above this rgb sum; a partial frame from `partial_white_min_sum`.
    double white_min_sum = 500;
    double partial_white_min_sum = 450;

    //  Partial frames for longer than this, with no hold, are the scene's
    //  own look (a dim cave floor, a street at night), not a fade.
    Milliseconds max_fall = Milliseconds(1000);
    //  A transition still dark or white after this has timed out: the
    //  screen did not come back.
    Milliseconds max_hold = Milliseconds(8000);

    //  Whether the game fades to white between maps (BDSP's cave exits).
    //  Without, every white is an encounter's flash.
    bool has_white_fade = true;
    //  A white held this long or less, back on the same scene, is a flash.
    Milliseconds flash_max_hold = Milliseconds(150);
    //  A white held this long or more, not back on the same scene, is a
    //  white fade. Between the two the white is unsure.
    Milliseconds white_fade_min_hold = Milliseconds(400);
};

//  BDSP. The uniform tier is the shared walk's (FADE_MAX_STDDEV_SUM, the
//  Oreburgh Gate's textured floor is no fade); the partial
//  tier is run_into_door()'s 60, which caught the doorway's dim fade frames.
//  A cave mouth's fade is dim (45 to 64 a channel), hence the dark sum of 225.
//  The flash and white-fade holds are provisional: no console frames of an
//  encounter flash and a cave exit have been timed side by side yet, so the
//  band between them is wide and reports WHITE_UNSURE.
extern const FadeProfile BDSP_FADE_PROFILE;
//  FRLG. The GBA fades to true black, so the dark tiers are the shared
//  BlackScreenDetector's 100 and a little above; it has no white fade between
//  maps, so every white is an encounter's flash.
extern const FadeProfile FRLG_FADE_PROFILE;


//  What a transition was.
enum class TransitionKind : uint8_t{
    FADE_BLACK,     //  A door, a cave mouth, a scene cut, a battle's own fade.
    FADE_WHITE,     //  A cave exit: a long white onto another scene.
    FLASH,          //  An encounter's flash: a short white, back on the scene.
    WHITE_UNSURE,   //  A white that may be either: wait for a battle.
};
//  "black fade", for the log.
std::string transition_kind_name(TransitionKind kind);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

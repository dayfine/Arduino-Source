/*  PokemonLZA AutoStory - Lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pressing through lines that follow a press of the player's (a throw,
 *  a menu's reward), on the story backend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryLines_H
#define PokemonAutomation_PokemonLZA_AutoStoryLines_H

#include "Common/Cpp/Time.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What press_a_through_lines() saw.
struct LinesPassed{
    //  A text box or the advance chevron showed.
    bool text = false;
    //  A tutorial card showed (TUTORIAL_CARD_READING).
    bool card = false;
    //  The lines went on past the limit.
    bool timed_out = false;
};

//  A at each look, LINES_POLL apart, with a text box, the advance chevron
//  or a tutorial card, until none has shown for `quiet`, by the looks'
//  times; `limit` at most.
LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit);

//  B, a second apart, until the party bar or a text box shows: out of a
//  menu, back to the overworld or to the talk that follows it. Returns
//  whether one showed within `presses` presses.
bool press_b_out_of_menu(PlzaBackend& backend, size_t presses);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

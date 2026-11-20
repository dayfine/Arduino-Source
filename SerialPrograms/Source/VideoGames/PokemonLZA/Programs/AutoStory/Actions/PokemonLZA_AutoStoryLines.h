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
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What press_a_through_lines() saw.
struct LinesPassed{
    //  A text box or the advance chevron.
    bool text = false;
    bool card = false;
    bool timed_out = false;
};

//  A while text or a tutorial card shows, until none has shown for `quiet`; `limit` at most.
LinesPassed press_a_through_lines(PlzaBackend& backend, Milliseconds quiet, Milliseconds limit);

//  B until the party bar or a text box shows; false if neither did within `presses`.
bool press_b_out_of_menu(PlzaBackend& backend, size_t presses);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Overworld Walk Gates
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console steps of a route through gates (RouteAction::OPEN_GATE and
 *  PRESS_SWITCH), next to the other steps of OverworldWalkSteps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkGates_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkGates_H

#include <functional>
#include <optional>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What shows after an A at a statue: its YES/NO prompt, a page of text
//  still to turn ("A secret switch!" with its page arrow), or neither.
enum class StatueScreen{
    PROMPT,
    MORE_TEXT,
    NOTHING,
};

//  Presses A and looks (`press_a_and_look`) until the statue's prompt
//  shows, turning at most `max_pages` pages of text on the way. Returns the
//  presses it took, or nothing if no prompt showed. "A secret switch!" is
//  a page of its own before "Press it?" (pokefirered
//  PokemonMansion_Text_PressSecretSwitch: a \p between them).
std::optional<int> page_to_switch_prompt(const std::function<StatueScreen()>& press_a_and_look, int max_pages);


namespace OverworldWalkSteps{


//  Facing a card-key gate `direction` (turning first unless `facing_gate`),
//  presses A: with the Card Key the game says so and the door opens. B
//  clears the message, then the walk waits for the door to settle.
void open_gate(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_gate);

//  Facing a statue `direction` (turning first unless `facing_statue`),
//  presses A, turns the page to the prompt with A (page_to_switch_prompt()),
//  takes Yes, and clears the text with B.
void press_switch(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_statue);



} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

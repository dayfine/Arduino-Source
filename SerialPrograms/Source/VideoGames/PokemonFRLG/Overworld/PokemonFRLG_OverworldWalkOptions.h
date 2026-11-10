/*  Overworld Walk Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a walk of an overworld route is told: what can stop the player
 *  part-way through a leg (OverworldInterrupt), and how fast to move.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkOptions_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkOptions_H

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Something that stops the player part-way through a leg: a wild battle, a
//  message. See Pokemon::LegInterrupt.
using OverworldInterrupt = Pokemon::LegInterrupt<ConsoleHandle, ProControllerContext>;
//  Overworld movement interruption triggers.
using OverworldInterrupts = Pokemon::LegInterrupts<ConsoleHandle, ProControllerContext>;

//  What to watch for on a route's steps: `wild` on steps where a wild
//  Pokemon can appear (RouteStep::wild), `other` on the rest. Watching for a
//  battle where none can start only risks a false one: walking to the edge
//  of a small room, the screen darkens like a battle's flash (console B,
//  2026-11-01, the bedroom's east wall).
struct OverworldInterruptSet{
    OverworldInterrupts wild;
    OverworldInterrupts other;
};

//  A wild battle, run from, and a message (e.g. "REPEL's effect wore off..."),
//  closed with B. See OverworldInterruptSet.
const OverworldInterruptSet& default_overworld_interrupts();

//  The same, except that after a message another Max Repel is used, from
//  slot `max_repel_slot` (from 0) of the Items pocket. Any message on the
//  overworld is taken to be the one saying the last Repel wore off.
OverworldInterruptSet overworld_interrupts_with_repel(int max_repel_slot);

//  From the overworld, uses the Max Repel in `slot` (from 0) of the Items
//  pocket and returns to the overworld.
void use_max_repel_from_bag(ConsoleHandle& console, ProControllerContext& context, int slot);


//  How the player moves on foot: walking, or running with B held, which
//  needs the Running Shoes (FLAG_SYS_B_DASH, from Oak's aide in Pewter City
//  after Brock). On the water the player surfs at its own pace either way.
enum class MoveSpeed{
    WALK,
    RUN,
};
//  Name of a movement speed.
std::string_view move_speed_name(MoveSpeed speed);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

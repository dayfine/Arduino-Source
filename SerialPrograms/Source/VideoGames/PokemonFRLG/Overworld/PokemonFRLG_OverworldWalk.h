/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks a planned overworld route with the D-pad. FRLG moves one tile per
 *  16 frames and finishes a step once it starts, so a leg is one hold timed
 *  to land mid-way between the last tile and one more. Warps are waited out
 *  by their fade; Surf goes through its dialog.
 *
 *  A message or a wild battle can stop the player part-way through a leg.
 *  Each kind is an OverworldInterrupt: the walker watches for all of them
 *  while it holds the D-pad, and when one shows it stops, has it cleared and
 *  walks the rest of the leg.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalk_H
#define PokemonAutomation_PokemonFRLG_OverworldWalk_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkMoves.h"
#include "PokemonFRLG_OverworldWalkBattles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  Walks `route` from where it was planned, at `speed`. The player must be
//  standing still on the overworld and not on the bike. `facing` is the way
//  the player faces at the start; if not given, it is read from the screen.
//  Throws OperationFailedException if a warp's fade or the Surf prompt
//  doesn't show, or a leg is interrupted too many times.
void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<Direction> facing = std::nullopt,
    const OverworldInterruptSet& interrupts = default_overworld_interrupts(),
    MoveSpeed speed = MoveSpeed::WALK
);


//  The pieces walk_overworld_route() is made of, for a walker that runs a
//  route its own way. Each throws OperationFailedException as
//  walk_overworld_route() describes.
namespace OverworldWalkSteps{

//  After a hold onto `step`'s warp: waits out its fade.
void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step);

//  Before `step`: fails unless the overworld is up.
void check_overworld(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step);

//  Before `step`: if a message is open (a trainer who saw the player on the
//  last leg and spoke after its watch), hands what shows to `interrupts`,
//  once, instead of failing at once. check_overworld() then fails if the
//  overworld still isn't up.
void clear_open_message(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& interrupts, const RouteStep& step
);

//  The way the player faces, read from the screen; nothing if it can't be read.
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context);

//  Facing `direction` (turning first unless `facing_water`), starts Surf.
void start_surf(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_water);

} // namespace OverworldWalkSteps


//  Registers overworld walk unit tests.
void add_tests_OverworldWalk(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

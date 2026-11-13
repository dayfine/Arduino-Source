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
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "Pokemon/Navigation/Pokemon_LegWalker.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkMoves.h"
#include "PokemonFRLG_OverworldWalkBattles.h"
#include "PokemonFRLG_OverworldFacing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  Walks `route` from where it was planned, at `speed`. The player must be
//  standing still on the overworld and not on the bike. `facing` is the way
//  the player faces at the start; if not given, it is read from the screen
//  with `player`'s sprites.
//  Throws OperationFailedException if a warp's fade or the Surf prompt
//  doesn't show, or a leg is interrupted too many times.
void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<Direction> facing = std::nullopt,
    const OverworldInterruptSet& interrupts = default_overworld_interrupts(),
    MoveSpeed speed = MoveSpeed::WALK,
    PlayerGender player = PlayerGender::BOY
);


//  The pieces walk_overworld_route() is made of, for a walker that runs a
//  route its own way. Each throws OperationFailedException as
//  walk_overworld_route() describes.
namespace OverworldWalkSteps{

//  The hold watchers (Pokemon::walk_leg()) of a leg onto `step`: `fade`,
//  which watches for the warp's fade from the start of the hold, if the
//  step warps. A fade can be over before the hold ends.
std::vector<Pokemon::WatchAlong*> warp_hold_watchers(const RouteStep& step, Pokemon::WatchAlong& fade);

//  After a hold onto `step`'s warp: waits out its fade, unless `fade`, a
//  BlackScreenOverWatcher watched along the hold, saw it over already;
//  else waits on with that watcher, which may have seen the black. Then
//  fails unless the screen after the fade differs from `before`, taken
//  before the hold (warp_left_scene()).
void wait_out_warp(
    ConsoleHandle& console, ProControllerContext& context, const RouteStep& step,
    Pokemon::WatchAlong& fade, const VideoSnapshot& before
);
//  The same, watching only from now: for a caller that saw the black itself.
//  A step whose fade only dims (RouteStep::fade_dims) is confirmed by the
//  scene from `before` gone and the screen settled instead.
void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step, const VideoSnapshot& before);

//  Whether a warp onto `step` whose fade went unseen was taken anyway: the
//  game box of `later` shows another scene than `before` (taken before the
//  steps onto the warp; warp_left_scene()), is not black, and matches
//  `now`, taken a little before it, so no fade is still on. The Department
//  Store's lift room is mostly black around its lit floor: a watch for the
//  fade started once it was over never sees black end (console B,
//  2026-11-12 00:39, 10.01 lift_to_1f: two steps more into the lift's back
//  wall, then "no fade in and out").
bool arrived_without_fade(
    const ImageViewRGB32& before, const ImageViewRGB32& now, const ImageViewRGB32& later,
    const ImageFloatBox& game, const RouteStep& step
);

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

//  Facing `direction` (turning first unless `facing_water`), starts Surf.
void start_surf(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_water);

//  Facing a card-key gate `direction` (turning first unless `facing_gate`),
//  presses A: with the Card Key the game says so and the door opens. B
//  clears the message, then the walk waits for the door to settle.
void open_gate(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_gate);

//  Facing a statue `direction` (turning first unless `facing_statue`),
//  presses A, takes Yes at the prompt, and clears the text with B.
void press_switch(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_statue);

} // namespace OverworldWalkSteps


//  Registers overworld walk unit tests.
void add_tests_OverworldWalk(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

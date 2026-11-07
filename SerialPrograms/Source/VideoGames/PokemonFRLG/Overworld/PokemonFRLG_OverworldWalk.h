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
const char* move_speed_name(MoveSpeed speed);


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

//  Walking duration for a single tile (16 frames at 60 fps).
inline constexpr Milliseconds TILE_TIME(267);
//  Surfing duration for a single tile.
inline constexpr Milliseconds SURF_TILE_TIME(133);
//  8 frames: running is 2 pixels a frame, twice walking. Not measured on the
//  console yet; the walker logs each leg to a wall (speed_note()) for that.
inline constexpr Milliseconds RUN_TILE_TIME(133);
//  turns a player facing another way, without a step.
inline constexpr Milliseconds TURN_TAP(40);
//  duration to wait after turning to allow state to settle.
inline constexpr Milliseconds TURN_SETTLE(300);
//  short enough to turn without stepping.
inline constexpr Milliseconds FACE_HOLD(40);
//  duration of button release between movement steps.
inline constexpr Milliseconds STEP_RELEASE(100);
//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
inline constexpr Milliseconds AFTER_WARP(1500);
//  After a warp's fade, which a wild battle's flashes and wipe can pass for:
//  a battle's HP box shows within this long (console B, 2026-11-01).
inline constexpr Milliseconds AFTER_WARP_BATTLE_WATCH(5000);
//  A message set off by a leg's last step shows after the hold ends. Caught
//  here, before the next leg's turn tap, which the message would swallow.
inline constexpr Milliseconds AFTER_LEG_WATCH(500);
//  The same after a leg through grass. A battle whose flashes are missed is
//  seen only by its "Wild <Pokemon> appeared!", 3.7s after its step's press
//  (twice on Route 1, console B, 2026-11-01, run 7). Unless the walker waits
//  that long, the next leg is walked into the battle and lost: there, "east
//  4" and "west 4".
inline constexpr Milliseconds AFTER_WILD_LEG_WATCH(3500);
//  How long clear_open_message() watches for the message it saw.
inline constexpr Milliseconds OPEN_MESSAGE_WATCH(1000);
//  Maximum interruptions before a leg is abandoned.
inline constexpr size_t MAX_INTERRUPTIONS_PER_LEG = 5;

//  Maps Direction to corresponding DpadPosition.
DpadPosition dpad(Direction direction);

//  The leg mover: holds the D-pad. FRLG moves exactly a tile per 16 frames.
void hold_dpad(ProControllerContext& context, Direction direction, Milliseconds duration);

//  The leg mover for running: holds the D-pad with B for the whole hold.
//  Turn taps go without B: a tap only turns the player.
void hold_dpad_running(ProControllerContext& context, Direction direction, Milliseconds duration);

//  How long one tile of `step` takes at `speed`: on the water, the Surf
//  pace whatever the speed.
Milliseconds tile_time(const RouteStep& step, MoveSpeed speed);

//  The mover for `step` at `speed`: hold_dpad_running() for a run on foot,
//  hold_dpad() otherwise (B does nothing on the water).
Pokemon::LegMover<ProControllerContext> leg_mover(const RouteStep& step, MoveSpeed speed);

//  How long the walker holds the D-pad for a WALK `step` at `speed`
//  (Pokemon::leg_hold()).
Milliseconds leg_hold_time(const RouteStep& step, MoveSpeed speed);

//  For a leg to a wall on foot, the line the walker logs so a console run
//  can check the pace: "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms",
//  the tiles at the assumed pace and the hold. Empty for other steps.
std::string speed_note(const RouteStep& step, MoveSpeed speed);

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

//  After `step`'s warp: fails, once the battle is fled, if its fade was a
//  wild battle's.
void check_warp_was_no_battle(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step);

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

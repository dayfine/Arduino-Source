/*  Overworld Recovery Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <optional>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Async/InferenceSession.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_OverworldBikeTaps.h"
#include "PokemonFRLG_OverworldLateBattle.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"
#include "PokemonFRLG_OverworldWalkGates.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace MovementMechanics;
using namespace OverworldWalkSteps;


namespace{

//  An open leg stops at its first interruption: walking on from how far it
//  got went wrong too often on the console. The timing only narrows where
//  the player is, and the route finds them again at a wall. See
//  walk_overworld_route_with_recovery().
Pokemon::LegSpec leg_spec(const RouteStep& step, MoveSpeed speed){
    Pokemon::LegSpec leg;
    leg.direction = step.direction;
    leg.tiles = step.tiles + step.warp_slack;
    leg.to_wall = step.to_wall;
    leg.tile_time = tile_time(step, speed);
    leg.stop_on_interrupt = !step.to_wall;
    leg.stop_if_blocked = step.to_wall;
    leg.stalled_steps = step.connection_steps;
    return leg;
}

//  How one step's walk went: if an open leg stopped part-way, how far the
//  timing puts the player along it; when its hold started; and whether the
//  screen was a dark cave's then (dark_cave_screen()).
struct StepWalk{
    std::optional<WalkedTiles> stopped;
    WallClock hold_start;
    bool dark = false;
    //  Stopped by a Bicycle tap that didn't move (StepOutcome::blocked).
    bool blocked = false;
    HoldMovement movement = HoldMovement::UNMEASURED;
};

//  The four edge quarters of the game screen `game`: top, bottom, left and
//  right, all clear of the player in the middle.
std::array<ImageFloatBox, 4> edge_bands(const ImageFloatBox& game){
    const double w = game.width * 0.25;
    const double h = game.height * 0.25;
    return {
        ImageFloatBox(game.x, game.y, game.width, h),
        ImageFloatBox(game.x, game.y + game.height - h, game.width, h),
        ImageFloatBox(game.x, game.y, w, game.height),
        ImageFloatBox(game.x + game.width - w, game.y, w, game.height),
    };
}

//  How much hold_scroll_change() may read for a player who stood still (an
//  animated tile, water). Not yet measured on the console: a guess, set low
//  so that only a screen that did not move at all reads as not moved.
constexpr double HOLD_UNMOVED_RMSD = 6.0;

//  How long measure_movement() waits for a frame that can show the hold
//  (frame_shows_hold()), a wait at a time, when the feed has stalled.
constexpr Milliseconds STALE_FRAME_WAIT = std::chrono::milliseconds(100);
constexpr int STALE_FRAME_WAITS = 20;

//  Whether `band` of `screen` (in the game screen `game`) looks the same one
//  tile over along `direction`, as a stand-still would read: a scroll that
//  way by whole tiles leaves it the same. One color off the map's edge does
//  (Pokemon Mansion 1F's bottom row, 13.02), and so do a wall or floorboards
//  whose pattern repeats along the walk (Pokemon Mansion B1F's south wall,
//  0.0 after a step west, 13.03; both console B, 2026-11-14).
bool same_one_tile_over(const ImageViewRGB32& screen, const ImageFloatBox& band, const ImageFloatBox& game, Direction direction){
    const ImageViewRGB32 view = extract_box_reference(screen, band);
    const bool across = direction == Direction::EAST || direction == Direction::WEST;
    const size_t shift = across
        ? (size_t)(screen.width() * game.width / 15 + 0.5)
        : (size_t)(screen.height() * game.height / 10 + 0.5);
    const size_t length = across ? view.width() : view.height();
    if (shift == 0 || shift >= length){
        return false;
    }
    const ImageViewRGB32 from = across
        ? view.sub_image(0, 0, length - shift, view.height())
        : view.sub_image(0, 0, view.width(), length - shift);
    const ImageViewRGB32 to = across
        ? view.sub_image(shift, 0, length - shift, view.height())
        : view.sub_image(0, shift, view.width(), length - shift);
    return ImageMatch::pixel_RMSD(from, to) <= HOLD_UNMOVED_RMSD;
}

} // namespace

double tap_scroll_change(const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game){
    double most = 0;
    for (const ImageFloatBox& band : edge_bands(game)){
        most = std::max(most, ImageMatch::pixel_RMSD(extract_box_reference(before, band), extract_box_reference(after, band)));
    }
    return most;
}

namespace{

//  The game screen's map in pixels: 15 tiles across, 10 down.
struct ScreenTiles{
    size_t x;
    size_t y;
    size_t width;
    size_t height;
    size_t tile_width;
    size_t tile_height;
};
ScreenTiles screen_tiles(const ImageViewRGB32& screen, const ImageFloatBox& game){
    ScreenTiles out;
    out.x = (size_t)(screen.width() * game.x + 0.5);
    out.y = (size_t)(screen.height() * game.y + 0.5);
    out.width = (size_t)(screen.width() * game.width + 0.5);
    out.height = (size_t)(screen.height() * game.height + 0.5);
    out.tile_width = out.width / 15;
    out.tile_height = out.height / 10;
    return out;
}

//  How closely the RMSD over the map moved a tile must beat the one over
//  the map as it was for a tap to read as a step.
constexpr double WATER_STEP_RATIO = 0.8;

} // namespace

bool tap_moved_on_water(const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction){
    if (before.width() != after.width() || before.height() != after.height()){
        return tap_scroll_change(before, after, game) > WALL_TAP_MOVED_RMSD;
    }
    const ScreenTiles s = screen_tiles(before, game);
    //  A step `direction` moves the map a tile the other way: `after`'s
    //  part of the screen, and where it was in `before`.
    size_t x = s.x;
    size_t y = s.y;
    size_t from_x = s.x;
    size_t from_y = s.y;
    size_t width = s.width;
    size_t height = s.height;
    switch (direction){
    case Direction::NORTH:
        y += s.tile_height;
        height -= s.tile_height;
        break;
    case Direction::SOUTH:
        from_y += s.tile_height;
        height -= s.tile_height;
        break;
    case Direction::WEST:
        x += s.tile_width;
        width -= s.tile_width;
        break;
    case Direction::EAST:
        from_x += s.tile_width;
        width -= s.tile_width;
        break;
    }
    const ImageViewRGB32 now = after.sub_image(x, y, width, height);
    const double still = ImageMatch::pixel_RMSD(before.sub_image(x, y, width, height), now);
    const double stepped = ImageMatch::pixel_RMSD(before.sub_image(from_x, from_y, width, height), now);
    return stepped < still * WATER_STEP_RATIO;
}

namespace{

//  Over this share of water on the player's tile, they are surfing.
constexpr double SURFING_WATER_SHARE = 0.25;

double water_on_player_tile(const ImageViewRGB32& screen, const ImageFloatBox& game){
    const ScreenTiles s = screen_tiles(screen, game);
    if (s.tile_width == 0 || s.tile_height == 0 || s.x + s.width > screen.width() || s.y + s.height > screen.height()){
        return 0;
    }
    //  The player stands on column 7 and row 4 of the 15 by 10 tiles.
    const size_t x0 = s.x + 7 * s.tile_width;
    const size_t y0 = s.y + 4 * s.tile_height;
    size_t water = 0;
    size_t total = 0;
    for (size_t y = y0; y < y0 + s.tile_height; y++){
        for (size_t x = x0; x < x0 + s.tile_width; x++){
            const Color pixel(screen.pixel(x, y));
            const int r = pixel.red();
            const int g = pixel.green();
            const int b = pixel.blue();
            total++;
            if (b > 150 && b > r + 60 && b > g + 20){
                water++;
            }
        }
    }
    return total == 0 ? 0 : (double)water / total;
}

} // namespace

bool surfing_on_screen(const ImageViewRGB32& screen, const ImageFloatBox& game){
    return water_on_player_tile(screen, game) > SURFING_WATER_SHARE;
}

namespace{

//  A counted tap's press, its release and the wait after it for the
//  screen to show the step.
struct TapTiming{
    Milliseconds press;
    Milliseconds release;
    Milliseconds settle;
};
//  On the Bicycle: a BIKE_TAP press released to a BIKE_TAP_PERIOD period,
//  then BIKE_TAP_SETTLE.
constexpr TapTiming BIKE_TAP_TIMING{BIKE_TAP, BIKE_TAP_PERIOD - BIKE_TAP, BIKE_TAP_SETTLE};

//  One tap of a leg counted out (count_bike_taps()), timed by `timing`,
//  watching `interrupts` throughout. A tap that didn't scroll the screen
//  is watched on for a late interrupt, which is cleared.
BikeTap console_bike_tap(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterrupts& interrupts, const TapTiming& timing
){
    context.wait_for_all_requests();
    const VideoSnapshot before = console.video().snapshot();
    std::vector<std::unique_ptr<VisualInferenceCallback>> watchers;
    std::vector<PeriodicInferenceCallback> callbacks;
    for (const auto& interrupt : interrupts){
        watchers.emplace_back(interrupt->make_watcher());
        callbacks.emplace_back(*watchers.back());
    }
    const int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            pbf_press_dpad(context, dpad(step.direction), timing.press, timing.release);
            pbf_wait(context, timing.settle);
            context.wait_for_all_requests();
        },
        callbacks
    );
    if (ret >= 0){
        console.log("Bike: " + interrupts[ret]->name() + " on a tap of " + to_string(step) + ".", COLOR_ORANGE);
        return BikeTap{.moved = false, .interrupt = (size_t)ret};
    }
    const VideoSnapshot after = console.video().snapshot();
    if (before && after && tap_scroll_change(before, after, GameSettings::instance().GAME_BOX) > WALL_TAP_MOVED_RMSD){
        return BikeTap{.moved = true};
    }
    const bool late = Pokemon::clear_late_interrupts(console, context, interrupts, AFTER_LEG_WATCH, MAX_INTERRUPTIONS_PER_LEG) > 0;
    return BikeTap{.moved = false, .interrupt = std::nullopt, .cleared_late = late};
}

//  An open leg on the Bicycle, a tap at a time (count_bike_taps()).
StepWalk walk_bike_taps(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterruptSet& interrupts, PlayerGender player
){
    StepWalk out{std::nullopt, current_time(), false, false};
    const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
    BikeTapCalls calls;
    calls.tap = [&](int){ return console_bike_tap(console, context, step, watched, BIKE_TAP_TIMING); };
    calls.clear = [&](size_t index){ watched[index]->clear(console, context); };
    calls.log = [&](const std::string& message){ console.log(message, COLOR_ORANGE); };
    //  On foot if the player's standing sprite reads.
    const auto remount = [&]{
        if (!read_facing(console, context, player)){
            return false;
        }
        console.log("Bike: a tap didn't move and the player is on foot.", COLOR_ORANGE);
        get_on_bike(console, context, player);
        return true;
    };
    const StepOutcome outcome = count_bike_taps_remounting(step, calls, remount);
    out.stopped = outcome.walked;
    out.blocked = outcome.blocked;
    return out;
}

//  A leg on foot, a step tap at a time (counts_step_taps()): STEP_TAP
//  pressed and STEP_TAP_RELEASE let go, past the step's lockout, then the
//  slide off a spinner waited out (step_tap_settle()).
StepWalk walk_step_taps(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterruptSet& interrupts, PlayerGender player
){
    StepWalk out{std::nullopt, current_time(), false, false};
    const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
    BikeTapCalls calls;
    calls.tap = [&](int tap){
        const TapTiming timing{STEP_TAP, STEP_TAP_RELEASE, step_tap_settle(step, tap)};
        return console_bike_tap(console, context, step, watched, timing);
    };
    calls.clear = [&](size_t index){ watched[index]->clear(console, context); };
    calls.log = [&](const std::string& message){ console.log(message, COLOR_ORANGE); };
    calls.facing = [&]{ return read_facing(console, context, player); };
    calls.tries = STEP_TAP_TRIES;
    const StepOutcome outcome = count_bike_taps(step, calls);
    out.stopped = outcome.walked;
    out.blocked = outcome.blocked;
    return out;
}

//  stop_tiles(), logged when the leg started in a dark cave.
WalkedTiles logged_stop_tiles(ConsoleHandle& console, const RouteStep& step, bool dark, WalkedTiles timed){
    if (dark){
        console.log(std::format(
            "Route: {} started in a dark cave: the stop's timing ({} to {} tiles in) isn't trusted.",
            to_string(step), timed.min, timed.max
        ), COLOR_ORANGE);
    }
    return stop_tiles(step, timed, dark);
}

//  Whether the screen shows a dark cave's overworld (dark_cave_screen()).
bool on_dark_cave(ConsoleHandle& console){
    const VideoSnapshot screen = console.video().snapshot();
    return screen && dark_cave_screen(*screen.frame);
}

//  Whether the map scrolled between the shot before a hold and now, once
//  the hold's first tile has had its time (`first_tile` after `hold_start`;
//  a one-tile open leg is let go half-way through its step).
HoldMovement measure_movement(
    ConsoleHandle& console, ProControllerContext& context,
    const VideoSnapshot& before, Direction direction, Milliseconds first_tile, WallClock hold_start
){
    const Milliseconds since_start = std::chrono::duration_cast<Milliseconds>(current_time() - hold_start);
    if (since_start < first_tile + TURN_SETTLE){
        context.wait_for(first_tile + TURN_SETTLE - since_start);
    }
    context.wait_for_all_requests();
    VideoSnapshot after = console.video().snapshot();
    for (int wait = 0; after && !detail::frame_shows_hold(after.timestamp, hold_start, first_tile) && wait < STALE_FRAME_WAITS; wait++){
        context.wait_for(STALE_FRAME_WAIT);
        after = console.video().snapshot();
    }
    if (!before || !after){
        return HoldMovement::UNMEASURED;
    }
    if (!detail::frame_shows_hold(after.timestamp, hold_start, first_tile)){
        console.log("Position check: the video shows no frame from after the hold's first tile: not measured.", COLOR_ORANGE);
        return HoldMovement::UNMEASURED;
    }
    const std::optional<double> change = hold_scroll_change(*before.frame, *after.frame, GameSettings::instance().GAME_BOX, direction);
    if (change){
        console.log(std::format("Position check: the screen changed by {:.1f} over the hold.", *change));
    }else{
        console.log(std::format("Position check: no edge of the screen would show a step {}, before or after the hold: not measured.", direction_name(direction)), COLOR_ORANGE);
    }
    return hold_movement(change);
}

//  One WALK step: turned first when it should, then its hold, and the warp
//  it ends on; for a hold not onto a warp, whether the screen moved.
StepWalk walk_hold_step(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const StepStart& from,
    const OverworldInterruptSet& interrupts, MoveSpeed speed, PlayerGender player
){
    StepWalk out{std::nullopt, current_time(), false, false};
    //  Turning first, with a tap, starts the hold from a standstill facing
    //  the right way: exact over 15 tiles, where a turn lost one.
    if (turns_first(step, speed, from.facing)){
        const bool walking = !step.surfing && step_speed(step, speed) != MoveSpeed::BIKE;
        turn_to(console, context, step.direction, player, walking ? TurnSprite::WALKING : TurnSprite::NOT_READ, from.at, UnreadTurn::then_for(step));
    }
    if (const std::string note = speed_note(step, speed); !note.empty()){
        console.log(note);
    }
    context.wait_for_all_requests();
    out.dark = on_dark_cave(console);
    const VideoSnapshot before = console.video().snapshot();
    out.hold_start = current_time();
    BlackScreenOverWatcher arrival(COLOR_RED);
    Pokemon::WatchAlong fade(arrival);
    const Pokemon::LegResult result = Pokemon::walk_leg(
        console, context, leg_mover(step, speed), step.wild ? interrupts.wild : interrupts.other,
        leg_spec(step, speed), MAX_INTERRUPTIONS_PER_LEG, warp_hold_watchers(step, fade)
    );
    if (result.stopped){
        out.stopped = logged_stop_tiles(console, step, out.dark, walked_near(step, result.tiles, tile_time(step, speed)));
        return out;
    }
    if (!step.warps){
        out.movement = measure_movement(console, context, before, step.direction, tile_time(step, speed), out.hold_start);
        return out;
    }
    wait_out_warp(console, context, step, fade, before);
    static_assert(AFTER_WARP_BATTLE_WATCH >= AFTER_WARP);
    if (run_from_battle_after_warp(console, context)){
        console.log("Route: a wild battle, not the warp to " + step.end.map + "; the player is on " + to_string(step) + " before the warp.", COLOR_ORANGE);
        out.stopped = walked_before_warp(step);
    }
    return out;
}

//  One step of a route, from `from`.
StepWalk walk_step(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const StepStart& from,
    const OverworldInterruptSet& interrupts, MoveSpeed speed, PlayerGender player
){
    StepWalk out{std::nullopt, current_time(), false, false};
    switch (step.action){
    case RouteAction::WALK:
        return walk_hold_step(console, context, step, from, interrupts, speed, player);
    case RouteAction::SURF:
        start_surf(console, context, step.direction, from.facing == step.direction);
        break;
    case RouteAction::FACE:
        turn_to(console, context, step.direction, player, TurnSprite::WALKING, from.at);
        break;
    case RouteAction::OPEN_GATE:
        turn_to(console, context, step.direction, player, TurnSprite::WALKING, from.at);
        open_gate(console, context, step.direction, true);
        break;
    case RouteAction::PRESS_SWITCH:
        turn_to(console, context, step.direction, player, TurnSprite::WALKING, from.at);
        press_switch(console, context, step.direction, true);
        break;
    }
    return out;
}

//  An open leg's screens, recorded with the interrupts' observer from
//  before its hold until the watch after it ends, alongside the walker's
//  own watchers. Records nothing for other steps, or with no observer.
class LegRecorder{
public:
    LegRecorder(
        ConsoleHandle& console, ProControllerContext& context,
        const OverworldInterruptSet& interrupts, const RouteStep& step
    )
        : m_observer(records(interrupts, step) ? interrupts.make_observer() : nullptr)
        , m_watcher(m_observer ? std::make_unique<ScreenEventWatcher>(*m_observer) : nullptr)
        , m_session(context, console, callbacks())
    {}

    //  Where what was seen after `step`'s hold, from `hold_start`, stopped
    //  the player (place_late_stop()); nothing if not recorded or not placed.
    std::optional<WalkedTiles> place(
        ConsoleHandle& console, const RouteStep& step, Milliseconds tile_time, WallClock hold_start
    ) const{
        if (!m_watcher){
            return std::nullopt;
        }
        const LateStop stop = place_late_stop(step, tile_time, m_watcher->log(), hold_start, current_time());
        console.log(late_stop_line(step, stop, hold_start), COLOR_ORANGE);
        return stop.walked;
    }

private:
    static bool records(const OverworldInterruptSet& interrupts, const RouteStep& step){
        const bool open_leg = step.action == RouteAction::WALK && !step.to_wall && !step.warps;
        return open_leg && interrupts.make_observer;
    }
    std::vector<PeriodicInferenceCallback> callbacks() const{
        if (!m_watcher){
            return {};
        }
        return {*m_watcher};
    }

    std::unique_ptr<ScreenEventObserver> m_observer;
    std::unique_ptr<ScreenEventWatcher> m_watcher;
    //  With no watcher it runs nothing.
    InferenceSession m_session;
};

//  walk_step(), then a watch for what its last step set off. Not DONE if
//  the player may have stopped short of the step's end: an open leg
//  stopped, or something seen after the hold, which may have stopped it
//  before the end (after a warp, it was checked for already). A battle
//  on its way that the watch missed counts as seen (clear_unseen_battle()),
//  also after a leg the maps see no wild tiles on. One on its way before
//  the step starts was set off before it: the step isn't walked, and is
//  BATTLE_BEFORE (clear_battle_before()).
StepOutcome walk_and_watch(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const StepStart& from,
    const OverworldInterruptSet& interrupts, MoveSpeed speed, PlayerGender player
){
    if (clear_battle_before(console, context, interrupts.wild, step) > 0){
        return StepOutcome{.end = StepEnd::BATTLE_BEFORE, .walked = std::nullopt};
    }
    clear_open_message(console, context, step.wild ? interrupts.wild : interrupts.other, step);
    check_overworld(console, context, step);
    const LegRecorder recorder(console, context, interrupts, step);
    StepWalk walked;
    if (counts_step_taps(step)){
        walked = walk_step_taps(console, context, step, interrupts, player);
    }else if (counts_bike_taps(step, speed)){
        walked = walk_bike_taps(console, context, step, interrupts, player);
    }else{
        walked = walk_step(console, context, step, from, interrupts, speed, player);
    }
    if (walked.stopped){
        return StepOutcome{.end = StepEnd::STOPPED, .walked = walked.stopped, .blocked = walked.blocked};
    }
    if (step.action == RouteAction::FACE || step.action == RouteAction::OPEN_GATE || step.action == RouteAction::PRESS_SWITCH){
        return StepOutcome{};
    }
    const HoldMovement movement = walked.movement;
    const Milliseconds watch = step.wild ? AFTER_WILD_LEG_WATCH : AFTER_LEG_WATCH;
    const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
    size_t late = Pokemon::clear_late_interrupts(console, context, watched, watch, MAX_INTERRUPTIONS_PER_LEG);
    if (late == 0){
        late = clear_unseen_battle(console, context, interrupts.wild, step);
    }
    //  After a warp it was checked for already.
    const bool done = late == 0 || step.warps || step.action != RouteAction::WALK;
    if (done){
        return StepOutcome{.end = StepEnd::DONE, .walked = std::nullopt, .movement = movement};
    }
    std::optional<WalkedTiles> placed = recorder.place(console, step, tile_time(step, speed), walked.hold_start);
    if (walked.dark && step.action == RouteAction::WALK && !step.to_wall){
        placed = logged_stop_tiles(console, step, true, placed.value_or(whole_leg(step)));
    }
    return StepOutcome{.end = StepEnd::LATE, .walked = placed};
}

//  Holds on along `step`, stopped part-way off the grass or by a wild
//  battle where no trainer may see the player, until its warp's fade. Off
//  the grass only a message stops a leg (a trainer who saw the player, a
//  Repel wearing off), and the player then stands on the leg's line, as
//  after a wild battle, so holding on reaches the warp however far they
//  got (console B, 2026-11-01: Camper Liam stopped "south 9 (warp)" out of
//  Pewter Gym). After a trainer the player faces them, so this turns first.
void hold_to_warp(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterrupts& interrupts, MoveSpeed speed
){
    const Pokemon::LegMover<ProControllerContext> mover = leg_mover(step, speed);
    for (size_t count = 0; count <= MAX_INTERRUPTIONS_PER_LEG; count++){
        BlackScreenWatcher fade(COLOR_RED);
        std::vector<std::unique_ptr<VisualInferenceCallback>> watchers;
        std::vector<PeriodicInferenceCallback> callbacks{fade};
        for (const auto& interrupt : interrupts){
            watchers.emplace_back(interrupt->make_watcher());
            callbacks.emplace_back(*watchers.back());
        }
        context.wait_for_all_requests();
        const VideoSnapshot before = console.video().snapshot();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [&](ProControllerContext& context){
                pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TURN_SETTLE);
                mover(context, step.direction, tile_time(step, speed) * (step.tiles + 2));
            },
            callbacks
        );
        if (ret == 0){
            wait_out_warp(console, context, step, before);
            check_warp_was_no_battle(console, context, step);
            return;
        }
        if (ret < 0){
            break;
        }
        console.log("Route: " + interrupts[ret - 1]->name() + " on the way to the warp.", COLOR_ORANGE);
        interrupts[ret - 1]->clear(console, context);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "walk_overworld_route(): no fade holding on toward the warp to " + step.end.map + ".",
        console
    );
}

//  After `step`, which crosses the sight of a trainer not beaten
//  (`crossing`): a trainer who sees the player stops them, a "!" shows
//  over the trainer, the trainer walks up, then speaks. Looks for
//  trainer_first_look() from the hold's end; if anything shows, watches on
//  for the trainer to walk up and speak (trainer_full_watch()), and
//  clears what shows.
StepEnd watch_for_trainer(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const TrainerCrossing& crossing, const OverworldInterruptSet& interrupts
){
    const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
    const Milliseconds look = trainer_first_look(crossing);
    const size_t first = Pokemon::clear_late_interrupts(console, context, watched, look, MAX_INTERRUPTIONS_PER_LEG);
    if (first == 0){
        console.log(std::format("Route: {} crosses a trainer's sight; watch {} ms: nothing.", to_string(step), look.count()));
        return StepEnd::DONE;
    }
    const Milliseconds watch = trainer_full_watch(crossing.approach_tiles);
    console.log(std::format("Route: {} crosses a trainer's sight; watch {} ms: something showed; watching {} ms more.", to_string(step), look.count(), watch.count()));
    Pokemon::clear_late_interrupts(console, context, watched, watch, MAX_INTERRUPTIONS_PER_LEG);
    return StepEnd::LATE;
}

//  TODO(forest drift, 2026-11-01): diagnostic; remove or turn into a recovery once the cause is known.
//  Taps once toward the wall `step` ended at (the player faces it, so a tap
//  steps if it can) and says whether the map scrolled: whether the player
//  moved instead of bumping into the wall.
bool moved_past_wall(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    context.wait_for_all_requests();
    VideoSnapshot before = console.video().snapshot();
    pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TILE_TIME + TURN_SETTLE);
    context.wait_for_all_requests();
    VideoSnapshot after = console.video().snapshot();
    if (!before || !after){
        return false;
    }
    const ImageFloatBox game = GameSettings::instance().GAME_BOX;
    const double change = tap_scroll_change(before, after, game);
    if (step.surfing){
        //  The water's animation changes the edge bands as much as a step.
        const bool moved = tap_moved_on_water(before, after, game, step.direction);
        console.log(std::format(
            "Position check: a tap toward the wall of {} on the water changed the screen by {:.1f}; the map {}.",
            to_string(step), change, moved ? "moved a tile" : "stayed put"
        ));
        return moved;
    }
    console.log(std::format("Position check: a tap toward the wall of {} changed the screen by {:.1f}.", to_string(step), change));
    return change > WALL_TAP_MOVED_RMSD;
}

//  Whether the player's walking sprite shows, read up to LANDED_READS
//  times LANDED_REREAD apart: the hop off the water and the steps after
//  it may still be on. Surfing, the sprite isn't read (PlayerFacingReader).
//  A sprite that never shows may be hidden: drawn behind a roof or a tree
//  (Cinnabar's beach, console B, 2026-11-13 09:46, 12.03 land_beach, only
//  the head showing). Then the player's own tile tells: water around them
//  while surfing, none on land (water_on_player_tile()).
constexpr size_t LANDED_READS = 4;
constexpr Milliseconds LANDED_REREAD(150);
bool stands_on_foot(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    VideoSnapshot screen;
    for (size_t read = 1; read <= LANDED_READS; read++){
        if (read > 1){
            pbf_wait(context, LANDED_REREAD);
        }
        context.wait_for_all_requests();
        screen = console.video().snapshot();
        if (screen && read_player_facing(screen, player)){
            return true;
        }
    }
    if (!screen){
        console.log("Route: no frame to tell whether the player landed.", COLOR_ORANGE);
        return false;
    }
    if (!surfing_on_screen(screen, GameSettings::instance().GAME_BOX)){
        console.log("Route: the player's walking sprite doesn't show, but no water shows on their tile: landed, drawn behind something.", COLOR_ORANGE);
        return true;
    }
    console.log("Route: the player's walking sprite doesn't show, and water shows on their tile: still surfing.", COLOR_ORANGE);
    return false;
}

} // namespace


namespace{
std::string file_part(const std::string& text){
    std::string out;
    for (char c : text){
        const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (keep){
            out += c;
        }else if (c == '/'){
            out += '-';
        }else if (!out.empty() && out.back() != '_'){
            out += '_';
        }
    }
    return out;
}
} // namespace

std::string save_route_screenshot(ConsoleHandle& console, const std::string& label, const std::string& text){
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return "";
    }
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
    Filesystem::create_directories(Filesystem::Path(folder));
    const std::string path = folder + now_to_filestring() + "-" + file_part(label) + "-" + file_part(text) + ".png";
    return screen->save(path) ? path : "";
}

namespace{

//  The calls of a console route that don't walk: log, fail, the wall
//  check, the trainer watch and the leg-end screenshot.
RouteCalls console_route_calls(
    ConsoleHandle& console, ProControllerContext& context,
    const RoutePlanning& planning, const OverworldInterruptSet& interrupts
){
    RouteCalls calls;
    calls.log = [&console](const std::string& message){ console.log(message, COLOR_ORANGE); };
    calls.fail = [&console](const std::string& message){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, console);
    };
    calls.moved_past_wall = [&console, &context](const RouteStep& step){
        return moved_past_wall(console, context, step);
    };
    calls.watch_for_trainer = [&console, &context, &interrupts](const RouteStep& step, const TrainerCrossing& crossing){
        return watch_for_trainer(console, context, step, crossing, interrupts);
    };
    calls.leg_ended = [&console, &planning](const std::string& leg_text){
        const bool stepped = planning.pause != nullptr && !planning.pause->step_label().empty();
        return save_route_screenshot(console, stepped ? planning.pause->step_label() : planning.label, leg_text);
    };
    return calls;
}

} // namespace

bool detail::frame_shows_hold(WallClock frame_time, WallClock hold_start, Milliseconds first_tile){
    return frame_time >= hold_start + first_tile;
}

std::optional<double> hold_scroll_change(const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction){
    //  A scroll moves every band that would show it; a town's banner (top
    //  left) or water only some.
    std::optional<double> least;
    for (const ImageFloatBox& band : edge_bands(game)){
        if (same_one_tile_over(before, band, game, direction) && same_one_tile_over(after, band, game, direction)){
            continue;
        }
        const double change = ImageMatch::pixel_RMSD(extract_box_reference(before, band), extract_box_reference(after, band));
        least = least ? std::min(*least, change) : change;
    }
    return least;
}

HoldMovement hold_movement(std::optional<double> change){
    if (!change){
        return HoldMovement::UNMEASURED;
    }
    return *change > HOLD_UNMOVED_RMSD ? HoldMovement::MOVED : HoldMovement::NONE;
}

void walk_overworld_route_with_recovery(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    const RoutePlanning& planning,
    PlayerGender player,
    std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts,
    MoveSpeed speed
){
    if (route.empty()){
        return;
    }
    if (!facing){
        facing = read_facing(console, context, player);
    }
    //  A door takes the player off the Bicycle; the next leg where it may
    //  be ridden gets on again with SELECT. A battle leaves them on.
    bool riding = speed == MoveSpeed::BIKE;
    //  Where the next step starts, while known: the planned start, then the
    //  end of each step that ended as planned. Unknown after a stop.
    std::optional<MapPosition> at = planning.start;
    RouteCalls calls = console_route_calls(console, context, planning, interrupts);
    calls.walk = [&](const RouteStep& step, std::optional<Direction> from_facing){
        if (gets_on_bike(step, speed, riding)){
            get_on_bike(console, context, player);
            riding = true;
            from_facing = std::nullopt;
        }
        const StepOutcome outcome = walk_and_watch(console, context, step, StepStart{from_facing, at}, interrupts, riding ? speed : on_foot(speed), player);
        at = outcome.end == StepEnd::DONE ? std::optional<MapPosition>(step.end) : std::nullopt;
        if (riding && step.warps && !warp_keeps_bike(step.end.map)){
            console.log("Bike: through the warp to " + step.end.map + ", off the Bicycle.");
            riding = false;
        }
        return outcome;
    };
    calls.hold_to_warp = [&](const RouteStep& step){
        hold_to_warp(console, context, step, step.wild ? interrupts.wild : interrupts.other, riding ? speed : on_foot(speed));
        riding = riding && warp_keeps_bike(step.end.map);
        at = step.end;
    };
    calls.landed_on = [&](const MapPosition&){ return stands_on_foot(console, context, player); };
    run_route(route, facing, &planning, calls);
    context.wait_for_all_requests();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  Overworld Recovery Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include <optional>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Async/InferenceSession.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_OverworldLateBattle.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"

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
};

//  Where a stop on `step` leaves the player, from `timed`, the timing's
//  tiles: anywhere on the leg if the leg started in a dark cave. There the
//  timing has no basis: the screen is black before any battle's wipe, and
//  a battle at the end of "east 12" on Rock Tunnel B1F was placed 0 to 3
//  tiles in (console B, 2026-11-09, 00.28); the player was found again at
//  a wall 5 tiles off.
WalkedTiles stop_tiles(ConsoleHandle& console, const RouteStep& step, bool dark, WalkedTiles timed){
    if (!dark){
        return timed;
    }
    const WalkedTiles out = whole_leg(step);
    console.log(std::format(
        "Route: {} started in a dark cave: the stop's timing ({} to {} tiles in) isn't trusted; {} to {} tiles in.",
        to_string(step), timed.min, timed.max, out.min, out.max
    ), COLOR_ORANGE);
    return out;
}

//  One step of a route, for a player facing `facing`.
StepWalk walk_step(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts, MoveSpeed speed
){
    StepWalk out{std::nullopt, current_time(), false};
    switch (step.action){
    case RouteAction::WALK:{
        //  Turning first, with a tap, makes the hold start from a standstill
        //  facing the right way: holding through a turn lost a tile on the
        //  console, while holds from a standstill were exact over 15 tiles.
        if (facing != step.direction){
            turn_to(console, context, step.direction);
        }
        if (const std::string note = speed_note(step, speed); !note.empty()){
            console.log(note);
        }
        context.wait_for_all_requests();
        if (VideoSnapshot before = console.video().snapshot()){
            out.dark = dark_cave_screen(*before.frame);
        }
        out.hold_start = current_time();
        BlackScreenOverWatcher arrival(COLOR_RED);
        Pokemon::WatchAlong fade(arrival);
        const Pokemon::LegResult result = Pokemon::walk_leg(
            console, context, leg_mover(step, speed), step.wild ? interrupts.wild : interrupts.other,
            leg_spec(step, speed), MAX_INTERRUPTIONS_PER_LEG, warp_hold_watchers(step, fade)
        );
        if (result.stopped){
            out.stopped = stop_tiles(console, step, out.dark, walked_near(step, result.tiles, tile_time(step, speed)));
            return out;
        }
        if (step.warps){
            wait_out_warp(console, context, step, fade);
            static_assert(AFTER_WARP_BATTLE_WATCH >= AFTER_WARP);
            if (run_from_battle_after_warp(console, context)){
                console.log("Route: a wild battle, not the warp to " + step.end.map + "; the player is on " + to_string(step) + " before the warp.", COLOR_ORANGE);
                out.stopped = walked_before_warp(step);
                return out;
            }
        }
        break;
    }
    case RouteAction::SURF:
        start_surf(console, context, step.direction, facing == step.direction);
        break;
    case RouteAction::FACE:
        pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
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
        , m_watcher(m_observer ? std::make_unique<Pokemon::ScreenEventWatcher>(*m_observer) : nullptr)
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

    std::unique_ptr<Pokemon::ScreenEventObserver> m_observer;
    std::unique_ptr<Pokemon::ScreenEventWatcher> m_watcher;
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
    const RouteStep& step, std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts, MoveSpeed speed
){
    if (clear_battle_before(console, context, interrupts.wild, step) > 0){
        return StepOutcome{.end = StepEnd::BATTLE_BEFORE, .walked = std::nullopt};
    }
    clear_open_message(console, context, step.wild ? interrupts.wild : interrupts.other, step);
    check_overworld(console, context, step);
    const LegRecorder recorder(console, context, interrupts, step);
    const StepWalk walked = walk_step(console, context, step, facing, interrupts, speed);
    if (walked.stopped){
        return StepOutcome{.end = StepEnd::STOPPED, .walked = walked.stopped};
    }
    if (step.action == RouteAction::FACE){
        return StepOutcome{};
    }
    const Milliseconds watch = step.wild ? AFTER_WILD_LEG_WATCH : AFTER_LEG_WATCH;
    const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
    size_t late = Pokemon::clear_late_interrupts(console, context, watched, watch, MAX_INTERRUPTIONS_PER_LEG);
    if (late == 0){
        late = clear_unseen_battle(console, context, interrupts.wild, step);
    }
    //  After a warp it was checked for already.
    const bool done = late == 0 || step.warps || step.action != RouteAction::WALK;
    if (done){
        return StepOutcome{};
    }
    std::optional<WalkedTiles> placed = recorder.place(console, step, tile_time(step, speed), walked.hold_start);
    if (walked.dark && step.action == RouteAction::WALK && !step.to_wall){
        placed = stop_tiles(console, step, true, placed.value_or(whole_leg(step)));
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
        const int ret = run_until<ProControllerContext>(
            console, context,
            [&](ProControllerContext& context){
                pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TURN_SETTLE);
                mover(context, step.direction, tile_time(step, speed) * (step.tiles + 2));
            },
            callbacks
        );
        if (ret == 0){
            wait_out_warp(console, context, step);
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

//  A trainer who sees the player stops them, a "!" shows over the trainer,
//  the trainer walks up, then speaks. The leg's hold may run on past the
//  tile the player was stopped on, so the watch counts from the hold's end,
//  with room to spare: on Route 3 (console B, 2026-11-02) Lass Robin's
//  message showed after the 500ms watch after the leg had ended. Not
//  measured on the console yet.
constexpr Milliseconds TRAINER_NOTICE(1500);
constexpr Milliseconds TRAINER_SPEAKS(1000);

//  After `step`, which crosses a trainer's sight: watches for the trainer
//  to walk up `approach_tiles` tiles and speak, and clears what shows.
StepEnd watch_for_trainer(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, int approach_tiles, const OverworldInterruptSet& interrupts
){
    const Milliseconds watch = TRAINER_NOTICE + TILE_TIME * approach_tiles + TRAINER_SPEAKS;
    console.log(std::format("Route: {} crosses a trainer's sight; watching {} ms for them.", to_string(step), watch.count()));
    const size_t seen = Pokemon::clear_late_interrupts(
        console, context, step.wild ? interrupts.wild : interrupts.other, watch, MAX_INTERRUPTIONS_PER_LEG
    );
    return seen == 0 ? StepEnd::DONE : StepEnd::LATE;
}

//  TODO(forest drift, 2026-11-01): diagnostic; remove or turn into a recovery once the cause is known.
//  How much the game screen's top quarter, clear of the player in the
//  middle, may change when a tap toward a wall only bumps the player (the
//  player's own animation, an animated tile). A step scrolls the whole map
//  by a tile. Not yet measured on the console: a guess on the safe side.
constexpr double WALL_TAP_MOVED_RMSD = 20.0;

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
    const ImageFloatBox band(game.x, game.y, game.width, game.height * 0.25);
    const double change = ImageMatch::pixel_RMSD(extract_box_reference(before, band), extract_box_reference(after, band));
    console.log(std::format("Position check: a tap toward the wall of {} changed the screen by {:.1f}.", to_string(step), change));
    return change > WALL_TAP_MOVED_RMSD;
}

} // namespace


void walk_overworld_route_with_recovery(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    const RoutePlanning& planning,
    std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts,
    MoveSpeed speed
){
    if (route.empty()){
        return;
    }
    if (!facing){
        facing = read_facing(console, context);
    }
    RouteCalls calls;
    calls.walk = [&](const RouteStep& step, std::optional<Direction> from_facing){
        return walk_and_watch(console, context, step, from_facing, interrupts, speed);
    };
    calls.hold_to_warp = [&](const RouteStep& step){
        hold_to_warp(console, context, step, step.wild ? interrupts.wild : interrupts.other, speed);
    };
    calls.log = [&](const std::string& message){ console.log(message, COLOR_ORANGE); };
    calls.fail = [&](const std::string& message){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, console);
    };
    calls.moved_past_wall = [&](const RouteStep& step){
        return moved_past_wall(console, context, step);
    };
    calls.watch_for_trainer = [&](const RouteStep& step, int approach_tiles){
        return watch_for_trainer(console, context, step, approach_tiles, interrupts);
    };
    run_route(route, facing, &planning, calls);
    context.wait_for_all_requests();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

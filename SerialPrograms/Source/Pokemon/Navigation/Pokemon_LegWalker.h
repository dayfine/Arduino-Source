/*  Leg Walker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks one leg of a plan: holds a direction for the leg's tiles while
 *  watching for whatever can stop the player part-way (a wild battle, a
 *  message, a trainer). Each of those is a LegInterrupt the game supplies.
 *  When one shows, the hold stops, the game clears it, and the rest of the
 *  leg is walked: a leg to a wall is simply held again, since the wall stops
 *  the player on its tile. An open leg can't be checked like that, so where
 *  an interrupt is likely it is walked a tile at a time, each step watched
 *  long enough to see what it set off; elsewhere how far the player got is
 *  worked out from when the interrupt was seen.
 *
 *  How a direction is held (D-pad or stick) and how long a tile takes are
 *  the game's too, through LegMover. Templated on the console and the
 *  controller context, like run_until(), so this stays free of any one
 *  console's types.
 *
 */

#ifndef PokemonAutomation_Pokemon_LegWalker_H
#define PokemonAutomation_Pokemon_LegWalker_H

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Something that stops the player part-way through a leg.
template <typename Console, typename Context>
class LegInterrupt{
public:
    virtual ~LegInterrupt() = default;

    virtual std::string name() const = 0;

    //  A new watcher for the screen the interruption brings up.
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const = 0;

    //  How long after the step that set it off the watcher fires. The games
    //  only interrupt between steps, so from this the walker works out how
    //  many tiles of an open leg were walked.
    virtual Milliseconds detection_delay() const = 0;

    //  Clears it, leaving the player standing on the overworld, on the same
    //  tile and facing the same way.
    virtual void clear(Console& console, Context& context) const = 0;
};

template <typename Console, typename Context>
using LegInterrupts = std::vector<std::unique_ptr<LegInterrupt<Console, Context>>>;


//  How the game moves the player.
template <typename Context>
class LegMover{
public:
    virtual ~LegMover() = default;
    //  Holds `direction` for `duration`, then lets go.
    virtual void hold(Context& context, Direction direction, Milliseconds duration) const = 0;
};


//  A straight leg, for a player standing still and facing its way.
struct LegSpec{
    Direction direction = Direction::NORTH;
    int tiles = 0;
    //  A wall stops the player after the last tile.
    bool to_wall = false;
    //  How long one tile takes, for this leg's direction and way of moving.
    Milliseconds tile_time{0};
    //  If set, an open leg is walked a tile at a time: one short press per
    //  step, watched for this long from the press before the next. It must be
    //  longer than the interrupts' detection delays. See
    //  steps_back_to_interrupt().
    Milliseconds step_window{0};
    //  The last step lands on a warp. Walked a tile at a time, that step
    //  isn't watched: the warp's fade looks like a battle's flash.
    bool warps = false;
    //  Walked a tile at a time, the last watched step is watched this much
    //  longer, for an interrupt whose first sign was missed and that shows
    //  only later: it is then placed before the leg ends, not after.
    Milliseconds settle{0};
    //  An open leg stops at its first interrupt, once cleared, instead of
    //  walking on from an estimate; the caller finds the player again.
    bool stop_on_interrupt = false;
};
std::string to_string(const LegSpec& leg);

struct LegResult{
    //  Tiles walked, which is `tiles` unless the leg gave up.
    int tiles = 0;
    //  Whether `tiles` is certain: an interrupted open leg is an estimate.
    bool exact = true;
    size_t interruptions = 0;
    //  The leg stopped at an interrupt (LegSpec::stop_on_interrupt): the
    //  player is somewhere along it, and `tiles` is not known.
    bool stopped = false;
};


//  How long to hold for `tiles` tiles: an open leg lets go half a tile before
//  the last step would end, so the step under way finishes and no other
//  starts; a leg to a wall is held two tiles longer than it takes.
Milliseconds leg_hold(const LegSpec& leg, int tiles);

//  How many tiles an open leg had walked when an interrupt was seen
//  `elapsed` into its hold, given the interrupt's detection delay. Steps end
//  a whole number of tiles into the hold, so this rounds to the nearest.
int tiles_before_interrupt(const LegSpec& leg, Milliseconds elapsed, Milliseconds detection_delay, int remaining);

//  For a leg walked a tile at a time: how many steps before the one just
//  pressed set off an interrupt seen `elapsed` after that press. A step's
//  interrupt shows after the step lands, a tile into its window, and so
//  within its window when the delay is shorter than the window, or else
//  before the next press lands, which it then swallows. So one seen less than
//  a tile after a press is the previous step's. This needs no measured delay,
//  only one shorter than the window. An interrupt slower than that is placed
//  on the step that landed nearest `detection_delay` before it was seen,
//  which is only as good as the delay.
int steps_back_to_interrupt(const LegSpec& leg, Milliseconds elapsed, Milliseconds detection_delay);


//  What one hold reported: nothing if it ran out, or which interrupt showed
//  and how long into the hold.
struct HoldOutcome{
    size_t interrupt = 0;
    Milliseconds elapsed{0};
};

//  The leg's bookkeeping, with the console side passed in, so it can be
//  tested with scripted outcomes. `hold_watching` holds for `hold`, watches
//  until at least `window` after the hold started, and reports the first
//  interrupt seen; `hold` only holds; `clear` clears interrupt i;
//  `detection_delay` is interrupt i's delay; `log` reports progress; `fail`
//  throws, and is called after more than `max_interruptions`.
struct LegCalls{
    std::function<std::optional<HoldOutcome>(Milliseconds hold, Milliseconds window)> hold_watching;
    std::function<void(Milliseconds hold)> hold;
    std::function<void(size_t interrupt)> clear;
    std::function<Milliseconds(size_t interrupt)> detection_delay;
    std::function<std::string(size_t interrupt)> name;
    std::function<void(const std::string& message)> log;
    std::function<void(const std::string& message)> fail;
};
LegResult walk_leg_with(const LegSpec& leg, const LegCalls& calls, size_t max_interruptions);


//  Walks `leg`. Throws OperationFailedException if it is interrupted more
//  than `max_interruptions` times.
template <typename Console, typename Context>
LegResult walk_leg(
    Console& console, Context& context,
    const LegMover<Context>& mover,
    const LegInterrupts<Console, Context>& interrupts,
    const LegSpec& leg,
    size_t max_interruptions = 5
){
    auto hold_watching = [&](Milliseconds hold, Milliseconds window) -> std::optional<HoldOutcome>{
        std::vector<std::unique_ptr<VisualInferenceCallback>> watchers;
        std::vector<PeriodicInferenceCallback> callbacks;
        for (const auto& interrupt : interrupts){
            watchers.emplace_back(interrupt->make_watcher());
            callbacks.emplace_back(*watchers.back());
        }
        context.wait_for_all_requests();
        WallClock start = current_time();
        const int ret = run_until<Context>(
            console, context,
            [&](Context& context){
                start = current_time();
                mover.hold(context, leg.direction, hold);
                context.wait_for_all_requests();
                context.wait_until(start + window);
            },
            callbacks
        );
        if (ret < 0){
            return std::nullopt;
        }
        return HoldOutcome{(size_t)ret, std::chrono::duration_cast<Milliseconds>(current_time() - start)};
    };
    LegCalls calls;
    calls.hold_watching = hold_watching;
    calls.hold = [&](Milliseconds hold){
        mover.hold(context, leg.direction, hold);
        context.wait_for_all_requests();
    };
    calls.clear = [&](size_t index){ interrupts[index]->clear(console, context); };
    calls.detection_delay = [&](size_t index){ return interrupts[index]->detection_delay(); };
    calls.name = [&](size_t index){ return interrupts[index]->name(); };
    calls.log = [&](const std::string& message){ console.log("Leg: " + message, COLOR_ORANGE); };
    calls.fail = [&](const std::string& message){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, console);
    };
    return walk_leg_with(leg, calls, max_interruptions);
}


//  After a leg: waits `watch` for an interrupt the last step set off, which
//  shows after the hold has ended, and clears it. Returns how many it
//  cleared. Throws OperationFailedException after `max_interruptions` in a
//  row.
template <typename Console, typename Context>
size_t clear_late_interrupts(
    Console& console, Context& context,
    const LegInterrupts<Console, Context>& interrupts,
    Milliseconds watch,
    size_t max_interruptions = 5
){
    for (size_t count = 0; ; count++){
        std::vector<std::unique_ptr<VisualInferenceCallback>> watchers;
        std::vector<PeriodicInferenceCallback> callbacks;
        for (const auto& interrupt : interrupts){
            watchers.emplace_back(interrupt->make_watcher());
            callbacks.emplace_back(*watchers.back());
        }
        context.wait_for_all_requests();
        const int ret = wait_until(console, context, watch, callbacks);
        if (ret < 0){
            return count;
        }
        if (count >= max_interruptions){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format("Interrupted {} times in a row after a leg.", count),
                console
            );
        }
        console.log("Leg: " + interrupts[ret]->name() + " at the end of the leg.", COLOR_ORANGE);
        interrupts[ret]->clear(console, context);
    }
}




}
}
#endif

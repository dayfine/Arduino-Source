/*  Leg Walker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks one leg of a plan: holds a direction for the leg's tiles while
 *  watching for whatever can stop the player part-way (a wild battle, a
 *  message, a trainer). Each of those is a LegInterrupt the game supplies.
 *  When one shows, the hold stops, the game clears it, and the rest of the
 *  leg is walked: a leg to a wall is simply held again, since the wall stops
 *  the player on its tile; for an open leg, how far the player got is worked
 *  out from when the interrupt was seen.
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
#include <atomic>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Pokemon_Grid.h"
#include "Pokemon_LegTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What one hold reported: nothing if it ran out, or which interrupt showed
//  and how long into the hold.
struct HoldOutcome{
    size_t interrupt = 0;
    Milliseconds elapsed{0};
};

//  The leg's bookkeeping, with the console side passed in, so it can be
//  tested with scripted outcomes. `hold_watching` holds for the duration and
//  reports the first interrupt seen; `clear` clears interrupt i;
//  `detection_delay` is interrupt i's delay; `log` reports progress; `fail`
//  throws, and is called after more than `max_interruptions`.
struct LegCalls{
    std::function<std::optional<HoldOutcome>(Milliseconds hold)> hold_watching;
    std::function<void(size_t interrupt)> clear;
    std::function<Milliseconds(size_t interrupt)> detection_delay;
    std::function<std::string(size_t interrupt)> name;
    std::function<void(const std::string& message)> log;
    std::function<void(const std::string& message)> fail;
};
// Low-level leg execution using provided callback interface.
LegResult walk_leg_with(const LegSpec& leg, const LegCalls& calls, size_t max_interruptions);


//  Shows a hold's frames to `watcher` without letting it stop the hold, and
//  keeps whether it fired. A warp's fade can be over before the hold onto
//  the warp ends, so a wait for it that starts after the hold sees nothing:
//  the walk watches the hold with this, then waits on with the same watcher
//  if it has not fired (FRLG 00.27, console B, 2026-11-09 01:41, the warp to
//  CeruleanCity: the watch started 1.3 s into the hold and missed the fade).
class WatchAlong : public VisualInferenceCallback{
public:
    explicit WatchAlong(VisualInferenceCallback& watcher)
        : VisualInferenceCallback("WatchAlong")
        , m_watcher(watcher)
    {}

    VisualInferenceCallback& watcher() const{
        return m_watcher;
    }
    bool fired() const{
        return m_fired.load(std::memory_order_acquire);
    }

    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_watcher.make_overlays(items);
    }
    using VisualInferenceCallback::process_frame;
    virtual bool process_frame(const VideoSnapshot& frame) override{
        if (!fired() && m_watcher.process_frame(frame)){
            m_fired.store(true, std::memory_order_release);
        }
        return false;
    }

private:
    VisualInferenceCallback& m_watcher;
    std::atomic<bool> m_fired{false};
};


//  Walks `leg`. Throws OperationFailedException if it is interrupted more
//  than `max_interruptions` times. `hold_watchers` see every hold's frames
//  too, after the interrupts' watchers, but stop nothing: a WatchAlong.
template <typename Console, typename Context>
LegResult walk_leg(
    Console& console, Context& context,
    const std::type_identity_t<LegMover<Context>>& mover,
    const LegInterrupts<Console, Context>& interrupts,
    const LegSpec& leg,
    size_t max_interruptions = 5,
    const std::vector<WatchAlong*>& hold_watchers = std::vector<WatchAlong*>()
);

template <typename Console, typename Context>
LegResult walk_leg(
    Console& console, Context& context,
    const std::type_identity_t<LegMover<Context>>& mover,
    const LegInterrupts<Console, Context>& interrupts,
    const LegSpec& leg,
    size_t max_interruptions,
    const std::vector<WatchAlong*>& hold_watchers
){
    auto hold_watching = [&](Milliseconds hold) -> std::optional<HoldOutcome>{
        std::vector<std::unique_ptr<VisualInferenceCallback>> watchers;
        std::vector<PeriodicInferenceCallback> callbacks;
        for (const auto& interrupt : interrupts){
            watchers.emplace_back(interrupt->make_watcher());
            callbacks.emplace_back(*watchers.back());
        }
        for (WatchAlong* along : hold_watchers){
            callbacks.emplace_back(*along);
        }
        context.wait_for_all_requests();
        WallClock start = current_time();
        const int ret = run_until<Context>(
            console, context,
            [&](Context& context){
                start = current_time();
                mover(context, leg.direction, hold);
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
);

template <typename Console, typename Context>
size_t clear_late_interrupts(
    Console& console, Context& context,
    const LegInterrupts<Console, Context>& interrupts,
    Milliseconds watch,
    size_t max_interruptions
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




} // namespace Pokemon
} // namespace PokemonAutomation
#endif

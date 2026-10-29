/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

constexpr Milliseconds TILE_TIME = 267ms;       //  16 frames at 60 fps, measured exact over 15 tiles
constexpr Milliseconds SURF_TILE_TIME = 133ms;  //  the game moves a surfing player at running speed
constexpr Milliseconds TURN_TAP = 40ms;         //  turns a player facing another way, without a step
constexpr Milliseconds TURN_SETTLE = 300ms;
constexpr Milliseconds FACE_HOLD = 40ms;        //  short enough to turn without stepping
constexpr Milliseconds FACE_SETTLE = 300ms;     //  the turn animation, before A reaches what is ahead
constexpr Milliseconds STEP_RELEASE = 100ms;
constexpr Milliseconds WARP_FADE_TIMEOUT = 3000ms;
constexpr Milliseconds WARP_ARRIVAL_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_ANIMATION = 3000ms;
//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
constexpr Milliseconds AFTER_WARP = 1500ms;
//  A message set off by a leg's last step shows after the hold ends. Caught
//  here, before the next leg's turn tap, which the message would swallow.
constexpr Milliseconds AFTER_LEG_WATCH = 500ms;
constexpr int MAX_INTERRUPTIONS_PER_LEG = 5;


class WildBattleInterrupt : public OverworldInterrupt{
public:
    virtual const char* name() const override{
        return "a wild battle";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        return std::make_unique<BattleDialogWatcher>(COLOR_RED);
    }
    //  The battle's opening animation, until "Wild <Pokemon> appeared!".
    //  Not calibrated yet.
    virtual Milliseconds detection_delay() const override{
        return 4000ms;
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        flee_battle(console, context);
    }
};

class MessageInterrupt : public OverworldInterrupt{
public:
    virtual const char* name() const override{
        return "a message";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        return std::make_unique<WhiteDialogWatcher>(COLOR_RED);
    }
    //  The box shows as the step ends; then video lag and the watcher's
    //  250ms. Not calibrated yet.
    virtual Milliseconds detection_delay() const override{
        return 400ms;
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        WhiteDialogOverWatcher closed(COLOR_RED);
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                for (int c = 0; c < 5; c++){
                    pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                }
            },
            {closed}
        );
        if (ret < 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "walk_overworld_route(): a message on the overworld didn't close.",
                console
            );
        }
    }
};


DpadPosition dpad(MapDirection direction){
    switch (direction){
    case MapDirection::NORTH:
        return DPAD_UP;
    case MapDirection::EAST:
        return DPAD_RIGHT;
    case MapDirection::SOUTH:
        return DPAD_DOWN;
    case MapDirection::WEST:
        return DPAD_LEFT;
    }
    return DPAD_NONE;
}

//  A watcher for each interrupt, in the same order.
class InterruptWatchers{
public:
    explicit InterruptWatchers(const OverworldInterrupts& interrupts){
        for (const std::unique_ptr<OverworldInterrupt>& interrupt : interrupts){
            m_watchers.emplace_back(interrupt->make_watcher());
            m_callbacks.emplace_back(*m_watchers.back());
        }
    }
    const std::vector<PeriodicInferenceCallback>& callbacks() const{
        return m_callbacks;
    }

private:
    std::vector<std::unique_ptr<VisualInferenceCallback>> m_watchers;
    std::vector<PeriodicInferenceCallback> m_callbacks;
};

void throw_if_too_many(ConsoleHandle& console, int interruptions, const RouteStep& step){
    if (interruptions >= MAX_INTERRUPTIONS_PER_LEG){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): interrupted " + std::to_string(interruptions) + " times on: " + to_string(step),
            console
        );
    }
}

//  Walks one WALK step, starting from a standstill facing its way.
//
//  An open leg is released half a tile before the last step would end: the
//  step in progress completes and no further step starts. A leg to a wall is
//  held two tiles longer, since the wall stops the player on its tile however
//  long the stick is held.
//
//  When an interrupt stops the leg, a leg to a wall is simply held again. For
//  an open leg, the tiles walked are worked out from when the interrupt was
//  seen: the game only interrupts as a step ends, a whole number of tiles
//  into the hold.
void walk_leg(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterrupts& interrupts
){
    const Milliseconds tile = step.surfing ? SURF_TILE_TIME : TILE_TIME;
    int remaining = step.tiles;
    for (int interruptions = 0; ; interruptions++){
        const Milliseconds hold = step.to_wall
            ? tile * (remaining + 2)
            : tile * (remaining - 1) + tile / 2;
        InterruptWatchers watchers(interrupts);
        WallClock start = current_time();
        context.wait_for_all_requests();
        const int ret = run_until<ProControllerContext>(
            console, context,
            [&](ProControllerContext& context){
                start = current_time();
                pbf_press_dpad(context, dpad(step.direction), hold, STEP_RELEASE);
            },
            watchers.callbacks()
        );
        if (ret < 0){
            return;
        }
        const Milliseconds elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start);
        const OverworldInterrupt& interrupt = *interrupts[ret];
        throw_if_too_many(console, interruptions + 1, step);
        interrupt.clear(console, context);
        if (step.to_wall){
            console.log(std::string("Route: ") + interrupt.name() + " after " + std::to_string(elapsed.count()) + "ms, walking to the wall again.", COLOR_ORANGE);
            continue;
        }
        const int walked = std::clamp(
            (int)((elapsed - interrupt.detection_delay() + tile / 2) / tile),
            0, remaining
        );
        remaining -= walked;
        console.log(
            std::string("Route: ") + interrupt.name() + " after " + std::to_string(elapsed.count())
                + "ms, estimated " + std::to_string(walked) + " tiles walked, " + std::to_string(remaining) + " to go.",
            COLOR_ORANGE
        );
        if (remaining <= 0){
            return;
        }
    }
}

//  Clears what the last step of a leg set off, if anything shows soon after.
void clear_late_interrupts(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterrupts& interrupts
){
    for (int interruptions = 0; ; interruptions++){
        InterruptWatchers watchers(interrupts);
        context.wait_for_all_requests();
        const int ret = wait_until(console, context, AFTER_LEG_WATCH, watchers.callbacks());
        if (ret < 0){
            return;
        }
        throw_if_too_many(console, interruptions + 1, step);
        console.log(std::string("Route: ") + interrupts[ret]->name() + " at the end of the leg.", COLOR_ORANGE);
        interrupts[ret]->clear(console, context);
    }
}

void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    BlackScreenWatcher fade(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, WARP_FADE_TIMEOUT, {fade}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no fade after walking onto the warp to " + step.end.map + ".",
            console
        );
    }
    BlackScreenOverWatcher arrival(COLOR_RED);
    if (wait_until(console, context, WARP_ARRIVAL_TIMEOUT, {arrival}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): the screen stayed black after the warp to " + step.end.map + ".",
            console
        );
    }
}

void start_surf(ConsoleHandle& console, ProControllerContext& context, MapDirection direction, bool facing_water){
    //  "The water is a deep blue... Would you like to SURF?", with YES selected.
    //  An A pressed while the player is still turning is lost.
    if (!facing_water){
        pbf_press_dpad(context, dpad(direction), FACE_HOLD, FACE_SETTLE);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    SelectionDialogWatcher prompt(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, SURF_PROMPT_TIMEOUT, {prompt}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no Surf prompt facing the water.",
            console
        );
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);

    //  "<Pokemon> used SURF!" waits for A, then the player jumps onto the water.
    for (int c = 0; c < 5; c++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        if (!screen || !WhiteDialogDetector(COLOR_RED).detect(screen)){
            break;
        }
        pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    }
    pbf_wait(context, SURF_ANIMATION);
}

}



const OverworldInterrupts& default_overworld_interrupts(){
    //  The battle first: its screen is checked before the message box's.
    static const OverworldInterrupts interrupts = []{
        OverworldInterrupts ret;
        ret.emplace_back(std::make_unique<WildBattleInterrupt>());
        ret.emplace_back(std::make_unique<MessageInterrupt>());
        return ret;
    }();
    return interrupts;
}


void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<MapDirection> facing,
    const OverworldInterrupts& interrupts
){
    //  Each leg starts with a turn tap unless it goes the way the player faces.
    //  If the first facing isn't known it taps anyway, which takes an extra
    //  step if the player already faced that way.
    for (const RouteStep& step : route){
        console.log("Route: " + to_string(step));
        switch (step.action){
        case RouteAction::WALK:
            //  Turning first, with a tap, makes the hold start from a standstill
            //  facing the right way: holding through a turn lost a tile on the
            //  console, while holds from a standstill were exact over 15 tiles.
            if (facing != step.direction){
                pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TURN_SETTLE);
            }
            walk_leg(console, context, step, interrupts);
            if (step.warps){
                wait_out_warp(console, context, step);
                pbf_wait(context, AFTER_WARP);
            }
            break;
        case RouteAction::SURF:
            start_surf(console, context, step.direction, facing == step.direction);
            break;
        case RouteAction::FACE:
            pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
            break;
        }
        if (step.action != RouteAction::FACE){
            clear_late_interrupts(console, context, step, interrupts);
        }
        facing = step.walked_out ? MapDirection::SOUTH : step.direction;
    }
    context.wait_for_all_requests();
}



}
}
}

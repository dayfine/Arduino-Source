/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_PlayerFacingReader.h"
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
//  From the end of the hold, through the fade out and in.
constexpr Milliseconds WARP_TIMEOUT = 8000ms;
constexpr Milliseconds SURF_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_ANIMATION = 3000ms;
//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
constexpr Milliseconds AFTER_WARP = 1500ms;
//  After a warp's fade, which a wild battle's flashes and wipe can pass for:
//  a battle's HP box shows within this long (console B, 2026-11-01).
constexpr Milliseconds AFTER_WARP_BATTLE_WATCH = 5000ms;
//  A message set off by a leg's last step shows after the hold ends. Caught
//  here, before the next leg's turn tap, which the message would swallow.
constexpr Milliseconds AFTER_LEG_WATCH = 500ms;
//  After a leg through grass. A battle whose flashes are missed is seen only
//  by its "Wild <Pokemon> appeared!", 3.7s after its step's press (twice on
//  Route 1, console B, 2026-11-01, run 7). Unless the walker waits that long,
//  the next leg is walked into the battle and lost: there, "east 4" and
//  "west 4".
constexpr Milliseconds AFTER_WILD_LEG_WATCH = 3500ms;
//  After a message closes: a trainer who saw the player speaks first, then
//  the battle starts, with "<Trainer> would like to battle!" seconds later.
constexpr Milliseconds TRAINER_BATTLE_WATCH = 6000ms;
constexpr size_t MAX_INTERRUPTIONS_PER_LEG = 5;


//  A wild battle, seen by the flashes it starts with.
class WildBattleInterrupt : public OverworldInterrupt{
public:
    virtual std::string name() const override{
        return "a wild battle";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        return std::make_unique<BattleFlashWatcher>(COLOR_RED);
    }
    //  The first flash shows about 130ms after the step on the console, and
    //  the watcher needs it to start fading back. Not calibrated yet.
    virtual Milliseconds detection_delay() const override{
        return 300ms;
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        flee_battle(console, context);
    }
};

//  A wild battle whose flashes were missed, seen once "Wild <Pokemon>
//  appeared!" shows. Where an open leg stopped is then only a rough guess.
class LateWildBattleInterrupt : public WildBattleInterrupt{
public:
    virtual std::string name() const override{
        return "a wild battle (seen late)";
    }
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const override{
        return std::make_unique<BattleScreenWatcher>(COLOR_RED);
    }
    //  The flashes, the wipe and the opening animation: 3.7s after the step's
    //  press, twice on Route 1 (console B, 2026-11-01). Other maps open
    //  battles with other wipes, so elsewhere this is a guess.
    virtual Milliseconds detection_delay() const override{
        return 3700ms;
    }
};

class MessageInterrupt : public OverworldInterrupt{
public:
    virtual std::string name() const override{
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
        check_no_trainer_battle(console, context);
    }

private:
    //  A trainer's battle can't be fled, and the walker must not try (console
    //  B, 2026-11-01, Viridian Forest): it stops here for the caller.
    static void check_no_trainer_battle(ConsoleHandle& console, ProControllerContext& context){
        BattleScreenWatcher battle(COLOR_RED);
        context.wait_for_all_requests();
        if (wait_until(console, context, TRAINER_BATTLE_WATCH, {battle}) >= 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "walk_overworld_route(): trainer battle: a battle started after a message on the overworld.",
                console
            );
        }
    }
};

//  A message, taken to be "REPEL's effect wore off...", after which another
//  Max Repel is used from the Items pocket.
class RepelWoreOffInterrupt : public MessageInterrupt{
public:
    explicit RepelWoreOffInterrupt(int max_repel_slot)
        : m_max_repel_slot(max_repel_slot)
    {}
    virtual std::string name() const override{
        return "a message (Repel wore off?)";
    }
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const override{
        MessageInterrupt::clear(console, context);
        use_max_repel_from_bag(console, context, m_max_repel_slot);
    }

private:
    int m_max_repel_slot;
};


DpadPosition dpad(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return DPAD_UP;
    case Direction::EAST:
        return DPAD_RIGHT;
    case Direction::SOUTH:
        return DPAD_DOWN;
    case Direction::WEST:
        return DPAD_LEFT;
    }
    return DPAD_NONE;
}

//  The leg mover: holds the D-pad. FRLG moves exactly a tile per 16 frames.
class DpadMover : public Pokemon::LegMover<ProControllerContext>{
public:
    virtual void hold(ProControllerContext& context, Direction direction, Milliseconds duration) const override{
        pbf_press_dpad(context, dpad(direction), duration, STEP_RELEASE);
    }
};

//  An open leg stops at its first interruption: how far it got isn't
//  counted (that went wrong too often on the console), the route finds the
//  player again at a wall instead. See walk_overworld_route().
Pokemon::LegSpec leg_spec(const RouteStep& step){
    Pokemon::LegSpec leg;
    leg.direction = step.direction;
    leg.tiles = step.tiles;
    leg.to_wall = step.to_wall;
    leg.tile_time = step.surfing ? SURF_TILE_TIME : TILE_TIME;
    leg.stop_on_interrupt = !step.to_wall;
    leg.stop_if_blocked = step.to_wall;
    return leg;
}

//  One watcher for the whole fade: a ladder's black is short enough to be
//  over before a second watcher, started once the fade is seen, looks.
void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    BlackScreenOverWatcher arrival(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, WARP_TIMEOUT, {arrival}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no fade in and out after walking onto the warp to " + step.end.map + ".",
            console
        );
    }
}

//  Fails unless the overworld is up. Under the START menu or a message the
//  D-pad only moves a cursor, and legs to a wall seem to go fine: three
//  "walked" that way in Viridian Forest without the player moving (console B,
//  2026-11-01).
void check_overworld(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return;
    }
    const char* open = nullptr;
    if (StartMenuDetector(COLOR_RED).detect(screen)){
        open = "the START menu";
    }else if (SelectionDialogDetector(COLOR_RED).detect(screen)){
        open = "a menu";
    }else if (WhiteDialogDetector(COLOR_RED).detect(screen)){
        open = "a message";
    }
    if (open != nullptr){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::string("walk_overworld_route(): ") + open + " is open, before: " + to_string(step),
            console
        );
    }
}

//  After a warp's fade: fails, once the battle is fled, if it was a wild
//  battle's. In run 7 (console B, 2026-11-01) the player had drifted onto
//  Route 1's grass, and the step meant for Oak's Lab's door set off a
//  battle; its flashes and wipe passed for the warp's fade, the route went
//  on as if inside, and the next screen was the battle's FIGHT menu.
void check_warp_was_no_battle(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    BattleScreenWatcher battle(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, AFTER_WARP_BATTLE_WATCH, {battle}) < 0){
        return;
    }
    flee_battle(console, context);
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "walk_overworld_route(): a wild battle, not the warp to " + step.end.map + ": the player is off the route.",
        console
    );
}

void start_surf(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_water){
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

//  One step of a route, for a player facing `facing`. Returns false if an
//  open leg stopped part-way, so where the player is isn't known.
bool walk_step(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts
){
    switch (step.action){
    case RouteAction::WALK:
        //  Turning first, with a tap, makes the hold start from a standstill
        //  facing the right way: holding through a turn lost a tile on the
        //  console, while holds from a standstill were exact over 15 tiles.
        if (facing != step.direction){
            pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TURN_SETTLE);
        }
        if (Pokemon::walk_leg(console, context, DpadMover(), step.wild ? interrupts.wild : interrupts.other, leg_spec(step), MAX_INTERRUPTIONS_PER_LEG).stopped){
            return false;
        }
        if (step.warps){
            wait_out_warp(console, context, step);
            static_assert(AFTER_WARP_BATTLE_WATCH >= AFTER_WARP);
            check_warp_was_no_battle(console, context, step);
        }
        break;
    case RouteAction::SURF:
        start_surf(console, context, step.direction, facing == step.direction);
        break;
    case RouteAction::FACE:
        pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
        break;
    }
    return true;
}

//  walk_step(), then a watch for what its last step set off. Returns false
//  if the player may have stopped short of the step's end: an open leg
//  stopped, or something seen after the hold, which may have stopped it
//  before the end (after a warp, it was checked for already).
StepEnd walk_and_watch(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts
){
    if (!walk_step(console, context, step, facing, interrupts)){
        return StepEnd::STOPPED;
    }
    if (step.action == RouteAction::FACE){
        return StepEnd::DONE;
    }
    const Milliseconds watch = step.wild ? AFTER_WILD_LEG_WATCH : AFTER_LEG_WATCH;
    const size_t late = Pokemon::clear_late_interrupts(console, context, step.wild ? interrupts.wild : interrupts.other, watch, MAX_INTERRUPTIONS_PER_LEG);
    //  After a warp it was checked for already.
    return late == 0 || step.warps || step.action != RouteAction::WALK ? StepEnd::DONE : StepEnd::LATE;
}

//  Holds on along `step`, stopped part-way off the grass, until its warp's
//  fade. Off the grass only a message stops a leg (a trainer who saw the
//  player, a Repel wearing off), and the player then stands on the leg's
//  line, so holding on reaches the warp however far they got (console B,
//  2026-11-01: Camper Liam stopped "south 9 (warp)" out of Pewter Gym).
//  After a trainer the player faces them, so this turns first.
void hold_to_warp(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterrupts& interrupts
){
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
                pbf_press_dpad(context, dpad(step.direction), TILE_TIME * (step.tiles + 2), STEP_RELEASE);
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

//  Each leg starts with a turn tap unless it goes the way the player faces.
//  A tap while already facing that way takes a step, so a facing not given
//  is read from the screen. If that fails too it taps anyway.
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    std::optional<Direction> facing;
    if (screen){
        facing = read_player_facing(screen);
    }
    console.log(std::string("Route: the player faces ") + (facing ? Pokemon::direction_name(*facing) : "an unknown way") + ".");
    return facing;
}

}



OverworldInterruptSet overworld_interrupts_with_repel(int max_repel_slot){
    OverworldInterruptSet ret;
    ret.wild.emplace_back(std::make_unique<WildBattleInterrupt>());
    ret.wild.emplace_back(std::make_unique<LateWildBattleInterrupt>());
    ret.wild.emplace_back(std::make_unique<RepelWoreOffInterrupt>(max_repel_slot));
    ret.other.emplace_back(std::make_unique<RepelWoreOffInterrupt>(max_repel_slot));
    return ret;
}

void use_max_repel_from_bag(ConsoleHandle& console, ProControllerContext& context, int slot){
    console.log(std::format("Using a Max Repel (Items pocket, slot {}).", slot + 1));
    open_bag_from_overworld(console, context);
    //  The bag is seen while it still slides in, and ignores presses until
    //  it is done.
    pbf_wait(context, 1000ms);
    //  To the Items pocket, then to the top of it.
    for (int c = 0; c < 3; c++){
        pbf_press_dpad(context, DPAD_LEFT, 100ms, 600ms);
    }
    for (int c = 0; c < 20; c++){
        pbf_press_dpad(context, DPAD_UP, 100ms, 100ms);
    }
    for (int c = 0; c < slot; c++){
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 300ms);
    }
    //  USE, "<player> used the MAX REPEL.", back to the bag, then out of the
    //  bag and the start menu. A third A here would open the item's menu again.
    pbf_press_button(context, BUTTON_A, 160ms, 800ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(context, BUTTON_A, 160ms, 1500ms);
    pbf_press_button(context, BUTTON_B, 160ms, 2000ms);
    pbf_press_button(context, BUTTON_B, 160ms, 1000ms);
    context.wait_for_all_requests();
}

const OverworldInterruptSet& default_overworld_interrupts(){
    //  Battles first: their screens are checked before the message box's.
    static const OverworldInterruptSet interrupts = []{
        OverworldInterruptSet ret;
        ret.wild.emplace_back(std::make_unique<WildBattleInterrupt>());
        ret.wild.emplace_back(std::make_unique<LateWildBattleInterrupt>());
        ret.wild.emplace_back(std::make_unique<MessageInterrupt>());
        ret.other.emplace_back(std::make_unique<MessageInterrupt>());
        return ret;
    }();
    return interrupts;
}


void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts,
    const RoutePlanning* planning
){
    if (route.empty()){
        return;
    }
    if (!facing){
        facing = read_facing(console, context);
    }
    RouteCalls calls;
    calls.walk = [&](const RouteStep& step, std::optional<Direction> from_facing){
        check_overworld(console, context, step);
        return walk_and_watch(console, context, step, from_facing, interrupts);
    };
    calls.hold_to_warp = [&](const RouteStep& step){
        hold_to_warp(console, context, step, interrupts.other);
    };
    calls.log = [&](const std::string& message){ console.log(message, COLOR_ORANGE); };
    calls.fail = [&](const std::string& message){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, console);
    };
    run_route(route, facing, planning, calls);
    context.wait_for_all_requests();
}



}
}
}

/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleTextBoxDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using namespace MovementMechanics;
using namespace OverworldWalkSteps;


namespace{

//  From the end of the hold, through the fade out and in.
constexpr Milliseconds WARP_TIMEOUT = 8000ms;
constexpr Milliseconds SURF_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_ANIMATION = 3000ms;
//  After a message closes: a trainer who saw the player speaks first, then
//  the battle starts, with "<Trainer> would like to battle!" seconds later.
constexpr Milliseconds TRAINER_BATTLE_WATCH = 6000ms;


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
    virtual Milliseconds detection_delay() const override{
        return OverworldWalkSteps::LATE_WILD_BATTLE_DELAY;
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


Pokemon::LegSpec leg_spec(const RouteStep& step, MoveSpeed speed){
    Pokemon::LegSpec leg{step.direction, step.tiles + step.warp_slack, step.to_wall, tile_time(step, speed)};
    leg.stalled_steps = step.connection_steps;
    return leg;
}

} // namespace


namespace OverworldWalkSteps{

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

void hold_dpad(ProControllerContext& context, Direction direction, Milliseconds duration){
    pbf_press_dpad(context, dpad(direction), duration, STEP_RELEASE);
}

void hold_dpad_running(ProControllerContext& context, Direction direction, Milliseconds duration){
    pbf_controller_state(context, BUTTON_B, dpad(direction), {0, 0}, {0, 0}, duration);
    pbf_wait(context, STEP_RELEASE);
}

MoveSpeed step_speed(const RouteStep& step, MoveSpeed speed){
    return step.walk_only ? MoveSpeed::WALK : speed;
}

Milliseconds tile_time(const RouteStep& step, MoveSpeed speed){
    if (step.surfing){
        return SURF_TILE_TIME;
    }
    return step_speed(step, speed) == MoveSpeed::RUN ? RUN_TILE_TIME : TILE_TIME;
}

Pokemon::LegMover<ProControllerContext> leg_mover(const RouteStep& step, MoveSpeed speed){
    if (step_speed(step, speed) == MoveSpeed::RUN && !step.surfing){
        return hold_dpad_running;
    }
    return hold_dpad;
}

Milliseconds leg_hold_time(const RouteStep& step, MoveSpeed speed){
    return Pokemon::leg_hold(leg_spec(step, speed), step.tiles);
}

std::string speed_note(const RouteStep& step, MoveSpeed speed){
    if (step.action != RouteAction::WALK || !step.to_wall || step.surfing){
        return "";
    }
    return std::format(
        "[Speed] {} {}: {} tiles in {} ms, held {} ms",
        move_speed_name(step_speed(step, speed)), Pokemon::direction_name(step.direction), step.tiles,
        (tile_time(step, speed) * step.tiles).count(), leg_hold_time(step, speed).count()
    );
}

std::vector<Pokemon::WatchAlong*> warp_hold_watchers(const RouteStep& step, Pokemon::WatchAlong& fade){
    if (!step.warps){
        return {};
    }
    return {&fade};
}

//  One watcher for the whole fade: a ladder's black is short enough to be
//  over before a second watcher, started once the fade is seen, looks.
void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step, Pokemon::WatchAlong& fade){
    if (fade.fired()){
        console.log("Route: the fade to " + step.end.map + " was over before the hold ended.");
        return;
    }
    context.wait_for_all_requests();
    if (wait_until(console, context, WARP_TIMEOUT, {fade.watcher()}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no fade in and out after walking onto the warp to " + step.end.map + ".",
            console
        );
    }
}

void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    BlackScreenOverWatcher arrival(COLOR_RED);
    Pokemon::WatchAlong fade(arrival);
    wait_out_warp(console, context, step, fade);
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

void clear_open_message(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& interrupts, const RouteStep& step
){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen || !WhiteDialogDetector(COLOR_RED).detect(screen)){
        return;
    }
    console.log("Route: a message is open before " + to_string(step) + "; handing it to the leg's interrupts.", COLOR_ORANGE);
    Pokemon::clear_late_interrupts(console, context, interrupts, OPEN_MESSAGE_WATCH, 1);
}

//  A battle whose flashes were missed shows its HP boxes, which the leg's
//  watch for a late battle needs, seconds after its text box: in Mt. Moon
//  the watch after "south 15 (to wall)" ended on the box still empty, the
//  leg was taken as walked to its wall, and the next leg was walked into
//  the battle, 5 tiles short of where it was planned from (console B,
//  2026-11-03, row 00.13).
namespace{

//  What on screen shows a wild battle on its way: its text box, or the
//  black of its wipe. Nothing if neither shows, or on a dark cave's
//  overworld (dark_cave_screen()).
const char* battle_on_its_way(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen || dark_cave_screen(*screen.frame)){
        return nullptr;
    }
    if (BattleTextBoxDetector(COLOR_RED).detect(screen)){
        return "a battle's text box";
    }
    if (BlackScreenDetector(COLOR_RED).detect(screen)){
        return "a black screen";
    }
    return nullptr;
}

//  The parts of the game screen a dark cave shows black (its corners) and
//  lit (the player's tile and the ones next to it), and how bright each
//  may be, as the sum of the average's channels. Not measured on the
//  console's frames yet: the corners are a GBA screen's 2 by 2 tiles, the
//  middle its player's tile.
constexpr double DARK_CORNER_MAX = 45.0;
constexpr double LIT_MIDDLE_MIN = 120.0;

bool box_brightness_below(const ImageViewRGB32& screen, const ImageFloatBox& box, double limit){
    return image_stats(extract_box_reference(screen, box)).average.sum() < limit;
}

} // namespace

bool dark_cave_screen(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    const ImageFloatBox game = GameSettings::instance().GAME_BOX;
    auto part = [&](double x, double y, double width, double height){
        return ImageFloatBox(game.x + game.width * x, game.y + game.height * y, game.width * width, game.height * height);
    };
    const ImageFloatBox corners[] = {
        part(0.00, 0.00, 0.13, 0.20), part(0.87, 0.00, 0.13, 0.20),
        part(0.00, 0.80, 0.13, 0.20), part(0.87, 0.80, 0.13, 0.20),
    };
    for (const ImageFloatBox& corner : corners){
        if (!box_brightness_below(screen, corner, DARK_CORNER_MAX)){
            return false;
        }
    }
    return !box_brightness_below(screen, part(0.45, 0.42, 0.10, 0.16), LIT_MIDDLE_MIN);
}

size_t clear_unseen_battle(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& wild, const RouteStep& step
){
    if (step.action != RouteAction::WALK || step.warps){
        return 0;
    }
    const char* seen = battle_on_its_way(console, context);
    if (seen == nullptr){
        return 0;
    }
    console.log(std::format("Route: {} after {}, which nothing stopped: watching on for a wild battle.", seen, to_string(step)), COLOR_ORANGE);
    return Pokemon::clear_late_interrupts(console, context, wild, UNSEEN_BATTLE_WATCH, MAX_INTERRUPTIONS_PER_LEG);
}

size_t clear_battle_before(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& wild, const RouteStep& step
){
    const char* seen = battle_on_its_way(console, context);
    if (seen == nullptr){
        return 0;
    }
    console.log(std::format("Route: {} before {}: watching on for a wild battle set off before it.", seen, to_string(step)), COLOR_ORANGE);
    return Pokemon::clear_late_interrupts(console, context, wild, UNSEEN_BATTLE_WATCH, MAX_INTERRUPTIONS_PER_LEG);
}

//  After a warp's fade: fails, once the battle is fled, if it was a wild
//  battle's. In run 7 (console B, 2026-11-01) the player had drifted onto
//  Route 1's grass, and the step meant for Oak's Lab's door set off a
//  battle; its flashes and wipe passed for the warp's fade, the route went
//  on as if inside, and the next screen was the battle's FIGHT menu.
bool run_from_battle_after_warp(ConsoleHandle& console, ProControllerContext& context){
    BattleScreenWatcher battle(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, AFTER_WARP_BATTLE_WATCH, {battle}) < 0){
        return false;
    }
    flee_battle(console, context);
    return true;
}

void check_warp_was_no_battle(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    if (!run_from_battle_after_warp(console, context)){
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "walk_overworld_route(): a wild battle, not the warp to " + step.end.map + ": the player is off the route.",
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

void turn_to(ConsoleHandle& console, ProControllerContext& context, Direction direction){
    for (size_t tap = 1; tap <= MAX_TURN_TAPS; tap++){
        pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const std::optional<Direction> facing = screen ? read_player_facing(screen) : std::nullopt;
        if (!facing || *facing == direction){
            return;
        }
        console.log(std::format(
            "Route: after turn tap {} {}, the player faces {}.",
            tap, Pokemon::direction_name(direction), Pokemon::direction_name(*facing)
        ), COLOR_ORANGE);
    }
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

} // namespace OverworldWalkSteps



const char* move_speed_name(MoveSpeed speed){
    switch (speed){
    case MoveSpeed::WALK:
        return "walk";
    case MoveSpeed::RUN:
        return "run";
    }
    return "?";
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


namespace{

//  A WALK step's leg, and the fade of the warp it walks onto, watched from
//  the start of the hold.
void walk_step_leg(
    ConsoleHandle& console, ProControllerContext& context,
    const RouteStep& step, const OverworldInterruptSet& interrupts, MoveSpeed speed
){
    BlackScreenOverWatcher arrival(COLOR_RED);
    Pokemon::WatchAlong fade(arrival);
    Pokemon::walk_leg(
        console, context, leg_mover(step, speed), step.wild ? interrupts.wild : interrupts.other,
        leg_spec(step, speed), MAX_INTERRUPTIONS_PER_LEG, warp_hold_watchers(step, fade)
    );
    if (step.spin_tiles){
        //  The hold let go on the spinner; the game slides the player on.
        pbf_wait(context, SPIN_TILE_TIME * *step.spin_tiles + SPIN_STOP_SETTLE);
    }
    if (!step.warps){
        return;
    }
    wait_out_warp(console, context, step, fade);
    static_assert(AFTER_WARP_BATTLE_WATCH >= AFTER_WARP);
    check_warp_was_no_battle(console, context, step);
}

} // namespace


void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts,
    MoveSpeed speed
){
    if (!facing){
        facing = read_facing(console, context);
    }
    for (const RouteStep& step : route){
        console.log("Route: " + to_string(step));
        clear_open_message(console, context, step.wild ? interrupts.wild : interrupts.other, step);
        check_overworld(console, context, step);
        switch (step.action){
        case RouteAction::WALK:
            //  Turning first, with a tap, makes the hold start from a standstill
            //  facing the right way: holding through a turn lost a tile on the
            //  console, while holds from a standstill were exact over 15 tiles.
            if (facing != step.direction){
                turn_to(console, context, step.direction);
            }
            if (const std::string note = speed_note(step, speed); !note.empty()){
                console.log(note);
            }
            walk_step_leg(console, context, step, interrupts, speed);
            break;
        case RouteAction::SURF:
            start_surf(console, context, step.direction, facing == step.direction);
            break;
        case RouteAction::FACE:
            pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
            break;
        }
        if (step.action != RouteAction::FACE){
            const OverworldInterrupts& watched = step.wild ? interrupts.wild : interrupts.other;
            if (Pokemon::clear_late_interrupts(console, context, watched, step.wild ? AFTER_WILD_LEG_WATCH : AFTER_LEG_WATCH, MAX_INTERRUPTIONS_PER_LEG) == 0){
                clear_unseen_battle(console, context, interrupts.wild, step);
            }
        }
        facing = step.facing_after;
    }
    context.wait_for_all_requests();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

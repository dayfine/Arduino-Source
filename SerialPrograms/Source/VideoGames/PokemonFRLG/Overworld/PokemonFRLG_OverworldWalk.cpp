/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include <cstdlib>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
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
#include "PokemonFRLG_OverworldBike.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_BikeSpinPresses.h"

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
//  A card-key door's message, and the door opening after it.
constexpr Milliseconds GATE_MESSAGE_WAIT = 1500ms;
constexpr Milliseconds GATE_SETTLE = 1500ms;
//  The Yes/No prompt of a statue, and how long B's through the text before
//  and after the switch last.
constexpr Milliseconds SWITCH_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SWITCH_SETTLE = 2000ms;
constexpr int MAX_TEXT_BOXES = 5;
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
    switch (step_speed(step, speed)){
    case MoveSpeed::RUN:
        return RUN_TILE_TIME;
    case MoveSpeed::BIKE:
        return BIKE_TILE_TIME;
    case MoveSpeed::WALK:
        break;
    }
    return TILE_TIME;
}

bool turns_first(const RouteStep& step, MoveSpeed speed, std::optional<Direction> facing){
    if (facing == step.direction){
        return false;
    }
    return step.surfing || step_speed(step, speed) != MoveSpeed::BIKE || BIKE_TAP_ONLY_TURNS;
}

bool counts_bike_taps(const RouteStep& step, MoveSpeed speed){
    return step.action == RouteAction::WALK && step_speed(step, speed) == MoveSpeed::BIKE
        && !step.surfing && !step.to_wall && !step.warps && !step.spin_tiles
        && step.connection_steps.empty() && step.tiles > 0;
}

bool gets_on_bike(const RouteStep& step, MoveSpeed speed, bool riding){
    return speed == MoveSpeed::BIKE && !riding && step.action == RouteAction::WALK
        && !step.walk_only && !step.surfing;
}

bool select_onto_bike(const BikeSelectCalls& calls){
    for (int press = 1; press <= 2; press++){
        calls.press_select();
        if (calls.riding()){
            return true;
        }
        if (calls.wait_to_read_again){
            calls.wait_to_read_again();
        }
        if (calls.riding()){
            return true;
        }
    }
    return false;
}

bool screen_shows_riding(const ImageViewRGB32& screen, PlayerGender player){
    if (StartMenuDetector(COLOR_RED).detect(screen) || SelectionDialogDetector(COLOR_RED).detect(screen)
        || WhiteDialogDetector(COLOR_RED).detect(screen)
    ){
        return false;
    }
    return !read_player_facing(screen, player).has_value();
}

void get_on_bike(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    BikeSelectCalls calls;
    calls.press_select = [&]{
        pbf_press_button(context, BUTTON_MINUS, BIKE_SELECT_PRESS, BIKE_SELECT_SETTLE);
    };
    calls.riding = [&]{
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const bool riding = screen && screen_shows_riding(*screen.frame, player);
        console.log(riding ? "Bike: riding." : "Bike: not riding after SELECT.", riding ? COLOR_BLACK : COLOR_ORANGE);
        return riding;
    };
    calls.wait_to_read_again = [&]{
        pbf_wait(context, BIKE_SELECT_SETTLE);
    };
    console.log("Bike: SELECT (MINUS) onto the Bicycle.");
    if (!select_onto_bike(calls)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "get_on_bike(): not on the Bicycle after SELECT twice. Is it registered?",
            console
        );
    }
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

namespace{

//  How close (pixel RMSD) the overlap of two screens is for the same scene.
constexpr double SAME_SCENE_RMSD = 25;
//  The GBA screen's tiles across and down.
constexpr int SCREEN_TILES_X = 15;
constexpr int SCREEN_TILES_Y = 10;
//  From the fade's end until the new map is in full light.
constexpr Milliseconds WARP_FADE_IN(300);
//  Between screens compared for a dim warp's fade (wait_out_dim_warp()).
constexpr Milliseconds DIM_WARP_POLL(150);

//  The RMSD of `before` and `after` where they overlap, with the scene
//  scrolled `dx`, `dy` pixels: `after` at (x, y) shows `before` at
//  (x + dx, y + dy).
double scrolled_rmsd(const ImageViewRGB32& before, const ImageViewRGB32& after, int dx, int dy){
    const size_t width = before.width() - (size_t)std::abs(dx);
    const size_t height = before.height() - (size_t)std::abs(dy);
    const ImageViewRGB32 old_part = before.sub_image(dx > 0 ? dx : 0, dy > 0 ? dy : 0, width, height);
    const ImageViewRGB32 new_part = after.sub_image(dx < 0 ? -dx : 0, dy < 0 ? -dy : 0, width, height);
    return ImageMatch::pixel_RMSD(old_part, new_part);
}

//  Whether `after` no longer shows the scene `before` showed, scrolled 0 to
//  `max_tiles` tiles on toward `direction` inside the game box `game`: black
//  then clear with the old scene back is no warp. A screen too dark to
//  compare (a dark cave) counts as left.
bool warp_left_scene(
    const ImageViewRGB32& before, const ImageViewRGB32& after,
    const ImageFloatBox& game, Direction direction, int max_tiles
){
    const ImageViewRGB32 old_box = extract_box_reference(before, game);
    const ImageViewRGB32 new_box = extract_box_reference(after, game);
    if (old_box.width() != new_box.width() || old_box.height() != new_box.height()){
        return true;
    }
    if (dark_cave_screen(before) || dark_cave_screen(after)){
        return true;
    }
    const int tile_x = (int)old_box.width() / SCREEN_TILES_X;
    const int tile_y = (int)old_box.height() / SCREEN_TILES_Y;
    //  Keep at least 5 tiles of overlap to compare.
    const int most = std::min(max_tiles, (direction == Direction::NORTH || direction == Direction::SOUTH ? SCREEN_TILES_Y : SCREEN_TILES_X) - 5);
    for (int tiles = 0; tiles <= most; tiles++){
        int dx = 0;
        int dy = 0;
        switch (direction){
        case Direction::NORTH: dy = -tiles * tile_y; break;
        case Direction::SOUTH: dy = tiles * tile_y; break;
        case Direction::WEST:  dx = -tiles * tile_x; break;
        case Direction::EAST:  dx = tiles * tile_x; break;
        }
        if (scrolled_rmsd(old_box, new_box, dx, dy) < SAME_SCENE_RMSD){
            return false;
        }
    }
    return true;
}

} // namespace

bool arrived_without_fade(
    const ImageViewRGB32& before, const ImageViewRGB32& now, const ImageViewRGB32& later,
    const ImageFloatBox& game, const RouteStep& step
){
    if (BlackScreenDetector(COLOR_RED).detect(later)){
        return false;
    }
    const ImageViewRGB32 now_box = extract_box_reference(now, game);
    const ImageViewRGB32 later_box = extract_box_reference(later, game);
    if (now_box.width() != later_box.width() || now_box.height() != later_box.height()){
        return false;
    }
    if (scrolled_rmsd(now_box, later_box, 0, 0) >= SAME_SCENE_RMSD){
        return false;
    }
    return warp_left_scene(before, later, game, step.direction, step.tiles + step.warp_slack + 2);
}

namespace{

//  arrived_without_fade() on the screen now and DIM_WARP_POLL later.
bool arrived_unseen(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step, const VideoSnapshot& before){
    if (!before){
        return false;
    }
    context.wait_for_all_requests();
    const VideoSnapshot now = console.video().snapshot();
    context.wait_for(DIM_WARP_POLL);
    const VideoSnapshot later = console.video().snapshot();
    if (!now || !later){
        return false;
    }
    if (!arrived_without_fade(*before.frame, *now.frame, *later.frame, GameSettings::instance().GAME_BOX, step)){
        return false;
    }
    console.log(std::format("Route: no fade seen, but another scene is up and still: the warp to {} was taken.", step.end.map), COLOR_ORANGE);
    return true;
}

//  One watcher for the whole fade: a ladder's black is short enough to be
//  over before a second watcher, started once the fade is seen, looks. A
//  fade missed altogether is taken by the scene after it (arrived_unseen()),
//  before any step more.
void wait_out_fade(
    ConsoleHandle& console, ProControllerContext& context, const RouteStep& step,
    Pokemon::WatchAlong& fade, const VideoSnapshot& before
){
    if (fade.fired()){
        console.log("Route: the fade to " + step.end.map + " was over before the hold ended.");
        return;
    }
    context.wait_for_all_requests();
    if (wait_until(console, context, WARP_TIMEOUT, {fade.watcher()}) >= 0){
        return;
    }
    if (arrived_unseen(console, context, step, before)){
        return;
    }
    //  A hold that ended short of a stairs or door warp, or on an arrow
    //  warp: a step more at a time, twice, takes it. A hold ends up to two
    //  tiles short, and an arrow warp fires on the press made standing on it.
    for (int tap = 0; tap < 2; tap++){
        console.log("Route: no fade; one more step into the warp.", COLOR_ORANGE);
        BlackScreenOverWatcher arrival(COLOR_RED);
        Pokemon::WatchAlong retry_fade(arrival);
        pbf_press_dpad(context, dpad(step.direction), STEP_TAP, STEP_TAP_RELEASE);
        if (wait_until(console, context, WARP_TIMEOUT, {retry_fade.watcher()}) >= 0){
            return;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "walk_overworld_route(): no fade in and out after walking onto the warp to " + step.end.map + ".",
        console
    );
}

//  After the steps onto a warp whose fade only dims the screen
//  (RouteStep::fade_dims): waits, up to the warp timeout, for the scene
//  from `before`, taken before the first of those steps, to be gone and
//  the screen settled; fails if it never is.
void wait_out_dim_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step, const VideoSnapshot& before){
    context.wait_for_all_requests();
    if (!before){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("walk_overworld_route(): no screen before the steps onto the warp to {}.", step.end.map),
            console
        );
    }
    const ImageFloatBox& game = GameSettings::instance().GAME_BOX;
    VideoSnapshot last;
    const WallClock deadline = current_time() + WARP_TIMEOUT;
    while (current_time() < deadline){
        context.wait_for(DIM_WARP_POLL);
        VideoSnapshot now = console.video().snapshot();
        if (!now){
            continue;
        }
        const bool left = warp_left_scene(*before.frame, *now.frame, game, step.direction, step.tiles + 2);
        if (left && last && scrolled_rmsd(
            extract_box_reference(*last.frame, game), extract_box_reference(*now.frame, game), 0, 0
        ) < SAME_SCENE_RMSD){
            console.log(std::format("Route: the dim fade to {} is over.", step.end.map));
            return;
        }
        last = left ? std::move(now) : VideoSnapshot();
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("walk_overworld_route(): the scene from before the steps onto the warp to {} is still up.", step.end.map),
        console
    );
}

} // namespace

//  A black screen that clears with the player where they were is no warp
//  (console B, 2026-11-10 01:44, 09.01 run 4: "east 1 (warp)" was taken as
//  walked while the player stayed on B4F).
void wait_out_warp(
    ConsoleHandle& console, ProControllerContext& context, const RouteStep& step,
    Pokemon::WatchAlong& fade, const VideoSnapshot& before
){
    wait_out_fade(console, context, step, fade, before);
    if (!before){
        return;
    }
    pbf_wait(context, WARP_FADE_IN);
    context.wait_for_all_requests();
    const VideoSnapshot after = console.video().snapshot();
    if (!after){
        return;
    }
    if (!warp_left_scene(*before.frame, *after.frame, GameSettings::instance().GAME_BOX, step.direction, step.tiles + step.warp_slack + 2)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): the warp to " + step.end.map + " not confirmed: the screen after the fade shows the scene from before the hold.",
            console
        );
    }
}

void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step, const VideoSnapshot& before){
    BlackScreenOverWatcher arrival(COLOR_RED);
    Pokemon::WatchAlong fade(arrival);
    if (step.fade_dims){
        wait_out_dim_warp(console, context, step, before);
        return;
    }
    wait_out_warp(console, context, step, fade, before);
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
    std::string_view open;
    if (StartMenuDetector(COLOR_RED).detect(screen)){
        open = "the START menu";
    }else if (SelectionDialogDetector(COLOR_RED).detect(screen)){
        open = "a menu";
    }else if (WhiteDialogDetector(COLOR_RED).detect(screen)){
        open = "a message";
    }
    if (!open.empty()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("walk_overworld_route(): {} is open, before: {}", open, to_string(step)),
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
//  2026-11-03, row 04.05).
namespace{

//  What on screen shows a wild battle on its way: its text box, or the
//  black of its wipe. Empty if neither shows, or on a dark cave's
//  overworld (dark_cave_screen()).
std::string_view battle_on_its_way(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen || dark_cave_screen(*screen.frame)){
        return {};
    }
    if (BattleTextBoxDetector(COLOR_RED).detect(screen)){
        return "a battle's text box";
    }
    if (BlackScreenDetector(COLOR_RED).detect(screen)){
        return "a black screen";
    }
    return {};
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
    const std::string_view seen = battle_on_its_way(console, context);
    if (seen.empty()){
        return 0;
    }
    console.log(std::format("Route: {} after {}, which nothing stopped: watching on for a wild battle.", seen, to_string(step)), COLOR_ORANGE);
    return Pokemon::clear_late_interrupts(console, context, wild, UNSEEN_BATTLE_WATCH, MAX_INTERRUPTIONS_PER_LEG);
}

size_t clear_battle_before(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& wild, const RouteStep& step
){
    const std::string_view seen = battle_on_its_way(console, context);
    if (seen.empty()){
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
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    std::optional<Direction> facing;
    if (screen){
        facing = read_player_facing(screen, player);
    }
    console.log(std::string("Route: the player faces ") + (facing ? Pokemon::direction_name(*facing) : "an unknown way") + ".");
    return facing;
}

//  Reads of the facing after a turn tap before one that can't be read is
//  taken as turned, TURN_FACING_REREAD apart: a tap again while facing
//  that way already would take a step.
constexpr size_t TURN_FACING_READS = 3;
constexpr Milliseconds TURN_FACING_REREAD(100);

void turn_to(ConsoleHandle& console, ProControllerContext& context, Direction direction, PlayerGender player){
    //  A tap while the player faces `direction` already takes a step, so the
    //  screen is read first. A trainer's battle leaves the player facing the
    //  way they stepped into the line, not the trainer (pokefirered
    //  trainer_see.c, TrainerSeeFunc_PrepareToEngage): 10.05's turn tap west
    //  after Emilia stepped, and "west 4 (open)" ended on Pokemon Tower 6F
    //  (4, 8), a tile long (console B, 2026-11-12 04:39).
    context.wait_for_all_requests();
    if (const VideoSnapshot screen = console.video().snapshot(); screen && read_player_facing(screen, player) == direction){
        console.log(std::format("Route: the player faces {} already: no turn tap.", Pokemon::direction_name(direction)));
        return;
    }
    for (size_t tap = 1; tap <= MAX_TURN_TAPS; tap++){
        pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
        std::optional<Direction> facing;
        for (size_t read = 1; read <= TURN_FACING_READS && !facing; read++){
            if (read > 1){
                pbf_wait(context, TURN_FACING_REREAD);
            }
            context.wait_for_all_requests();
            VideoSnapshot screen = console.video().snapshot();
            facing = screen ? read_player_facing(screen, player) : std::nullopt;
        }
        if (!facing){
            console.log(std::format(
                "Route: after turn tap {} {}, the facing can't be read: taken as turned.",
                tap, Pokemon::direction_name(direction)
            ), COLOR_ORANGE);
            return;
        }
        if (*facing == direction){
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

namespace{

//  B through the text boxes that are open, at most MAX_TEXT_BOXES of them.
void clear_text(ConsoleHandle& console, ProControllerContext& context){
    for (int c = 0; c < MAX_TEXT_BOXES; c++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        if (!screen || !WhiteDialogDetector(COLOR_RED).detect(screen)){
            return;
        }
        pbf_press_button(context, BUTTON_B, 160ms, 1000ms);
    }
}

void face_for_a(ProControllerContext& context, Direction direction, bool facing){
    //  An A pressed while the player is still turning is lost.
    if (!facing){
        pbf_press_dpad(context, dpad(direction), FACE_HOLD, FACE_SETTLE);
    }
}

} // namespace

void open_gate(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_gate){
    face_for_a(context, direction, facing_gate);
    pbf_press_button(context, BUTTON_A, 160ms, GATE_MESSAGE_WAIT);
    clear_text(console, context);
    pbf_wait(context, GATE_SETTLE);
    context.wait_for_all_requests();
}

void press_switch(ConsoleHandle& console, ProControllerContext& context, Direction direction, bool facing_statue){
    face_for_a(context, direction, facing_statue);
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    SelectionDialogWatcher prompt(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, SWITCH_PROMPT_TIMEOUT, {prompt}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no prompt facing the statue.",
            console
        );
    }
    //  Yes is selected.
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    clear_text(console, context);
    pbf_wait(context, SWITCH_SETTLE);
    context.wait_for_all_requests();
}

} // namespace OverworldWalkSteps



std::string_view move_speed_name(MoveSpeed speed){
    switch (speed){
    case MoveSpeed::WALK:
        return "walk";
    case MoveSpeed::RUN:
        return "run";
    case MoveSpeed::BIKE:
        return "bike";
    }
    return "?";
}

MoveSpeed on_foot(MoveSpeed speed){
    return speed == MoveSpeed::BIKE ? MoveSpeed::RUN : speed;
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
    context.wait_for_all_requests();
    const VideoSnapshot before = console.video().snapshot();
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
    wait_out_warp(console, context, step, fade, before);
    static_assert(AFTER_WARP_BATTLE_WATCH >= AFTER_WARP);
    check_warp_was_no_battle(console, context, step);
}

} // namespace


void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<Direction> facing,
    const OverworldInterruptSet& interrupts,
    MoveSpeed speed,
    PlayerGender player
){
    if (!facing){
        facing = read_facing(console, context, player);
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
            //  Not on the Bicycle, where the tap is a step.
            if (turns_first(step, speed, facing)){
                turn_to(console, context, step.direction, player);
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
            //  Checked by the facing on screen, like a leg's turn: the game
            //  missed a single tap of "face south" and A found nothing
            //  (console B, 2026-11-11, 09.02 moon_stone).
            turn_to(console, context, step.direction, player);
            break;
        case RouteAction::OPEN_GATE:
            turn_to(console, context, step.direction, player);
            open_gate(console, context, step.direction, true);
            break;
        case RouteAction::PRESS_SWITCH:
            turn_to(console, context, step.direction, player);
            press_switch(console, context, step.direction, true);
            break;
        }
        if (step.action != RouteAction::FACE && step.action != RouteAction::OPEN_GATE && step.action != RouteAction::PRESS_SWITCH){
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

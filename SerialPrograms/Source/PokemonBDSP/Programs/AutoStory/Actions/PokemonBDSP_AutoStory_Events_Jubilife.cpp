/*  BDSP AutoStory - Events: Jubilife City (00.05b, 00.05c, 01.01)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Jubilife beats the story table cannot plan: the talk to a wandering
 *  clown, the check that the Poketch came, and where 00.05c starts.
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteInput.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteTalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteTiming.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_WanderWait.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::TilePos;


namespace{

//  00.05b: a talk to someone who wanders a tile either side of home along a
//  row (the clowns by the Mart and the Poketch Company, WANDER_AROUND with
//  x range 1). Waiting under home alone missed him for a minute (r66: he
//  stood a tile east), so walk the row below his range, a whole D-pad tile
//  at a time, asking north from each tile.
bool ask_north(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::NORTH);
    for (size_t ask = 0; ask < 2; ask++){
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            return true;
        }
    }
    return false;
}
//  A clown's talk is his question, then its Yes/No box: any dialog is not
//  (00.05b, lab-B 2026-11-11: the Poketch Company clown was "answered" in
//  round 5 by another line, and the president gave no Poketch). Press A
//  until the box's cursor shows, then A for Yes and through the coupon.
//  False when the talk ends with no box: someone or something else, or a
//  clown who has given his coupon already.
bool answer_coupon_question(VideoStream& stream, ProControllerContext& context, const std::string& label){
    static const ImageFloatBox YES_NO_ARROW_BOX{0.700, 0.620, 0.070, 0.120};
    SelectionArrowFinder yes_no(stream.overlay(), YES_NO_ARROW_BOX, COLOR_GREEN);
    for (size_t press = 0; press < 6; press++){
        context.wait_for(1200ms);
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && yes_no.detect(*screen.frame)){
            stream.log("[Talk] " + label + ": the coupon question; Yes.");
            pbf_press_button(context, BUTTON_A, 80ms, 0ms);
            finish_dialogue(stream, context, BUTTON_A, label);
            return true;
        }
        if (!dialogue_opens(stream, context, 500ms)){
            stream.log("[Talk] " + label + ": the talk ended with no Yes/No box.", COLOR_ORANGE);
            return false;
        }
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    }
    finish_dialogue(stream, context, BUTTON_B, label);
    stream.log("[Talk] " + label + ": no Yes/No box in 6 presses.", COLOR_ORANGE);
    return false;
}
//  One tile along the row, east (+1) or west (-1). Returns the tiles moved
//  east; fails the step when the walk left the row (wander_row_step()).
int step_along(VideoStream& stream, ProControllerContext& context, int east, const std::string& label){
    route_turn(stream, context, east > 0 ? Direction::EAST : Direction::WEST);
    const WalkResult result = walk_tiles_dpad(stream, context, run_from_wild_battle, {east > 0 ? 1.0 : -1.0, 0}, 1, label);
    const std::optional<int> moved = wander_row_step(result, east);
    if (!moved){
        fail_step(stream, std::format("{}: {} instead of a step along the row.", label, walk_stop_name(result.stop)));
    }
    return *moved;
}
//  Walks back below his home and faces north.
void back_below_home(VideoStream& stream, ProControllerContext& context, int at, const std::string& name){
    for (size_t tries = 0; at != 0 && tries < 4; tries++){
        at += step_along(stream, context, at > 0 ? -1 : 1, "Back below " + name + "'s home");
    }
    if (at != 0){
        fail_step(stream, std::format("{}: still {} tiles east of his home after 4 steps back.", name, at));
    }
    route_turn(stream, context, Direction::NORTH);
}
//  The talk to a wanderer from the step's declared pose, below his home.
//  With "again" (00.05c), a talk with no question counts too: he may have
//  given his coupon before, and the Poketch check after the president
//  catches a coupon that is still missing.
Pokemon::Outcome talk_to_wanderer(StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep& step){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    const bool again = Pokemon::story_arg<bool>(step.args, "again");
    std::optional<std::pair<std::string, TilePos>> home;
    if (step.pose_after){
        const Pokemon::Pose below = Pokemon::pose_of(*step.pose_after);
        const std::vector<TilePos> tiles = Pokemon::tiles_of(below);
        if (!tiles.empty()){
            home.emplace(below.map, tiles[0]);
        }
    }
    const std::string label = "Along the row below " + step.name;
    //  Home, east, west, west, east: each move one tile on the D-pad.
    const int moves[] = {0, 1, -1, -1, 1};
    int at = 0;
    for (size_t round = 0; round < 6; round++){
        for (int move : moves){
            if (move != 0){
                at += step_along(stream, context, move, label);
            }
            if (home && asks_someone_else(story_map(home->first), {home->second.x + at, home->second.y}, home->second.next(Direction::NORTH))){
                stream.log(std::format("[Talk] {}: not asking from {} tiles east of home: a sign or someone else is north.", step.name, at));
                continue;
            }
            if (!ask_north(stream, context)){
                continue;
            }
            const bool coupon = answer_coupon_question(stream, context, step.name);
            if (!coupon && !again){
                continue;
            }
            stream.log(std::format("[Talk] {}: {} in round {}, {} tiles east of home.",
                step.name, coupon ? "the coupon" : "talked, no question (his coupon given before)", round + 1, at));
            back_below_home(stream, context, at, step.name);
            return Pokemon::outcome_ok();
        }
    }
    fail_step(stream, step.name + ": no coupon question from any tile below his range.");
}

//  The console's side of a wait beside a wanderer: a D-pad tap a step,
//  counted, and A; the talk counts when his Yes/No box opens (or, with
//  "again", when any talk opens).
class ConsoleWanderWait : public WanderWaitBody{
public:
    ConsoleWanderWait(VideoStream& stream, ProControllerContext& context, std::string name, bool again)
        : m_stream(stream)
        , m_context(context)
        , m_name(std::move(name))
        , m_again(again)
    {}
    virtual bool step(Direction direction) override{
        const std::string label = "Waiting for " + m_name;
        route_turn(m_stream, m_context, direction);
        const WalkResult result = walk_tiles_dpad(m_stream, m_context, run_from_wild_battle, route_stick(direction), 1, label);
        const std::optional<int> moved = wander_row_step(result, 1);
        if (!moved){
            fail_step(m_stream, std::format("{}: {} instead of a one-tile step.", label, walk_stop_name(result.stop)));
        }
        return *moved != 0;
    }
    virtual void face(Direction direction) override{
        route_turn(m_stream, m_context, direction);
    }
    virtual bool ask() override{
        pbf_press_button(m_context, BUTTON_A, 80ms, 0ms);
        m_context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(m_stream, m_context, 1500ms, {{dialog}}) != 0){
            return false;
        }
        return answer_coupon_question(m_stream, m_context, m_name) || m_again;
    }
    virtual void wait(Milliseconds time) override{
        m_context.wait_for(time);
    }

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    std::string m_name;
    bool m_again;
};
//  The talk to a wanderer by waiting for him on the step's "stand" tiles,
//  then on its "far_stand" tiles (his box's other end), in turn, from the
//  tile the step before left us on. It ends on its declared tile, against
//  a wall the declared way and one at the stand column's back (away from
//  his box): both taps must not move.
Pokemon::Outcome wait_for_wanderer(StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep& step){
    VideoStream& stream = executor.stream();
    const Pokemon::GridPose& stand = Pokemon::story_arg<Pokemon::GridPose>(step.args, "stand");
    const Pokemon::GridPose& far_stand = Pokemon::story_arg<Pokemon::GridPose>(step.args, "far_stand");
    const std::vector<TilePos> start = Pokemon::tiles_of(state.pose);
    if (!step.pose_after || start.size() != 1 || stand.tiles.empty() || !stand.facing){
        fail_step(stream, step.name + ": needs one start tile, stand tiles and their facing, and a declared end.");
    }
    if (!far_stand.tiles.empty() && !far_stand.facing){
        fail_step(stream, step.name + ": its far stand tiles need a facing.");
    }
    const Pokemon::Pose exit = Pokemon::pose_of(*step.pose_after);
    const std::vector<TilePos> exit_tiles = Pokemon::tiles_of(exit);
    const std::optional<Direction> exit_wall = Pokemon::facing_of(exit);
    if (exit_tiles.size() != 1 || !exit_wall){
        fail_step(stream, step.name + ": its declared end needs one tile and a facing.");
    }
    //  The column's back: the way away from his box.
    const Direction back = Pokemon::ALL_DIRECTIONS[((size_t)*stand.facing + 2) % 4];
    WanderWaitPlan plan{.sides = {{.stands = stand.tiles, .facing = *stand.facing}}, .exit = exit_tiles[0], .exit_walls = {*exit_wall, back}};
    if (!far_stand.tiles.empty()){
        plan.sides.push_back({.stands = far_stand.tiles, .facing = *far_stand.facing});
    }
    ConsoleWanderWait body(stream, executor.context(), step.name, Pokemon::story_arg<bool>(step.args, "again"));
    const WanderWaitResult result = wait_beside_wanderer(body, start[0], plan);
    switch (result.end){
    case WanderWaitEnd::TALKED:
        stream.log(std::format("[Talk] {}: talked from {}; now on {}.", step.name, Pokemon::to_string(result.talked), Pokemon::to_string(result.at)));
        return Pokemon::outcome_ok();
    case WanderWaitEnd::NO_TALK:
        fail_step(stream, std::format("{}: no talk from any stand tile; on {}.", step.name, Pokemon::to_string(result.at)));
    case WanderWaitEnd::STUCK:
        fail_step(stream, std::format("{}: a step on the way stayed blocked at {}.", step.name, Pokemon::to_string(result.at)));
    case WanderWaitEnd::LOST:
        fail_step(stream, std::format("{}: a step from {} moved to {}: not on the exit.", step.name, Pokemon::to_string(exit_tiles[0]), Pokemon::to_string(result.at)));
    }
    fail_step(stream, step.name + ": unknown wait end.");
}

//  00.05c's end and 01.01's start: the Poketch shows on the screen's right
//  (R toggles it). Without it, Jubilife's east-exit guard (the coord event
//  on (60, 21..24)) asks about the Poketch campaign and turns us back.
void check_poketch(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    if (!ensure_poketch_visible(stream, context)){
        fail_step(stream, "No Poketch: the president gave none (a clown's coupon is missing?). "
            "Jubilife's east-exit guard stops anyone without one: rerun 00.05c.");
    }
    stream.log("[Talk] The Poketch is on the screen.");
}

//  00.05c starts against the Poketch Company's sign (14, 19) facing west
//  (00.05b's end) or on the street east, (58, 22) facing east (its own end
//  and 01.01's start, when a run gave no Poketch). A step west tells them
//  apart: the sign blocks it, the street does not.
Pokemon::Outcome jubilife_start_probe(StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    route_turn(stream, context, Direction::WEST);
    const WalkResult result = walk_tiles_dpad(stream, context, run_from_wild_battle, {-1.0, 0}, 1, "00.05c start probe");
    const std::optional<int> moved = wander_row_step(result, -1);
    if (!moved){
        fail_step(stream, std::format("00.05c start probe: {} instead of a step west.", walk_stop_name(result.stop)));
    }
    const TilePos at = *moved == 0 ? jubilife_start_tiles().first : jubilife_start_tiles().second.next(Direction::WEST);
    stream.log(std::format("[Talk] 00.05c starts {}: on {}.", *moved == 0 ? "at the Poketch Company's sign" : "on the street east", Pokemon::to_string(at)));
    return Pokemon::outcome_ok(Pokemon::tile_pose("JubilifeCity", {at}, Direction::WEST));
}

} // namespace


std::optional<int> wander_row_step(const WalkResult& result, int east){
    switch (result.stop){
    case WalkStop::TRANSITION:
    case WalkStop::DIALOG:
        return std::nullopt;
    default:
        return result.moved_chunks > 0 ? east : 0;
    }
}

bool asks_someone_else(const Pokemon::MapData& map, TilePos stand, TilePos wanderer_home){
    const TilePos north = stand.next(Direction::NORTH);
    if (north == wanderer_home || !map.tiles.contains(north)){
        return false;
    }
    if (map.tiles.at(north) == Pokemon::TileKind::SIGN){
        return true;
    }
    for (const Pokemon::MapObject& object : map.objects){
        if (!object.wander && object.x == north.x && object.y == north.y){
            return true;
        }
    }
    return false;
}

Pokemon::TileBox jubilife_mart_clown_box(){
    return {.x0 = 54, .y0 = 32, .x1 = 56, .y1 = 34};
}

std::pair<TilePos, TilePos> jubilife_start_tiles(){
    return {{14, 19}, {58, 22}};
}

void add_jubilife_events(StoryEvents& events){
    events.emplace("talk_to_wanderer", talk_to_wanderer);
    events.emplace("wait_for_wanderer", wait_for_wanderer);
    events.emplace("check_poketch", console_event(check_poketch));
    events.emplace("jubilife_start_probe", jubilife_start_probe);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

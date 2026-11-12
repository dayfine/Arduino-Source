/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"
#include "PokemonBDSP_AutoStory_SimWanderWait.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::story_arg;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


namespace detail{
ImageStats sim_screen(double brightness, double stddev){
    return ImageStats(FloatPixel(brightness, brightness, brightness), FloatPixel(stddev, stddev, stddev), 1);
}

std::vector<Pokemon::TransitionSample> sim_fade_frames(double floor, double fade, bool hold_seen){
    constexpr Milliseconds FRAME = 33ms;
    //  The fall and the rise: halfway, in brightness and in texture.
    const ImageStats between = sim_screen((floor + fade) / 2, (SIM_FLOOR_STDDEV + SIM_FADE_STDDEV) / 2);
    std::vector<ImageStats> looks{sim_screen(floor, SIM_FLOOR_STDDEV), between};
    if (hold_seen){
        for (size_t i = 0; i < 3; i++){
            looks.push_back(sim_screen(fade, SIM_FADE_STDDEV));
        }
        looks.push_back(between);
    }
    looks.push_back(sim_screen(floor, SIM_FLOOR_STDDEV));
    std::vector<Pokemon::TransitionSample> frames;
    for (size_t i = 0; i < looks.size(); i++){
        frames.push_back({.time = WallClock() + FRAME * (int64_t)i, .middle = looks[i]});
    }
    return frames;
}

WarpFade sim_watch_fade(const WarpFadeRule& rule, double floor, double fade, bool hold_seen){
    WarpFadeWatch watch(rule, [](const Pokemon::TransitionSample&, const Pokemon::TransitionSample&){
        return Pokemon::SceneChange::DIFFERENT;
    });
    for (const Pokemon::TransitionSample& frame : sim_fade_frames(floor, fade, hold_seen)){
        const WarpFade seen = watch.push(frame);
        if (seen != WarpFade::NONE){
            return seen;
        }
    }
    return WarpFade::NONE;
}

SimBeat sim_line(std::string what){
    return {.what = std::move(what), .line = true};
}
SimBeat sim_pause(std::string what, Milliseconds length, bool moving){
    return {.what = std::move(what), .length = length, .moving = moving};
}
SimBeat sim_walk_past(std::string what, Milliseconds length){
    return {.what = std::move(what), .length = length, .moving = true, .at_player = true};
}
SimBeat sim_fade(std::string what, Milliseconds length, WarpFade fade){
    return {.what = std::move(what), .length = length, .overworld = true, .moving = false, .fade = fade};
}

std::string play_scene(const SimScene& scene, const TalkOverRule& rule){
    const Milliseconds press = TALK_OVER_HOLD + talk_over_release(rule);
    TalkOver judge(rule);
    const WallClock start = WallClock();
    WallClock now = start;
    WallClock beat_start = start;
    size_t beat = 0;
    const auto at = [&]{
        return std::format("{} ms in, at beat {} of {}", std::chrono::duration_cast<Milliseconds>(now - start).count(), beat + 1, scene.size());
    };
    while (now - start < TALK_OVER_LIMIT){
        while (beat < scene.size() && !scene[beat].line && now - beat_start >= scene[beat].length){
            beat_start += scene[beat].length;
            beat++;
        }
        Pokemon::ScreenEvents look{Pokemon::ScreenEvent::PLAYER_CAN_MOVE, Pokemon::ScreenEvent::SCREEN_STILL, Pokemon::ScreenEvent::PLAYER_STILL};
        if (beat < scene.size()){
            const SimBeat& shown = scene[beat];
            look = {};
            look.set(Pokemon::ScreenEvent::DIALOG_BOX, shown.line);
            look.set(Pokemon::ScreenEvent::PLAYER_CAN_MOVE, shown.overworld);
            look.set(Pokemon::ScreenEvent::SCREEN_STILL, !shown.moving);
            look.set(Pokemon::ScreenEvent::PLAYER_STILL, !shown.at_player);

            look.merge(Pokemon::fade_events(shown.fade));
        }
        switch (judge.look(look, now)){
        case SceneMove::DONE:
            return beat < scene.size() ? std::format("judged over {} ({})", at(), scene[beat].what) : "";
        case SceneMove::BATTLE:
            return "read a battle " + at();
        case SceneMove::NEXT:
        case SceneMove::B:
            //  Either press closes a line; nothing else answers one.
            if (beat < scene.size() && scene[beat].line){
                beat++;
                beat_start = now + press;
            }
            break;
        }
        now += press;
    }
    return "never judged over: " + at() + ", " + judge.describe();
}
} // namespace detail

//  Plays the step's scene knob through TalkOver if it has one. Without, a
//  scene may take us to another map (Mom walks us out, 00.04): with a
//  dialogue open the stub scene plays it. Otherwise only a door fades.
void SimStoryStepIO::settle(Button, const std::string& label, SettleKind kind, const std::string&){
    const bool fade = kind == SettleKind::AFTER_WARP;
    m_misses.fading_in = false;
    const auto scene = m_knobs.scenes.find(m_step);
    if (scene != m_knobs.scenes.end()){
        m_misses.fade_unseen = false;
        m_sim.end_dialogue();
        const std::string early = play_scene(scene->second, {.warp = fade, .door = kind == SettleKind::DOOR});
        if (!early.empty()){
            m_route.note(label + ": talk-over " + early + ".", RouteNote::FAILURE);
        }
        return;
    }
    const bool scene_fades = m_sim.player().dialog && m_far_side();
    if (fade && ((!m_sim.player().warped && !scene_fades) || m_misses.fade_unseen)){
        m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
    }
    m_misses.fade_unseen = false;
    m_sim.end_dialogue();
}

SimStoryStepIO::FadeShown SimStoryStepIO::next_fade(){
    const size_t fade = m_misses.fades++;
    const FadeShown shown = m_misses.fade == fade ? FadeShown::UNSEEN : m_misses.hold == fade ? FadeShown::HOLD_UNSEEN : FadeShown::SEEN;
    m_misses.missed |= shown != FadeShown::SEEN;
    return shown;
}

WarpFade SimStoryStepIO::watched_fade(const WarpFadeRule& rule, double fade){
    const FadeShown shown = next_fade();
    return shown == FadeShown::UNSEEN ? WarpFade::NONE : sim_watch_fade(rule, floor_here(), fade, shown == FadeShown::SEEN);
}

//  A door's fade through the program's door watch: a hold missed shows at
//  the scene after it. A fade missed whole, the run goes on holding and
//  sees the far map's `banner` show after it, if it has one.
WarpSeen SimStoryStepIO::door_fade(const std::string& banner){
    const WarpFade fade = watched_fade(DOOR_FADE_RULE, SIM_DOOR_FADE);
    if (fade != WarpFade::NONE){
        return {.fade = fade};
    }
    m_misses.fade_unseen = true;
    return {.banner = !banner.empty() && !banner_missed() ? banner : ""};
}

//  A hold until something stops us. A door stops it at the fade, which
//  fails the step; a fade missed lets the hold go on past the mouth (r81's
//  timed walk south out of the Ravaged Path, onto Route 204's cliff). The
//  walk's watchers take uniform frames only, so a fade whose hold they
//  did not see is missed too.
void SimStoryStepIO::walk_until_stopped(Direction direction, size_t tiles, const std::string& label){
    m_route.check_fade_in(label);
    m_sim.tap(direction, ROUTE_TURN_TAP, false);
    Milliseconds limit = ROUTE_START_DELAY + 800ms + route_ms_per_tile(direction) * (int64_t)(2 * tiles);
    SimMove move = m_sim.hold(direction, limit, SimPace::WALK);
    if (move.stop == SimStop::WARP && next_fade() != FadeShown::SEEN && m_far_side()){
        limit -= std::min(limit, move.time);
        move = m_sim.hold(direction, limit, SimPace::WALK);
    }
    if (move.stop == SimStop::DIALOG || move.stop == SimStop::WARP){
        m_route.note(label + ": " + std::string(sim_stop_name(move.stop)) + " instead of a stop.", RouteNote::FAILURE);
    }
}

//  The D-pad, a tile at a time, until `map`'s banner reads; through a door
//  the walk goes on beyond it (the fade eats no tiles of a hold).
bool SimStoryStepIO::run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string& label){
    m_route.check_fade_in(label);
    const bool miss = banner_missed();
    size_t on = 0;
    for (Milliseconds ran = 0ms; ran < limit; ran += ROUTE_RUN_MS_PER_TILE_ACROSS){
        SimMove move = m_sim.tap(direction, ROUTE_MS_PER_TILE_ACROSS, false);
        const bool crossed = move.stop == SimStop::WARP && m_far_side();
        const bool stopped = !crossed && (move.distance == 0 || move.stop != SimStop::DONE);
        on = on_map(map) ? on + 1 : 0;
        if (on > 0 && (on > SIM_BANNER_TILES || stopped) && !miss){
            return true;
        }
        if (stopped){
            return false;
        }
    }
    return false;
}

double SimStoryStepIO::floor_here() const{
    for (const auto& [map, brightness] : m_knobs.floor){
        if (on_map(map)){
            return brightness;
        }
    }
    return SIM_FLOOR;
}

//  D-pad taps, each looked at, until the fade or the banner (the console's
//  ConsoleStoryStepIO::tap_to_banner()): none if the banner shows before
//  the first. Each look stands still, so a banner on the far side reads at
//  once. A fade the program's tap watch (TAP_FADE_RULE) sees ends the taps,
//  its hold seen or not; with the fade missed whole the look still sees the
//  banner.
std::optional<size_t> SimStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    const std::optional<size_t> taps = taps_to_banner(direction, map, max_taps);
    //  Through: the far side fades in as the step ends.
    m_misses.fading_in = taps.has_value();
    return taps;
}
std::optional<size_t> SimStoryStepIO::taps_to_banner(Direction direction, const std::string& map, size_t max_taps){
    if (m_misses.through == m_misses.tap_steps++){
        m_misses.missed = true;
        if (m_far_side() && m_misses.through_banner){
            return 0;
        }
    }
    if (std::exchange(m_misses.banner_up, "") == map){
        return 0;
    }
    m_route.check_fade_in(std::format("{}: taps toward {}", m_step, map));
    bool crossed = false;
    for (size_t tap = 1; tap <= max_taps; tap++){
        const SimMove move = m_sim.tap(direction, 160ms, false);
        if (move.stop == SimStop::DIALOG){
            return std::nullopt;
        }
        WarpFade fade = WarpFade::NONE;
        if (move.stop == SimStop::WARP){
            const auto into = m_knobs.fade_into.find(map);
            fade = watched_fade(TAP_FADE_RULE, into == m_knobs.fade_into.end() ? SIM_DOOR_FADE : into->second);
            crossed = m_far_side();
        }
        const bool banner = crossed && on_map(map) && !banner_missed();
        if (banner || fade != WarpFade::NONE){
            return tap;
        }
    }
    return std::nullopt;
}

void SimKnobRouteIO::check_fade_in(const std::string& label){
    if (m_knobs.slow_fade_in && std::exchange(m_misses.fading_in, false)){
        note(std::format("{}: moved while the last warp still faded in (no settle after it).", label), RouteNote::FAILURE);
    }
}

WalkResult SimKnobRouteIO::walk_to_wall(const RouteWallWalk& walk){
    check_fade_in(walk.label);
    WalkResult result = GridSimRouteIO::walk_to_wall(walk);
    const TilePos ahead = nearest_tile(m_grid.player().pos).next(walk.direction);
    const std::vector<SimWanderer>& people = sim_wanderers(m_grid);
    const bool pushing = std::any_of(people.begin(), people.end(), [&](const SimWanderer& person){
        return person.pos == ahead;
    });
    if (result.stop == WalkStop::BLOCKED && pushing){
        result.walked += m_knobs.pushing_someone;
    }
    return result;
}

WalkResult SimKnobRouteIO::walk_taps(Direction direction, size_t tiles, const std::string& label){
    check_fade_in(label);
    const WalkResult result = GridSimRouteIO::walk_taps(direction, tiles, label);
    if (result.stop != WalkStop::LIMIT || m_slid_steps.contains(m_step)){
        return result;
    }
    for (const SimSlide& item : m_knobs.slides){
        if (item.step == m_step && item.direction == direction){
            m_slid_steps.insert(m_step);
            return slide_on(item, result);
        }
    }
    return result;
}

//  The taps past the leg's end. Onto a warp the walk sees the fade as the
//  program's fade_of() judges it, the walk's watchers' test: a fade ends
//  it (TRANSITION), a white one too, as no battle follows it. The walk ends
//  at the fade, before the banner shows (about 1.5 s after it), so it
//  reads none.
WalkResult SimKnobRouteIO::slide_on(const SimSlide& slide, WalkResult result){
    m_misses.slid = true;
    for (size_t tap = 0; tap < slide.tiles; tap++){
        const SimMove move = m_grid.tap(slide.direction, ROUTE_MS_PER_TILE_ACROSS, false);
        if (move.stop != SimStop::WARP){
            note(std::format("[Sim] slid a tile on to {}.", to_string(m_grid.player().pos)), RouteNote::LOG);
            continue;
        }
        note("[Sim] slid onto a warp to " + Pokemon::to_string(slide.far) + ".", RouteNote::LOG);
        m_cross(slide.far);
        m_misses.banner_up = slide.banner_up ? slide.far.map : "";
        const double brightness = slide.fade == SimFade::BLACK ? SIM_DOOR_FADE : slide.fade == SimFade::DIM ? SIM_DIM_FADE : SIM_WHITE_FADE;
        result.warp.fade = fade_of(sim_screen(brightness, SIM_FADE_STDDEV));
        if (result.warp.fade != WarpFade::NONE){
            result.stop = WalkStop::TRANSITION;
        }
        return result;
    }
    return result;
}

bool SimKnobRouteIO::dialog_within(Milliseconds wait, Color color){
    const auto late = m_knobs.first_line.find(m_step);
    if (late != m_knobs.first_line.end() && late->second > wait){
        note(std::format("[Sim] {}: the first line comes after {} ms, past the {} ms wait.", m_step, late->second.count(), wait.count()), RouteNote::LOG);
        return false;
    }
    return GridSimRouteIO::dialog_within(wait, color);
}

namespace{

//  Someone the maps do not have, on `tile` of `map`, talked to or walked round:
//  from row `row` on, until row `until_row`'s step `until` has run if it is set.
struct SimPerson{
    std::string name;
    std::string map;
    TilePos tile;
    std::string row;
    std::string until_row = {};
    std::string until = {};
};
std::vector<SimPerson> sim_people(){
    return {
        //  Mom stands a row above the decomp's (7, 8) from 00.04 on (r43).
        {.name = "mom", .map = "PlayerHouse1F", .tile = {7, 7}, .row = "00.04"},
        //  Barry in the Gym's door, from the welcome (lab-B 2026-11-08) to his lines (r101, r104).
        {.name = "barry", .map = "OreburghCity", .tile = {26, 21}, .row = "01.02b", .until_row = "01.02b", .until = "barrys_lines"},
    };
}

//  A house's stairs, entered by a step `side` from the map's "stairs"
//  point into what the map draws as a wall (the table's 00.01: r8 and r9
//  for ours, r11 for Barry's).
struct SimStairs{
    std::string map;
    Direction side;
};
std::vector<SimStairs> sim_stairs(){
    return {
        {.map = "PlayerHouse2F", .side = Direction::EAST},
        {.map = "RivalHouse1F", .side = Direction::EAST},
        {.map = "RivalHouse2F", .side = Direction::WEST},
    };
}

} // namespace

//  The world the player walks on for `map`: the planner's, with what the
//  console showed that the maps lack. Nothing for a room with no map.
const StoryWorld* SimExecutor::sim_world(const std::string& map) const{
    const StoryWorld* world = story_plan_world(map, map);
    std::vector<SimPerson> here;
    std::string who;
    for (const SimPerson& person : sim_people()){
        const bool now = m_row >= person.row && (person.until.empty() || m_row < person.until_row ||
            (m_row == person.until_row && !m_ran.contains(person.until)));
        if (world != nullptr && now && story_plan_world(person.map, person.map) == world){
            here.push_back(person);
            who += person.name + ";";
        }
    }
    if (here.empty()){
        return world;
    }
    std::unique_ptr<StoryWorld>& peopled = m_peopled[{world, who}];
    if (!peopled){
        peopled = std::make_unique<StoryWorld>(*world);
        for (const SimPerson& person : here){
            peopled->block(story_to_world(*world, person.map, person.tile));
        }
    }
    return peopled.get();
}

SimWorld SimExecutor::setup(const StoryWorld* world) const{
    //  B runs once the shoes' scene has given them: the scene ends at home.
    SimWorld out{.world = world, .trainers = row_trainers(*world, m_row), .can_run = m_walks.can_run()};
    for (const SimStairs& stairs : sim_stairs()){
        if (sim_world(stairs.map) == world){
            out.doors.push_back(story_to_world(*world, stairs.map, map_point(stairs.map, "stairs").next(stairs.side)));
        }
    }
    for (const RowTrigger& armed : m_armed){
        if (sim_world(armed.map) != world){
            continue;
        }
        SimTrigger trigger{.name = armed.trigger.name, .tiles = {}};
        for (const TilePos& pos : armed.trigger.tiles){
            trigger.tiles.push_back(story_to_world(*world, armed.map, pos));
        }
        out.triggers.push_back(std::move(trigger));
    }
    out.wanderers = wanderers(*world);
    return out;
}

namespace{
bool on_world(const StoryWorld& world, const std::string& map){
    try{
        story_to_world(world, map, {0, 0});
        return true;
    }catch (const InternalProgramError&){
        return false;
    }
}
} // namespace

//  Everyone with a wander box on `world`'s maps, walking it from their tile
//  (SimWorldKnobs::wander_seed), each with their own seed and start time.
std::vector<SimWanderer> SimExecutor::wanderers(const StoryWorld& world) const{
    std::vector<SimWanderer> out;
    if (!m_knobs.wander_seed && m_knobs.standing.empty()){
        return out;
    }
    for (const auto& [map, json] : bdsp_map_json()){
        if (!on_world(world, map)){
            continue;
        }
        for (const auto& object : story_map(map).objects){
            const auto stands = m_knobs.standing.find(object.name);
            if (!object.wander || (!m_knobs.wander_seed && stands == m_knobs.standing.end())){
                continue;
            }
            const uint64_t seed = m_knobs.wander_seed.value_or(0) * 1000003 + out.size() * 7919 + (uint64_t)(object.x * 31 + object.y);
            const TilePos low = story_to_world(world, map, {object.wander->x0, object.wander->y0});
            const TilePos high = story_to_world(world, map, {object.wander->x1, object.wander->y1});
            const bool still = stands != m_knobs.standing.end();
            out.push_back({
                .name = object.name,
                .box = {.x0 = low.x, .y0 = low.y, .x1 = high.x, .y1 = high.y},
                .pos = story_to_world(world, map, still ? stands->second : TilePos{object.x, object.y}),
                .seed = seed,
                .step_interval = still ? Milliseconds(0) : Milliseconds(2000),
                .elapsed = Milliseconds(0),
                .time_offset = still ? Milliseconds(0) : Milliseconds((int64_t)(seed % 2000)),
            });
        }
    }
    return out;
}

//  Arms the triggers of this row that start after `after`. A variable holds
//  one value: the triggers armed for its value before are disarmed (Route
//  201's row 19 pulls back only after Barry's warning, not in 00.03, where
//  the variable has moved on).
void SimExecutor::arm(const std::string& after){
    std::vector<RowTrigger> now;
    for (const RowTrigger& trigger : story_triggers()){
        if (trigger.row == m_row && trigger.after == after){
            now.push_back(trigger);
        }
    }
    for (const RowTrigger& trigger : now){
        std::erase_if(m_armed, [&](const RowTrigger& old){
            return old.var == trigger.var;
        });
    }
    m_armed.insert(m_armed.end(), now.begin(), now.end());
    const size_t armed = now.size();
    const SimPlayer& player = m_sim.player();
    if (armed > 0 && m_placed && on_grid(player.pos) && !player.dialog){
        m_sim.place(setup(&m_sim.world()), nearest_tile(player.pos), player.facing);
    }
}

//  Puts the player on `pose`, on the world `on` if given, unless they stand
//  on one of its tiles already. Returns whether the pose is on a map the
//  simulation has.
bool SimExecutor::sync(const Pose& pose, const StoryWorld* on){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const StoryWorld* world = on != nullptr ? on : pose.map.empty() ? nullptr : sim_world(pose.map);
    if (world == nullptr || tiles.empty()){
        return false;
    }
    //  The program thinks we are on `pose`; we walk on from where the slide left us.
    if ((m_astray || m_misplaced) && m_placed){
        return true;
    }
    const SimPlayer& player = m_sim.player();
    const std::optional<TilePos> local = !m_placed || &m_sim.world() != world
        ? std::nullopt : story_to_local(*world, pose.map, nearest_tile(player.pos));
    if (local && on_grid(player.pos) && std::find(tiles.begin(), tiles.end(), *local) != tiles.end()){
        return true;
    }
    const std::optional<Pose>& start_at = m_knobs.start_at;
    if (!m_placed && start_at && start_at->map == pose.map && !Pokemon::tiles_of(*start_at).empty()){
        m_sim.place(setup(world), story_to_world(*world, pose.map, Pokemon::tiles_of(*start_at).front()), Pokemon::facing_of(pose));
        m_placed = true;
        m_misplaced = true;
        m_logger.log(std::format("[{}] the save leaves the player on {}, off the declared {}.", m_row, to_string(*start_at), to_string(pose)));
        return true;
    }
    m_sim.place(setup(world), story_to_world(*world, pose.map, tiles.front()), Pokemon::facing_of(pose));
    m_placed = true;
    return true;
}

void SimExecutor::start_scene_at_end(const StoryStep& step){
    std::optional<Pose> end;
    if (step.kind == StoryStepKind::ROUTE && step.args.contains("target")){
        end = Pokemon::pose_of(story_arg<GridPose>(step.args, "target"));
    }else if (step.pose_after){
        end = Pokemon::pose_of(*step.pose_after);
    }
    if (!end){
        return;
    }
    SimTrigger trigger{.name = "early scene after " + step.name, .tiles = {}};
    for (const TilePos& tile : Pokemon::tiles_of(*end)){
        try{
            trigger.tiles.push_back(story_to_world(m_sim.world(), end->map, tile));
        }catch (const InternalProgramError&){}
    }
    //  It is the scene of the trigger next to it, which then does not fire
    //  again (00.05b's president, a row up).
    const auto next_to = [&](const TilePos& tile){
        return std::ranges::any_of(trigger.tiles, [&](const TilePos& early){
            return std::abs(early.x - tile.x) + std::abs(early.y - tile.y) == 1;
        });
    };
    for (const SimTrigger& other : sim_triggers(m_sim)){
        if (std::ranges::any_of(other.tiles, next_to)){
            trigger.spends.push_back(other.name);
        }
    }
    m_logger.log(std::format("[{}] {}: the next scene starts on its end, {}.", m_row, step.name, Pokemon::to_string(*end)));
    sim_add_trigger(m_sim, std::move(trigger));
}

void SimExecutor::land(const std::string& step){
    count_coupons(step);
    const auto lands = m_knobs.lands.find(step);
    if (lands == m_knobs.lands.end()){
        return;
    }
    m_misplaced = false;
    const SimPlayer& player = m_sim.player();
    const Pose& pose = lands->second;
    if (sync(pose)){
        m_misplaced = true;
        m_logger.log(std::format("[{}] {}: the player lands on {}, off the table's pose.", m_row, step, to_string(player.pos)));
    }
}

//  Through a door or a cave mouth onto the step's far side, the player's
//  facing kept: a hold goes on from there.
bool SimExecutor::cross(){
    return m_far && cross_to(*m_far);
}
bool SimExecutor::cross_to(const Pose& far){
    const StoryWorld* world = sim_world(far.map);
    const std::vector<TilePos> tiles = Pokemon::tiles_of(far);
    if (world == nullptr || tiles.empty()){
        return false;
    }
    m_sim.place(setup(world), story_to_world(*world, far.map, tiles.front()), std::nullopt);
    return true;
}

//  Why the player does not stand on one of `pose`'s tiles, or "" if they
//  do or the simulation cannot tell (no map, or not placed yet).
std::string SimExecutor::off_pose(const Pose& pose) const{
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    if (!m_placed || tiles.empty() || sim_world(pose.map) == nullptr){
        return "";
    }
    const SimPlayer& player = m_sim.player();
    const TilePos at = nearest_tile(player.pos);
    //  In world tiles: a pose a leg worked out from the map it started on
    //  may lie past that map's edge, on the next map (Twinleaf's exit).
    //  story_to_world() throws for a map not on the player's world.
    try{
        const auto there = [&](const TilePos& tile){ return story_to_world(m_sim.world(), pose.map, tile) == at; };
        if (on_grid(player.pos) && std::ranges::any_of(tiles, there)){
            return "";
        }
    }catch (const InternalProgramError&){}
    return std::format("ended at {} ({}), not on {}.", to_string(player.pos), describe(m_sim.world(), at), Pokemon::to_string(pose));
}

//  The program's run into the Center: past its door, the inside shows, a missed fade or not.
Outcome SimExecutor::heal(RunState& state, const StoryStep& step){
    if (!sync(state.pose)){
        return Pokemon::outcome_ok();
    }
    run_into_center(m_io, m_walks.can_run(), [this]{
        m_misses.fade_unseen = false;
        return m_sim.player().warped.has_value();
    }, [&]{
        throw GridSimFailure{step.name + ": no door north of " + to_string(m_sim.player().pos)};
    }, step.name);
    if (m_misses.fading_in){
        return Pokemon::outcome_failed("heal", step.name + ": walked to the nurse while the Center was still fading in.");
    }
    return Pokemon::outcome_ok();
}

//  The battle is not played. The speeches after it are, when the knobs
//  script them, as the console's battle ends: confirm_battle_overworld()'s
//  talk-over.
Outcome SimExecutor::battle(RunState&, const StoryStep& step){
    const auto scene = m_knobs.scenes.find(step.name);
    if (scene != m_knobs.scenes.end()){
        const std::string early = play_scene(scene->second, {});
        if (!early.empty()){
            return Pokemon::outcome_failed("talk_over", step.name + ": " + early);
        }
    }
    return stub();
}

//  Why a walking step did not end where the table says, or "". A step
//  through a door the simulation did not cross is not checked: the sim's
//  maps have no warps' destinations.
std::string SimExecutor::check(const StoryStep& step, const Outcome& outcome){
    const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP;
    const std::optional<Pose> expected = step.pose_after ? std::optional<Pose>(Pokemon::pose_of(*step.pose_after)) : outcome.pose;
    if (!walks || !m_placed){
        return "";
    }
    if (m_astray){
        m_result.unchecked.push_back(m_row + " " + step.name + " (slid on)");
        return "";
    }
    if (!expected || Pokemon::tiles_of(*expected).empty()){
        m_result.unchecked.push_back(m_row + " " + step.name);
        return "";
    }
    const SimPlayer& player = m_sim.player();
    if (player.warped && !story_to_local(m_sim.world(), expected->map, nearest_tile(player.pos))){
        return "";
    }
    return off_pose(*expected);
}

Outcome SimExecutor::stub(){
    if (!m_result.rows.empty()){
        m_result.rows.back().stubbed.push_back(m_step);
    }
    m_sim.end_dialogue();
    return Pokemon::outcome_ok();
}

void SimExecutor::count_coupons(const std::string& step){
    static const std::set<std::string> CLOWNS{"mart_clown", "tv_clown", "company_clown"};
    if (CLOWNS.contains(step) && !m_knobs.talks_without_coupon.contains(step)){
        m_coupons.insert(step);
    }
    if (step == "president"){
        m_poketch = m_coupons.size() == CLOWNS.size();
        m_logger.log(std::format("[{}] president: {} coupons, {}.", m_row, m_coupons.size(), m_poketch ? "the Poketch" : "no Poketch"));
    }
}

//  The steps that need the console: stubbed, but for the Center's door and
//  the speeches after a battle.
StoryBoundActions SimExecutor::sim_actions(){
    const auto stubbed = [this](const StoryStep&, RunState&){
        return stub();
    };
    //  Played from where the step before left us, on its map.
    const auto wait_for_wanderer = [this, stubbed](const StoryStep& step, RunState& state){
        if (!sync(state.pose)){
            return stubbed(step, state);
        }
        if (m_knobs.out_of_reach.contains(step.name)){
            return Pokemon::outcome_failed("operation_failed", step.name + ": no talk from any stand tile.");
        }
        return sim_wait_for_wanderer(m_sim, m_sim.world(), state.pose, step);
    };
    return StoryBoundActions{
        .new_game = stubbed,
        .options = stubbed,
        .answer = stubbed,
        .battle = [this](const StoryStep& step, RunState& state){
            return battle(state, step);
        },
        .train = stubbed,
        .heal = [this](const StoryStep& step, RunState& state){
            return heal(state, step);
        },
        .save = stubbed,
        .event = [this, stubbed, wait_for_wanderer](const std::string& name) -> Pokemon::StoryActionFunction{
            if (name == "check_poketch"){
                return [this](const StoryStep& step, RunState&){
                    return m_poketch ? Pokemon::outcome_ok() : Pokemon::outcome_failed("no_poketch", step.name + ": no Poketch on the screen.");
                };
            }
            if (name == "wait_for_wanderer"){
                return wait_for_wanderer;
            }
            if (name == "read_coupons"){
                return [this](const StoryStep&, RunState& state){
                    for (const ClownCoupon& coupon : jubilife_clown_coupons()){
                        if (m_coupons.contains(std::string(coupon.step))){
                            state.flags.emplace(coupon.flag);
                        }
                    }
                    return Pokemon::outcome_ok();
                };
            }
            if (name == "check_coupons"){
                return [this](const StoryStep& step, RunState&){
                    return m_coupons.size() == jubilife_clown_coupons().size() ? Pokemon::outcome_ok()
                        : Pokemon::outcome_failed("operation_failed", std::format("{}: {} of the coupons in the Key Items.", step.name, m_coupons.size()));
                };
            }
            return stubbed;
        },
    };
}

//  The look at a battle's opening that shows its battle: past the three
//  looks of a door's fade (StoryInterrupts::max_looks), 1.8 s at BDSP's.
constexpr size_t OPENING_LOOKS = 6;

Pokemon::ScreenLook SimExecutor::look(bool fresh){
    const WallClock start = WallClock() + std::chrono::hours(1);
    Pokemon::ScreenLook out;
    out.log.record({Pokemon::ScreenEvent::PLAYER_CAN_MOVE}, start);
    if (std::exchange(m_dialog_rose, false)){
        out.log.record({Pokemon::ScreenEvent::DIALOG_BOX, Pokemon::ScreenEvent::PLAYER_CAN_MOVE}, start + 33ms);
    }
    if (m_battle_opening){
        out.log.record({Pokemon::ScreenEvent::FADE_WHITE}, start + 33ms);
        m_opening_looks = fresh ? 1 : m_opening_looks + 1;
        if (m_opening_looks >= OPENING_LOOKS){
            out.log.record({Pokemon::ScreenEvent::BATTLE_STARTED}, start + BETWEEN_STEPS_LOOK - 33ms);
        }
    }
    out.window = {.start = start, .end = start + BETWEEN_STEPS_LOOK};
    return out;
}

//  A dialog or a warp closes what is up and logs it: the simulation shows
//  only the dialogs it pops up. A battle ends its opening only: a scene the
//  walk before ran into goes on after it.
StoryInterruptActions SimExecutor::sim_interrupt_actions(){
    const auto cleared = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&, RunState&){
            m_logger.log(std::format("[{}] {}: {} cleared before it.", m_row, m_step, what));
            m_dialog_up = false;
            m_sim.end_dialogue();
            return Pokemon::outcome_ok();
        };
    };
    const auto battled = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&, RunState&){
            m_logger.log(std::format("[{}] {}: {} cleared before it.", m_row, m_step, what));
            m_battle_opening = false;
            return Pokemon::outcome_ok();
        };
    };
    return StoryInterruptActions{
        .wild = battled("a wild battle"),
        .trainer = battled("a trainer's battle"),
        .dialog = cleared("a dialog"),
        .warp = cleared("a warp"),
    };
}

Outcome SimExecutor::run_action(const StoryStep& step, RunState& state){
    if (m_dialog_up){
        if (!presses_through_text(step.kind)){
            return Pokemon::outcome_failed("dialog", step.name + ": a dialog that popped up before it is still up");
        }
        m_dialog_up = false;
    }
    return Pokemon::TableExecutor::run_action(step, state);
}

Outcome SimExecutor::run_step(const StoryStep& step, RunState& state){
    if (!m_start_mismatch.empty()){
        return Pokemon::outcome_failed("sim_mismatch", "the row before " + std::exchange(m_start_mismatch, ""));
    }
    m_far = step.pose_after ? std::optional<Pose>(Pokemon::pose_of(*step.pose_after)) : std::nullopt;
    //  A scene goes on from where the step before left us. A route walks
    //  on the world it plans on (Jubilife alone has no Route 204 north of it).
    //  An event of a walking kind (talk_to_wanderer) is the console's.
    const Pokemon::StoryAction* action = m_table.find(step);
    const bool walks = (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS ||
        step.kind == StoryStepKind::STEP || step.kind == StoryStepKind::TALK) &&
        action != nullptr && !action->needs_console;
    const GridPose* target_ptr = step.args.contains("target") ? &story_arg<GridPose>(step.args, "target") : nullptr;
    const StoryWorld* on = step.kind == StoryStepKind::ROUTE && target_ptr && !state.pose.map.empty()
        ? story_plan_world(state.pose.map, target_ptr->map) : nullptr;
    //  A world of one map is sim_world()'s, with its people.
    if (on != nullptr && on == story_plan_world(state.pose.map, state.pose.map)){
        on = nullptr;
    }
    if (walks && !sync(state.pose, on)){
        m_logger.log(std::format("[{}] {}: no map for {}; passed over.", m_row, step.name, Pokemon::to_string(state.pose)));
        m_passed_over = true;
        return Pokemon::outcome_ok();
    }
    if (walks && m_knobs.scene_at_walk_end && m_knobs.scene_at_walk_end(m_row, step.name)){
        start_scene_at_end(step);
    }
    return Pokemon::TableExecutor::run(step, state);
}

void SimExecutor::show_before(const StoryStep& step){
    if (m_knobs.dialog_before && m_knobs.dialog_before(m_row, step.name)){
        m_dialog_up = m_dialog_rose = true;
        m_logger.log(std::format("[{}] {}: a dialog pops up before it.", m_row, step.name));
    }
    m_battle_opening = m_knobs.battle_opening_before && m_knobs.battle_opening_before(m_row, step.name);
    if (m_battle_opening){
        m_logger.log(std::format("[{}] {}: a battle's opening is still playing before it.", m_row, step.name));
    }
}

Outcome SimExecutor::run(const StoryStep& step, RunState& state){
    Outcome outcome;
    m_passed_over = false;
    m_step = step.name;
    sim_forget_warp(m_sim);
    show_before(step);
    try{
        outcome = run_step(step, state);
    }catch (const GridSimFailure& e){
        outcome = Pokemon::outcome_failed("sim_failure", e.message);
    }
    m_battle_opening = false;
    m_astray |= std::exchange(m_misses.slid, false);
    const std::string mismatch = outcome.succeeded() && !m_passed_over ? check(step, outcome) : "";
    if (!mismatch.empty()){
        outcome = Pokemon::outcome_failed("sim_mismatch", step.name + ": " + mismatch);
    }
    //  What the simulation does not play (scenes, events, battles) ends
    //  where the table says it does.
    if (outcome.succeeded() && step.pose_after){
        m_astray = false;
        m_misplaced = false;
        sync(Pokemon::pose_of(*step.pose_after));
    }
    if (outcome.succeeded()){
        land(step.name);
    }
    m_logger.log(std::format("[{}] {}: {}, the player at {}.", m_row, step.name, Pokemon::to_string(outcome), to_string(m_sim.player().pos)));
    if (std::exchange(m_misses.missed, false) && m_result.missed_step.empty()){
        m_result.missed_step = m_row + " " + step.name;
    }
    if (!outcome.succeeded() && m_result.error.empty()){
        //  The walker's last notes, to see why.
        for (size_t i = m_notes.size() - std::min<size_t>(m_notes.size(), 40); i < m_notes.size(); i++){
            m_logger.log(std::format("[{}] note: {}", m_row, m_notes[i]));
        }
        m_result.error = m_row + " " + Pokemon::to_string(outcome);
        m_result.failed_step = m_row + " " + step.name;
    }
    const std::vector<std::string>& fought = m_sim.player().scenes_fought;
    for (; m_scenes_fought < fought.size(); m_scenes_fought++){
        m_logger.log(std::format("[{}] {}: the walk fought {}'s dialogue as a trainer's.", m_row, step.name, fought[m_scenes_fought]));
        m_result.scenes_fought.push_back(std::format("{} {}: {}", m_row, step.name, fought[m_scenes_fought]));
    }
    m_result.world = m_placed ? &m_sim.world() : nullptr;
    m_result.end = nearest_tile(m_sim.player().pos);
    m_ran.insert(step.name);
    arm(step.name);
    return outcome;
}

//  Runs `range` of `rows` (the table unless given) on `executor`, stopping
//  at the first failure.
Pokemon::SegmentReport run_rows_on(
    SimExecutor& executor, Logger& logger, const Pokemon::SegmentRange& range,
    const std::vector<StoryCheckpoint>& rows
){
    std::set<std::string> saved;
    Pokemon::Segment segment = make_story_segment(rows, executor, saved);
    Pokemon::SegmentPolicy policy;
    policy.attempts_per_checkpoint = 1;
    Pokemon::SegmentHooks hooks;
    hooks.reload = [](RunState&){ return Pokemon::outcome_ok(); };
    hooks.log = [&logger](const std::string& line){ logger.log(line); };
    RunState state;
    return Pokemon::run_segment(range, policy, segment, hooks, state);
}

//  Runs `range` of the table on the simulation, stopping at the first failure.
UnitTestResult run_rows(Logger& logger, const Pokemon::SegmentRange& range){
    SimExecutor executor(logger);
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, range);
    for (const std::string& step : executor.result().unchecked){
        logger.log("No end to check after " + step + ".");
    }
    if (!report.completed){
        return std::format("stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, executor.result().error);
    }
    return true;
}

namespace detail{
//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const StoryStep& step){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        for (StoryStep& old : checkpoint.steps){
            if (checkpoint.id == row && old.name == step.name){
                old = step;
                return rows;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + step.name + " in " + row);
}

bool ends_on(const SimRun& run, const Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const std::optional<TilePos> local = run.world ? story_to_local(*run.world, pose.map, run.end) : std::nullopt;
    return local && std::find(tiles.begin(), tiles.end(), *local) != tiles.end();
}
std::string end_of(const SimRun& run){
    return run.world ? describe(*run.world, run.end) : "nowhere";
}
std::string stub_report(const SimRun& run){
    std::string out;
    for (const SimRun::RowStart& row : run.rows){
        out += std::format("{}: {} stubbed", row.row, row.stubbed.size());
        for (size_t i = 0; i < row.stubbed.size(); i++){
            out += (i == 0 ? ": " : ", ") + row.stubbed[i];
        }
        out += "\n";
    }
    return out;
}
} // namespace detail


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

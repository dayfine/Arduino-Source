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
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Pokemon_CenterVisit.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStoryEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkScreen.h"
#include "PokemonBDSP_AutoStorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;

using namespace std::chrono_literals;
using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::StoryFailure;
using Pokemon::TilePos;


bool SimStoryStepIO::settle(Button, const std::string& label, SettleKind kind, const std::string&){
    const bool fade = kind == SettleKind::AFTER_WARP;
    const bool fading_in = std::exchange(m_misses.fading_in, false);
    bool saw_dialogue = m_sim.player().dialog;
    const Milliseconds uncovered = m_route.take_uncovered_first_line();
    const auto scene = m_knobs.scenes.find(m_step);
    if (scene != m_knobs.scenes.end()){
        SimScene beats = scene->second;
        if (uncovered > Milliseconds(0)){
            beats.insert(beats.begin(), sim_pause("the first line is still approaching", uncovered, true));
        }
        m_misses.fade_unseen = false;
        m_sim.end_dialogue();
        saw_dialogue |= std::ranges::any_of(beats, [](const SimBeat& beat){ return beat.line; });
        const std::string early = play_scene(beats, settle_rule(kind));
        if (!early.empty()){
            m_route.note(label + ": talk-over " + early + ".", RouteNote::FAILURE);
        }
        return saw_dialogue;
    }
    const auto center = m_knobs.centers.find(m_step);
    if (fading_in && center != m_knobs.centers.end() && center->second.fade_in > 0ms){
        const std::string early = play_scene({sim_fade("the Center's fade-in", center->second.fade_in)}, settle_rule(kind));
        if (!early.empty()){
            m_route.note(std::format("{}: settle {}.", label, early), RouteNote::FAILURE);
        }
    }
    const bool scene_fades = m_sim.player().dialog && m_far_side();
    if (fade && ((!m_sim.player().warped && !scene_fades) || m_misses.fade_unseen)){
        m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
    }
    m_misses.fade_unseen = false;
    m_sim.end_dialogue();
    return saw_dialogue;
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

WarpSeen SimStoryStepIO::door_fade(const std::string& banner){
    const WarpFade fade = watched_fade(DOOR_FADE_RULE, SIM_DOOR_FADE);
    if (fade != WarpFade::NONE){
        return {.fade = fade};
    }
    m_misses.fade_unseen = true;
    return {.banner = !banner.empty() && !banner_missed() ? banner : ""};
}

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

std::optional<size_t> SimStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    const std::optional<size_t> taps = taps_to_banner(direction, map, max_taps);
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
    return GridSimRouteIO::walk_to_wall(walk);
}

WalkResult SimKnobRouteIO::walk_taps(Direction direction, size_t tiles, const std::string& label){
    check_fade_in(label);
    const auto misread = std::find_if(m_knobs.still_taps.begin(), m_knobs.still_taps.end(), [&](const SimStillTaps& item){
        return item.step == m_step && item.direction == direction;
    });
    const WalkResult result = misread == m_knobs.still_taps.end()
        ? GridSimRouteIO::walk_taps(direction, tiles, label)
        : taps_misread(direction, tiles, label, *misread);
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

WalkResult SimKnobRouteIO::taps_misread(Direction direction, size_t tiles, const std::string& label, const SimStillTaps& misread){
    WalkResult result{.stop = WalkStop::LIMIT, .walked = 0ms, .wild_battles = 0, .moved_chunks = 0};
    size_t unchanged = 0;
    for (size_t tap = 1; tap <= tiles && result.stop == WalkStop::LIMIT; tap++){
        const WalkResult one = GridSimRouteIO::walk_taps(direction, 1, label);
        result.walked += one.walked;
        result.wild_battles += one.wild_battles;
        result.stop = one.stop;
        const bool moved = one.moved_chunks > 0 && !misread.taps.contains(tap);
        if (moved){
            result.moved_chunks += 1 + unchanged;
        }
        unchanged = moved ? 0 : unchanged + 1;
        if (result.stop == WalkStop::LIMIT && unchanged >= 2){
            result.stop = WalkStop::BLOCKED;
        }
    }
    note(std::format("[Sim] {}: {} taps {}, taps read still {}, {} counted, {}.", label, tiles, direction_name(direction),
        misread.taps.size(), result.moved_chunks, walk_stop_name(result.stop)), RouteNote::LOG);
    return result;
}

WalkResult SimKnobRouteIO::slide_on(const SimSlide& slide, WalkResult result){
    m_misses.slid = true;
    for (size_t tap = 0; tap < slide.tiles; tap++){
        const SimMove move = m_grid.tap(slide.direction, ROUTE_MS_PER_TILE_ACROSS, false);
        if (move.stop != SimStop::WARP){
            note(std::format("[Sim] slid a tile on to {}.", to_string(m_grid.player().pos)), RouteNote::LOG);
            continue;
        }
        note("[Sim] slid onto a warp to " + format_grid_pose(slide.far) + ".", RouteNote::LOG);
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
    m_uncovered_first_line = Milliseconds(0);
    Milliseconds& longest = m_misses.dialog_waits[m_step];
    longest = std::max(longest, wait);
    const auto late = m_knobs.first_line.find(m_step);
    if (late != m_knobs.first_line.end() && late->second > wait){
        m_uncovered_first_line = late->second - wait;
        note(std::format("[Sim] {}: the first line comes after {} ms, past the {} ms wait.", m_step, late->second.count(), wait.count()), RouteNote::LOG);
        return false;
    }
    if (late != m_knobs.first_line.end()){
        m_uncovered_first_line = Milliseconds(0);
        return true;
    }
    return GridSimRouteIO::dialog_within(wait, color);
}

namespace{

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
        {.name = "mom", .map = "PlayerHouse1F", .tile = {7, 7}, .row = "00.04"},
        {.name = "barry", .map = "OreburghCity", .tile = {26, 21}, .row = "01.02b", .until_row = "01.02b", .until = "barrys_lines"},
    };
}

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

bool on_world(const StoryWorld& world, const std::string& map){
    try{
        story_to_world(world, map, {0, 0});
        return true;
    }catch (const InternalProgramError&){
        return false;
    }
}

bool step_computes_pose(const BdspStep& step, const GridPose& before){
    if (step.action == "route"){
        return std::get<RouteStepArgs>(step.args).legs.empty();
    }
    if (step.action == "legs"){
        const std::string& variant = std::get<LegsStepArgs>(step.args).variant;
        return variant.empty() || variant == "legs" || variant == "one_short_ok" || variant == "measured";
    }
    if (step.action == "step"){
        const std::string& variant = std::get<StepMoveArgs>(step.args).variant;
        if (variant == "to_banner" || variant == "tap"){
            return false;
        }
        return !(step.pose_after && step.pose_after->map != before.map);
    }
    return false;
}

class SimCenterVisit : public BdspCenterVisit{
public:
    SimCenterVisit(StoryStepIO& io, bool run, std::string_view map, GridSim& sim, SimMisses& misses, const SimCenterDoors& doors, std::string label)
        : BdspCenterVisit(io, run, map)
        , m_sim(sim)
        , m_misses(misses)
        , m_doors(doors)
        , m_label(std::move(label))
    {}

    virtual WarpSeen walk_in() override{
        const WarpSeen seen = BdspCenterVisit::walk_in();
        if (!m_sim.player().warped.has_value()){
            return seen;
        }
        m_misses.fade_unseen = false;
        m_misses.fading_in = true;
        return m_doors.late_fade ? WarpSeen{} : seen;
    }
    virtual void line_up_by_sight() override{
        throw GridSimFailure{std::format("{}: no door north of {}", m_label, to_string(m_sim.player().pos))};
    }
    virtual Pokemon::CenterPlace place() override{
        const bool inside = m_out ? m_doors.stays_inside : m_sim.player().warped.has_value();
        return inside ? Pokemon::CenterPlace::INSIDE : Pokemon::CenterPlace::OUTSIDE;
    }
    virtual void heal() override{
        if (m_misses.fading_in){
            throw GridSimFailure{std::format("{}: walked to the nurse while the Center was still fading in.", m_label)};
        }
    }
    virtual WarpSeen walk_out() override{
        m_out = true;
        if (m_doors.late_fade && !m_doors.stays_inside){
            return {};
        }
        return {.fade = WarpFade::BLACK};
    }
    virtual void log(std::string_view line) override{
        m_io.route().note(std::string(line), RouteNote::LOG);
    }

private:
    GridSim& m_sim;
    SimMisses& m_misses;
    const SimCenterDoors& m_doors;
    std::string m_label;
    bool m_out = false;
};

} // namespace


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

std::vector<SimWanderer> SimExecutor::wanderers(const StoryWorld& world) const{
    std::vector<SimWanderer> out;
    if (!m_knobs.wander_seed){
        return out;
    }
    for (const auto& [map, json] : bdsp_map_json()){
        if (!on_world(world, map)){
            continue;
        }
        for (const auto& object : story_map(map).objects){
            if (!object.wander){
                continue;
            }
            const uint64_t seed = *m_knobs.wander_seed * 1000003 + out.size() * 7919 + (uint64_t)(object.x * 31 + object.y);
            const TilePos low = story_to_world(world, map, {object.wander->x0, object.wander->y0});
            const TilePos high = story_to_world(world, map, {object.wander->x1, object.wander->y1});
            out.push_back({
                .name = object.name,
                .box = {.x0 = low.x, .y0 = low.y, .x1 = high.x, .y1 = high.y},
                .pos = story_to_world(world, map, {object.x, object.y}),
                .seed = seed,
                .step_interval = Milliseconds(2000),
                .elapsed = Milliseconds(0),
                .time_offset = Milliseconds((int64_t)(seed % 2000)),
            });
        }
    }
    return out;
}

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

bool SimExecutor::sync(const GridPose& pose, const StoryWorld* on){
    const StoryWorld* world = on != nullptr ? on : pose.map.empty() ? nullptr : sim_world(pose.map);
    if (world == nullptr || pose.tiles.empty()){
        return false;
    }
    if ((m_astray || m_misplaced) && m_placed){
        return true;
    }
    const SimPlayer& player = m_sim.player();
    const std::optional<TilePos> local = !m_placed || &m_sim.world() != world
        ? std::nullopt : story_to_local(*world, pose.map, nearest_tile(player.pos));
    if (local && on_grid(player.pos) && std::find(pose.tiles.begin(), pose.tiles.end(), *local) != pose.tiles.end()){
        return true;
    }
    m_sim.place(setup(world), story_to_world(*world, pose.map, pose.tiles.front()), pose.facing);
    m_placed = true;
    return true;
}

void SimExecutor::start_scene_at_end(const BdspStep& step){
    std::optional<GridPose> end;
    if (step.action == "route"){
        end = std::get<RouteStepArgs>(step.args).target;
    }else if (step.pose_after){
        end = *step.pose_after;
    }
    if (!end){
        return;
    }
    SimTrigger trigger{.name = "early scene after " + step.name, .tiles = {}};
    for (const TilePos& tile : end->tiles){
        try{
            trigger.tiles.push_back(story_to_world(m_sim.world(), end->map, tile));
        }catch (const InternalProgramError&){}
    }
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
    m_logger.log(std::format("[{}] {}: the next scene starts on its end, {}.", m_row, step.name, format_grid_pose(*end)));
    sim_add_trigger(m_sim, std::move(trigger));
}

void SimExecutor::land(const std::string& step){
    const auto lands = m_knobs.lands.find(step);
    if (lands == m_knobs.lands.end()){
        return;
    }
    m_misplaced = false;
    const SimPlayer& player = m_sim.player();
    const GridPose& pose = lands->second;
    if (sync(pose)){
        m_misplaced = true;
        m_logger.log(std::format("[{}] {}: the player lands on {}, off the table's pose.", m_row, step, to_string(player.pos)));
    }
}

bool SimExecutor::cross(){
    return m_far && cross_to(*m_far);
}

bool SimExecutor::cross_to(const GridPose& far){
    const StoryWorld* world = sim_world(far.map);
    if (world == nullptr || far.tiles.empty()){
        return false;
    }
    m_sim.place(setup(world), story_to_world(*world, far.map, far.tiles.front()), std::nullopt);
    return true;
}

std::string SimExecutor::off_pose(const GridPose& pose) const{
    if (!m_placed || pose.tiles.empty() || sim_world(pose.map) == nullptr){
        return "";
    }
    const SimPlayer& player = m_sim.player();
    const TilePos at = nearest_tile(player.pos);
    try{
        const auto there = [&](const TilePos& tile){ return story_to_world(m_sim.world(), pose.map, tile) == at; };
        if (on_grid(player.pos) && std::ranges::any_of(pose.tiles, there)){
            return "";
        }
    }catch (const InternalProgramError&){}
    return std::format("ended at {} ({}), not on {}.", to_string(player.pos), describe(m_sim.world(), at), format_grid_pose(pose));
}

std::optional<StoryFailure> SimExecutor::heal(BdspRunState& state, const BdspStep& step){
    if (!sync(state.pose)){
        return std::nullopt;
    }
    static const SimCenterDoors PLAIN_DOORS;
    const auto doors = m_knobs.centers.find(step.name);
    SimCenterVisit visit(m_io, m_walks.can_run(), state.pose.map, m_sim, m_misses, doors == m_knobs.centers.end() ? PLAIN_DOORS : doors->second, step.name);
    return Pokemon::run_center_visit(step.name, visit);
}

std::optional<StoryFailure> SimExecutor::battle(BdspRunState&, const BdspStep& step){
    const auto scene = m_knobs.scenes.find(step.name);
    if (scene != m_knobs.scenes.end()){
        const std::string early = play_scene(scene->second, settle_rule(SettleKind::TALK));
        if (!early.empty()){
            return StoryFailure{.code = "talk_over", .detail = step.name + ": " + early};
        }
    }
    return stub();
}

std::string SimExecutor::check(const BdspStep& step, const GridPose& after_pose, bool pose_expected){
    const bool walks = step.action == "route" || step.action == "legs" || step.action == "step";
    const std::optional<GridPose> expected = step.pose_after
        ? step.pose_after
        : (pose_expected && !after_pose.tiles.empty() ? std::optional<GridPose>(after_pose) : std::nullopt);
    if (!walks || !m_placed){
        return "";
    }
    if (m_astray){
        m_result.unchecked.push_back(m_row + " " + step.name + " (slid on)");
        return "";
    }
    if (!expected || expected->tiles.empty()){
        m_result.unchecked.push_back(m_row + " " + step.name);
        return "";
    }
    const SimPlayer& player = m_sim.player();
    if (player.warped && !story_to_local(m_sim.world(), expected->map, nearest_tile(player.pos))){
        return "";
    }
    return off_pose(*expected);
}

std::optional<StoryFailure> SimExecutor::stub(){
    if (!m_result.rows.empty()){
        m_result.rows.back().stubbed.push_back(m_step);
    }
    m_sim.end_dialogue();
    return std::nullopt;
}

constexpr size_t OPENING_LOOKS = 6;

Pokemon::ScreenLook SimExecutor::look(bool fresh){
    const WallClock start = WallClock() + std::chrono::hours(1);
    Pokemon::ScreenLook out;
    out.log.record({ScreenEvent::PLAYER_CAN_MOVE}, start);
    if (std::exchange(m_dialog_rose, false)){
        out.log.record({ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE}, start + 33ms);
    }
    if (m_battle_opening){
        out.log.record({ScreenEvent::FADE_WHITE}, start + 33ms);
        m_opening_looks = fresh ? 1 : m_opening_looks + 1;
        if (m_opening_looks >= OPENING_LOOKS){
            out.log.record({ScreenEvent::BATTLE_STARTED}, start + BETWEEN_STEPS_LOOK - 33ms);
        }
    }
    out.window = {.start = start, .end = start + BETWEEN_STEPS_LOOK};
    return out;
}

StoryInterruptActions SimExecutor::sim_interrupt_actions(){
    const auto cleared = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            m_logger.log(std::format("[{}] {}: {} cleared before it.", m_row, m_step, what));
            m_dialog_up = false;
            m_sim.end_dialogue();
            return std::nullopt;
        };
    };
    const auto battled = [this](std::string_view what){
        return [this, what](const Pokemon::InterruptVerdict&) -> std::optional<StoryFailure>{
            m_logger.log(std::format("[{}] {}: {} cleared before it.", m_row, m_step, what));
            m_battle_opening = false;
            return std::nullopt;
        };
    };
    return StoryInterruptActions{
        .wild = battled("a wild battle"),
        .trainer = battled("a trainer's battle"),
        .dialog = cleared("a dialog"),
        .warp = cleared("a warp"),
    };
}

void SimExecutor::show_before(const BdspStep& step){
    if (m_knobs.dialog_before && m_knobs.dialog_before(m_row, step.name)){
        m_dialog_up = m_dialog_rose = true;
        m_logger.log(std::format("[{}] {}: a dialog pops up before it.", m_row, step.name));
    }
    m_battle_opening = m_knobs.battle_opening_before && m_knobs.battle_opening_before(m_row, step.name);
    if (m_battle_opening){
        m_logger.log(std::format("[{}] {}: a battle's opening is still playing before it.", m_row, step.name));
    }
}

void SimExecutor::on_attempt_start(const StoryCheckpoint& row){
    m_row = row.id;
    m_start_mismatch = row.start ? off_pose(*row.start) : "";
    m_ran.clear();
    m_astray = false;
    m_result.rows.push_back({.row = row.id, .fades = m_misses.fades, .banners = m_misses.banners});
    arm("");
}

std::optional<StoryFailure> SimExecutor::run_sim_step(
    const BdspStep& step,
    BdspRunState& state,
    const std::function<std::optional<StoryFailure>()>& run_action
){
    if (!m_start_mismatch.empty()){
        return StoryFailure{.code = "sim_mismatch", .detail = "the row before " + std::exchange(m_start_mismatch, "")};
    }
    m_far = step.pose_after;
    const bool walks = step.action == "route" || step.action == "legs" || step.action == "step" || step.action == "talk";
    const GridPose* target_ptr = step.action == "route" ? &std::get<RouteStepArgs>(step.args).target : nullptr;
    const StoryWorld* on = target_ptr != nullptr && !state.pose.map.empty()
        ? story_plan_world(state.pose.map, target_ptr->map) : nullptr;
    if (on != nullptr && on == story_plan_world(state.pose.map, state.pose.map)){
        on = nullptr;
    }
    if (walks && !sync(state.pose, on)){
        m_logger.log(std::format("[{}] {}: no map for {}; passed over.", m_row, step.name, format_grid_pose(state.pose)));
        m_passed_over = true;
        if (step.pose_after){
            state.pose = *step.pose_after;
        }
        return std::nullopt;
    }
    if (walks && m_knobs.scene_at_walk_end && m_knobs.scene_at_walk_end(m_row, step.name)){
        start_scene_at_end(step);
    }
    if (m_knobs.clear_interrupts){
        const Pokemon::InterruptCheck ic = Pokemon::clear_story_interrupt(m_interrupts, step.name, step.action);
        if (ic.pose_lost){
            state.pose = {};
        }
        if (ic.failure){
            return *ic.failure;
        }
    }
    if (m_dialog_up){
        if (!presses_through_text(step.action)){
            return StoryFailure{.code = "dialog", .detail = step.name + ": a dialog that popped up before it is still up"};
        }
        m_dialog_up = false;
    }
    return run_action();
}

void SimExecutor::record_step_result(const BdspStep& step, const std::optional<StoryFailure>& failure){
    m_logger.log(std::format(
        "[{}] {}: {}, the player at {}.",
        m_row, step.name, failure ? std::format("{}", *failure) : std::string("ok"), to_string(m_sim.player().pos)
    ));
    if (std::exchange(m_misses.missed, false) && m_result.missed_step.empty()){
        m_result.missed_step = m_row + " " + step.name;
    }
    if (failure && m_result.error.empty()){
        for (size_t i = m_notes.size() - std::min<size_t>(m_notes.size(), 40); i < m_notes.size(); i++){
            m_logger.log(std::format("[{}] note: {}", m_row, m_notes[i]));
        }
        m_result.error = m_row + " " + std::format("{}", *failure);
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
}

std::optional<StoryFailure> SimExecutor::wrap_step(
    const BdspStep& step,
    BdspRunState& state,
    const std::function<std::optional<StoryFailure>()>& run_action
){
    std::optional<StoryFailure> failure;
    m_passed_over = false;
    m_step = step.name;
    sim_forget_warp(m_sim);
    show_before(step);
    const bool pose_expected = step_computes_pose(step, state.pose);
    try{
        failure = run_sim_step(step, state, run_action);
    }catch (const GridSimFailure& e){
        failure = StoryFailure{.code = "sim_failure", .detail = e.message};
    }
    m_battle_opening = false;
    m_astray |= std::exchange(m_misses.slid, false);
    const std::string mismatch = !failure && !m_passed_over ? check(step, state.pose, pose_expected) : "";
    if (!mismatch.empty()){
        failure = StoryFailure{.code = "sim_mismatch", .detail = step.name + ": " + mismatch};
    }
    if (!failure && step.pose_after){
        m_astray = false;
        m_misplaced = false;
        sync(*step.pose_after);
    }
    if (!failure){
        land(step.name);
    }
    record_step_result(step, failure);
    return failure;
}

BdspStoryHandlers SimExecutor::sim_handlers(){
    const auto stubbed = [this](const BdspStep&, BdspRunState&){
        return stub();
    };
    return BdspStoryHandlers{
        .new_game = stubbed,
        .options = stubbed,
        .answer = stubbed,
        .battle = [this](const BdspStep& step, BdspRunState& state){
            return battle(state, step);
        },
        .train = stubbed,
        .heal = [this](const BdspStep& step, BdspRunState& state){
            return heal(state, step);
        },
        .save = stubbed,
        .event = [stubbed](const std::string&) -> BdspStepAction{
            return stubbed;
        },
        .on_attempt_start = [this](const StoryCheckpoint& row){
            on_attempt_start(row);
        },
        .wrap_step = [this](
            const StoryCheckpoint&,
            const BdspStep& step,
            BdspRunState& state,
            const std::function<std::optional<StoryFailure>()>& run_action
        ){
            return wrap_step(step, state, run_action);
        },
    };
}

SimRunReport SimExecutor::run_range(const SimRunRange& range, const std::vector<StoryCheckpoint>& rows){
    std::vector<StoryCheckpoint> slice;
    bool started = !range.from.has_value();
    for (const StoryCheckpoint& row : rows){
        if (!started && row.id == *range.from){
            started = true;
        }
        if (started){
            slice.push_back(row);
            if (range.to && row.id == *range.to){
                break;
            }
        }
    }
    if (slice.empty()){
        return SimRunReport{
            .completed = false,
            .failed_checkpoint = range.from.value_or(""),
            .failed_event = {},
            .failure = StoryFailure{.code = "empty_range", .detail = "no checkpoints in range"},
        };
    }

    std::vector<Pokemon::StoryCheckpoint> checkpoints = make_bdsp_engine_checkpoints(
        slice, m_walks, m_run_state, sim_handlers()
    );
    Pokemon::StoryGame game{.save = {}, .reload = {}, .attempts_per_checkpoint = 1};
    Pokemon::RunState engine_state;
    const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(
        m_backend, checkpoints, game, engine_state
    );
    if (!failure){
        return SimRunReport{.completed = true};
    }
    std::string failed_event;
    if (failure->event_index){
        for (const StoryCheckpoint& row : slice){
            if (row.id == failure->checkpoint_id && *failure->event_index < row.steps.size()){
                failed_event = row.steps[*failure->event_index].name;
                break;
            }
        }
    }
    return SimRunReport{
        .completed = false,
        .failed_checkpoint = failure->checkpoint_id,
        .failed_event = std::move(failed_event),
        .failure = failure->failure,
    };
}

SimRunReport run_rows_on(
    SimExecutor& executor, Logger&, const SimRunRange& range,
    const std::vector<StoryCheckpoint>& rows
){
    return executor.run_range(range, rows);
}

UnitTestResult run_rows(Logger& logger, const SimRunRange& range){
    SimExecutor executor(logger);
    const SimRunReport report = run_rows_on(executor, logger, range);
    for (const std::string& step : executor.result().unchecked){
        logger.log("No end to check after " + step + ".");
    }
    if (!report.completed){
        return std::format("stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, executor.result().error);
    }
    return true;
}

namespace detail{
std::vector<StoryCheckpoint> table_with(const std::string& row, const BdspStep& step){
    std::vector<StoryCheckpoint> rows = story_checkpoints();
    for (StoryCheckpoint& checkpoint : rows){
        for (BdspStep& old : checkpoint.steps){
            if (checkpoint.id == row && old.name == step.name){
                old = step;
                return rows;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + step.name + " in " + row);
}

bool ends_on(const SimRun& run, const GridPose& pose){
    const std::optional<TilePos> local = run.world ? story_to_local(*run.world, pose.map, run.end) : std::nullopt;
    return local && std::find(pose.tiles.begin(), pose.tiles.end(), *local) != pose.tiles.end();
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

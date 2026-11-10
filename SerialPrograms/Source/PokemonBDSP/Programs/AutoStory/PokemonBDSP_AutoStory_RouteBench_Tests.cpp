/*  BDSP AutoStory - Route Bench Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Scores the plan of every ROUTE step of the story table with the shared
 *  route scorer (Pokemon_RouteScorer.h), and the routes the user asked for
 *  on three of them, and checks that no route scores worse than
 *  Data/route_bench.golden.txt.
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_RouteScorer.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "Utils/PokemonBDSP_AutoStory_RouteChoice.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_TableRoutes.h"
#include "PokemonBDSP_AutoStory_RouteBench_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::RouteScore;
using Pokemon::ScoredLeg;
using Pokemon::ScoredLegKind;
using Pokemon::TilePos;

//  One route to score: the plan the program walks today, or one hand-written.
struct BenchCase{
    std::string name;
    const StoryWorld* world;
    std::vector<TilePos> from;
    std::vector<TilePos> goal;
    std::vector<TilePos> triggers;      //  a scene starts here and stops the player
    std::vector<ScoredLeg> legs;
    bool run = false;
    std::optional<RobustPlanCosts> costs;
    bool planned = true;                //  false: no plan exists
    //  A human's route for this case (the user's, or a recorded run's), the
    //  bar the plan is measured against; none: the stand-in.
    std::optional<std::vector<ScoredLeg>> human = std::nullopt;
    std::string human_source = "";
};

Pokemon::ScoreMap bench_map(const BenchCase& bench){
    return route_score_map(*bench.world, bench.triggers);
}

Pokemon::RoutePaces bench_paces(){
    return route_score_paces();
}

MoveSpeed bench_speed(bool run){
    return run ? MoveSpeed::RUN : MoveSpeed::WALK;
}

//  The legs as RouteWalker walks them (route_scored_legs()).
std::vector<ScoredLeg> walked_legs(const std::vector<PlannedLeg>& legs, bool run){
    return route_scored_legs(legs, bench_speed(run));
}

RouteScore score_case(const BenchCase& bench){
    if (!bench.planned){
        return {.verdict = Pokemon::RouteVerdict::FAIL, .why = "no plan"};
    }
    const std::vector<TilePos>& targets = bench.goal;
    const std::function<bool(const TilePos&)> goal = [&targets](const TilePos& pos){
        return std::find(targets.begin(), targets.end(), pos) != targets.end();
    };
    const Pokemon::RouteReplanner replan = [&](const std::vector<TilePos>& from) -> std::optional<std::vector<ScoredLeg>>{
        const std::optional<std::vector<PlannedLeg>> legs = plan_recovered_route(*bench.world, from, goal, bench.costs.value_or(RobustPlanCosts()));
        if (!legs){
            return std::nullopt;
        }
        return walked_legs(*legs, bench.run);
    };
    return Pokemon::score_route(bench_map(bench), bench.from, goal, bench.legs, bench_paces(), Pokemon::InterruptModel(), replan);
}


//  The table's ROUTE steps from a known place, as the program plans them.
std::vector<BenchCase> table_cases(){
    std::vector<BenchCase> cases;
    std::map<std::string, size_t> seen;
    for (const TableRoute& route : table_routes()){
        if (route.world == nullptr){
            continue;
        }
        std::string name = route.row + " " + route.step->name;
        const size_t repeat = seen[name]++;
        if (repeat > 0){
            name += std::format(" #{}", repeat + 1);
        }
        const RouteStepPlan plan = plan_table_route(route);
        const bool run = route.row > std::string(RUNNING_SHOES_ROW);
        cases.push_back({
            .name = name, .world = route.world, .from = plan.start,
            .goal = world_tiles(*route.world, *route.step->target), .triggers = {},
            .legs = plan.legs ? walked_legs(*plan.legs, run) : std::vector<ScoredLeg>{},
            .run = run, .costs = story_route_costs(route.step->variant), .planned = plan.legs.has_value(),
        });
    }
    return cases;
}

size_t case_index(const std::vector<BenchCase>& cases, const std::string& name){
    for (size_t i = 0; i < cases.size(); i++){
        if (cases[i].name == name){
            return i;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No table route " + name + ".");
}

TilePos wall_from(const StoryWorld& world, const TilePos& from, Direction direction){
    return slide_to_wall(world, from, direction).stop;
}

ScoredLeg taps(Direction direction, size_t tiles){
    return {.direction = direction, .tiles = tiles, .kind = ScoredLegKind::TAPS, .run = true};
}
ScoredLeg run_to_wall(Direction direction, size_t tiles){
    return {.direction = direction, .tiles = tiles, .kind = ScoredLegKind::TO_WALL, .run = true};
}
ScoredLeg run_open(Direction direction, size_t tiles){
    return {.direction = direction, .tiles = tiles, .kind = ScoredLegKind::OPEN_HOLD, .run = true};
}

//  00.04 "along_route_201_road": the console tapped "west 16 (open)" along
//  Route 201's road at 800 ms a tile (lab-B 2026-11-09, step 9 of 00.04).
//  The user's route: run west into the funnel of columns 36 to 39, whose
//  walls stop a run north on row 9; run north, then west to the wall at
//  column 13, a tap east onto column 15 and taps south over the ledge.
BenchCase road_target(const BenchCase& current){
    const StoryWorld& world = *current.world;
    const TilePos start = current.from.front();
    const TilePos funnel = story_to_world(world, "Route201", {38, 14});
    const TilePos row9 = wall_from(world, funnel, Direction::NORTH);
    const TilePos west = wall_from(world, row9, Direction::WEST);
    const TilePos goal = current.goal.front();
    BenchCase target = current;
    target.name = "TARGET " + current.name;
    target.legs = {
        run_open(Direction::WEST, (size_t)(start.x - funnel.x)),
        run_to_wall(Direction::NORTH, (size_t)(funnel.y - row9.y)),
        run_to_wall(Direction::WEST, (size_t)(row9.x - west.x)),
        taps(Direction::EAST, (size_t)(goal.x - west.x)),
        taps(Direction::SOUTH, 4),
    };
    return target;
}

//  00.04 "into_twinleaf": the console tapped "south 33 (open)" for 26 s
//  (lab-B 2026-11-09, step 10 of 00.04), where the column ends at a wall.
//  The user's route: run south to that wall, then tap back north.
BenchCase twinleaf_target(const BenchCase& current){
    const StoryWorld& world = *current.world;
    const TilePos start = current.from.front();
    const TilePos wall = wall_from(world, start, Direction::SOUTH);
    const TilePos goal = current.goal.front();
    BenchCase target = current;
    target.name = "TARGET " + current.name;
    target.legs = {run_to_wall(Direction::SOUTH, (size_t)(wall.y - start.y))};
    if (wall.y > goal.y){
        target.legs.push_back(taps(Direction::NORTH, (size_t)(wall.y - goal.y)));
    }
    return target;
}

//  00.04 "into_moms_call": from the room's east corner the plan walks round
//  Mom to (7, 9), then taps west into her call on (6, 9) (lab-B 2026-11-09
//  17:29, step 17 of 00.04). Scored to the trigger: the route's legs and
//  the tap after them.
BenchCase moms_call_current(const BenchCase& route){
    const StoryWorld& world = *route.world;
    BenchCase current = route;
    current.name = "00.04 into_moms_call + the tap into her call";
    current.goal = {story_to_world(world, "PlayerHouse1F", {6, 9})};
    current.triggers = current.goal;
    current.legs.push_back(taps(Direction::WEST, 1));
    return current;
}

//  The user's route: down to row 9, whose wall row 10 does not stop us, so
//  a tap; then run west along row 9 until her call stops us on (6, 9).
BenchCase moms_call_target(const BenchCase& current){
    BenchCase target = current;
    target.name = "TARGET " + current.name;
    target.legs = {taps(Direction::WEST, 1), taps(Direction::SOUTH, 1), run_to_wall(Direction::WEST, 4)};
    return target;
}

//  00.04 "into_twinleaf", lab-B run 4 (2026-11-09 18:50): "south 33 (open)"
//  was blocked after 22 taps, counted at Twinleaf (15, 11), by someone on
//  (15, 12). The plan from that one tile, "west 1 (to wall)", walked six
//  tiles west, and the walk ended beside the house, east of its door. A
//  blocked count can be a tile off (blocked_stop_tiles()): both cases are
//  scored from every tile it gives, the blocker on the map. "counted tile"
//  is the plan walk_route() made then, from the count alone; the other is
//  the plan it makes now.
const StoryWorld& run4_world(const StoryWorld& world){
    static const StoryWorld blocked = [&world]{
        StoryWorld out = world;
        out.block(story_to_world(world, "TwinleafTown", {15, 12}));
        return out;
    }();
    return blocked;
}
void add_run4_cases(std::vector<BenchCase>& cases){
    const BenchCase& route = cases[case_index(cases, "00.04 into_twinleaf")];
    const StoryWorld& world = run4_world(*route.world);
    const TilePos counted = story_to_world(world, "TwinleafTown", {15, 11});
    const std::vector<TilePos> targets = route.goal;
    const auto goal = [targets](const TilePos& pos){
        return std::find(targets.begin(), targets.end(), pos) != targets.end();
    };
    BenchCase recovery = route;
    recovery.world = &world;
    recovery.from = blocked_stop_tiles(world, {counted}, Direction::SOUTH);
    recovery.human = std::nullopt;
    const std::optional<std::vector<PlannedLeg>> old_plan = plan_recovered_route(world, {counted}, goal, *recovery.costs);
    BenchCase old_case = recovery;
    old_case.name = "00.04 into_twinleaf after run 4's block, planned from the counted tile";
    old_case.legs = old_plan ? walked_legs(*old_plan, recovery.run) : std::vector<ScoredLeg>{};
    old_case.planned = old_plan.has_value();
    cases.push_back(std::move(old_case));
    const std::optional<std::vector<PlannedLeg>> new_plan = plan_fastest_route({
        .world = world, .from = recovery.from, .goal = goal, .costs = *recovery.costs, .speed = bench_speed(recovery.run),
    });
    recovery.name = "00.04 into_twinleaf after run 4's block";
    recovery.legs = new_plan ? walked_legs(*new_plan, recovery.run) : std::vector<ScoredLeg>{};
    recovery.planned = new_plan.has_value();
    cases.push_back(std::move(recovery));
}

//  A human run recorded on the console, as legs: replaces a case's stand-in.
struct RecordedHumanRun{
    std::string case_name;
    std::vector<ScoredLeg> legs;
    std::string source;     //  the run's log
};
//  The hook for recorded human runs (a log of a human playing, turned into
//  legs). None recorded yet.
std::vector<RecordedHumanRun> recorded_human_runs(){
    return {};
}

//  Adds `target` after the case it improves, and makes it that case's human
//  route: the user described each of them.
void add_target(std::vector<BenchCase>& cases, const std::string& name, const std::function<BenchCase(const BenchCase&)>& make){
    BenchCase& current = cases[case_index(cases, name)];
    BenchCase target = make(current);
    current.human = target.legs;
    current.human_source = "user's route";
    target.human = target.legs;
    target.human_source = "user's route";
    cases.push_back(std::move(target));
}

std::vector<BenchCase> bench_cases(){
    std::vector<BenchCase> cases = table_cases();
    cases.push_back(moms_call_current(cases[case_index(cases, "00.04 into_moms_call")]));
    add_target(cases, "00.04 along_route_201_road", road_target);
    add_target(cases, "00.04 into_twinleaf", twinleaf_target);
    add_target(cases, "00.04 into_moms_call + the tap into her call", moms_call_target);
    add_run4_cases(cases);
    for (const RecordedHumanRun& run : recorded_human_runs()){
        BenchCase& bench = cases[case_index(cases, run.case_name)];
        bench.human = run.legs;
        bench.human_source = "recorded: " + run.source;
    }
    return cases;
}


constexpr const char* BENCH_HEADER =
    "BDSP AutoStory: the route bench. Each ROUTE step of the story table, its\r\n"
    "plan scored by Pokemon::score_route() as RouteWalker walks it (wall legs\r\n"
    "held, at a run after 00.03; open legs tapped), then the user's routes for\r\n"
    "three of them (TARGET). Written by the test \"Route bench: no route scores\r\n"
    "worse than Data/route_bench.golden.txt\"; rerun it with PA_UPDATE_ROUTE_BENCH=1\r\n"
    "to rewrite this file. A route is worse when its verdict leaves PASS, its\r\n"
    "expected time grows or one of its human ratios grows.\r\n"
    "columns: case | expected ms (interruptions by chance) | worst ms (slowest start and\r\n"
    "the costliest single interruption) | nominal ms (slowest start, none) | stops (wall\r\n"
    "re-localizations) | recoveries (places an interruption may happen) | replans (of those,\r\n"
    "the ones the plan cannot absorb) | slower than human (expected / human ms) | riskier\r\n"
    "than human ((recoveries + 1) / (human's + 1)) | more backtracking than human ((stops +\r\n"
    "replans + 1) / (human's stops + 1)) | the human's ms (the user's route, a recorded run,\r\n"
    "or the stand-in: the shortest safe walk run with no stops) | verdict (why). A ratio\r\n"
    "above 1.5 is flagged \"!\".\r\n"
    "\r\n";

//  The bar a case's plan is measured against: a human's walk of it.
struct HumanBaseline{
    long long ms = 0;
    size_t risk_tiles = 0;      //  places an interruption may happen
    size_t stops = 0;           //  re-localization stops
    std::string source;
};

//  A step of the stand-in's search: tiles walked, grass and wanderer tiles
//  avoided where a way round exists.
struct StandInNode{
    long long cost;
    long long ms;
    size_t risk;
    TilePos pos;
    bool operator>(const StandInNode& other) const{
        return cost > other.cost;
    }
};

using StandInQueue = std::priority_queue<StandInNode, std::vector<StandInNode>, std::greater<StandInNode>>;

bool in_goal(const BenchCase& bench, const TilePos& pos){
    return std::find(bench.goal.begin(), bench.goal.end(), pos) != bench.goal.end();
}

//  Queues the steps from `node` the stand-in may take: not through a warp
//  or a trigger off the goal, and round grass and wanderers where it can.
void push_steps(StandInQueue& open, const BenchCase& bench, const Pokemon::ScoreMap& map,
    const Pokemon::RoutePaces& paces, const StandInNode& node
){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        const std::optional<TilePos> next = map.step(node.pos, direction);
        if (!next){
            continue;
        }
        const Pokemon::TileRisk risk = map.risk(*next);
        if ((risk == Pokemon::TileRisk::WARP || risk == Pokemon::TileRisk::TRIGGER) && !in_goal(bench, *next)){
            continue;
        }
        const long long tiles = std::abs(next->x - node.pos.x) + std::abs(next->y - node.pos.y);
        const bool risky = risk == Pokemon::TileRisk::ENCOUNTER || risk == Pokemon::TileRisk::WANDERER;
        open.push({node.cost + tiles + (risky ? 1000 : 0), node.ms + tiles * paces.ms_per_tile(direction, true),
            node.risk + (risky ? 1 : 0), *next});
    }
}

//  The shortest safe walk from `start` to the goal, run the whole way.
std::optional<HumanBaseline> stand_in_from(const BenchCase& bench, const TilePos& start){
    const Pokemon::ScoreMap map = bench_map(bench);
    const Pokemon::RoutePaces paces = bench_paces();
    StandInQueue open;
    std::set<std::pair<int, int>> done;
    open.push({0, paces.start_ms, 0, start});
    while (!open.empty() && done.size() < 100000){
        const StandInNode node = open.top();
        open.pop();
        if (!done.insert({node.pos.x, node.pos.y}).second){
            continue;
        }
        if (in_goal(bench, node.pos)){
            return HumanBaseline{node.ms, node.risk, 0, "stand-in"};
        }
        push_steps(open, bench, map, paces, node);
    }
    return std::nullopt;
}

//  For a case with no human route: the shortest safe walk from its slowest
//  start, run the whole way, with no stops (as a player who sees the map
//  walks). Nothing if no walk reaches the goal.
std::optional<HumanBaseline> stand_in(const BenchCase& bench){
    std::optional<HumanBaseline> slowest;
    for (const TilePos& start : bench.from){
        const std::optional<HumanBaseline> walk = stand_in_from(bench, start);
        if (walk && (!slowest || slowest->ms < walk->ms)){
            slowest = walk;
        }
    }
    return slowest;
}

std::optional<HumanBaseline> human_baseline(const BenchCase& bench){
    if (!bench.human){
        return stand_in(bench);
    }
    BenchCase human = bench;
    human.legs = *bench.human;
    const RouteScore score = score_case(human);
    return HumanBaseline{score.nominal_ms, score.recoveries, score.stops, bench.human_source};
}

//  "1.23", with "!" past 1.5: a plan that far behind the human is flagged.
std::string ratio_text(double ratio){
    return std::format("{:.2f}{}", ratio, ratio > 1.5 ? "!" : "");
}

//  slower | riskier | more backtracking than the human, and the human's time.
std::string human_columns(const RouteScore& score, const std::optional<HumanBaseline>& human){
    if (!human || human->ms <= 0){
        return "- | - | - | no human baseline";
    }
    const double slower = score.expected_ms / (double)human->ms;
    const double riskier = (double)(score.recoveries + 1) / (double)(human->risk_tiles + 1);
    const double backtracking = (double)(score.stops + score.replans + 1) / (double)(human->stops + 1);
    return std::format("{} | {} | {} | {} ms ({})", ratio_text(slower), ratio_text(riskier), ratio_text(backtracking),
        human->ms, human->source);
}

std::string bench_line(const BenchCase& bench, const RouteScore& score){
    std::string line = std::format("{} | {} | {} | {} | {} | {} | {} | {} | {}", bench.name, (long long)(score.expected_ms + 0.5),
        score.worst_ms, score.nominal_ms, score.stops, score.recoveries, score.replans,
        human_columns(score, human_baseline(bench)), Pokemon::verdict_name(score.verdict));
    return score.why.empty() ? line : line + " (" + score.why + ")";
}

std::string bench_text(){
    std::string out = BENCH_HEADER;
    for (const BenchCase& bench : bench_cases()){
        out += bench_line(bench, score_case(bench)) + "\r\n";
    }
    return out;
}

//  A bench line's columns.
std::vector<std::string> bench_fields(const std::string& line){
    std::vector<std::string> fields;
    size_t begin = 0;
    while (true){
        const size_t end = line.find(" | ", begin);
        fields.push_back(line.substr(begin, end == std::string::npos ? std::string::npos : end - begin));
        if (end == std::string::npos){
            return fields;
        }
        begin = end + 3;
    }
}

//  A bench line's expected time and whether it passed.
struct GoldenRow{
    long long expected_ms;
    bool pass;
    std::vector<double> ratios;     //  slower, riskier, backtracking; empty without a human
};
//  The rows of a bench text, by case name.
std::map<std::string, GoldenRow> golden_rows(const std::string& text){
    std::map<std::string, GoldenRow> rows;
    size_t begin = 0;
    bool body = false;      //  past the header's blank line
    while (begin < text.size()){
        const size_t end = std::min(text.find('\n', begin), text.size());
        std::string line = text.substr(begin, end - begin);
        begin = end + 1;
        std::erase(line, '\r');
        const size_t first = line.find(" | ");
        if (!body || first == std::string::npos){
            body = body || line.empty();
            continue;
        }
        const std::vector<std::string> fields = bench_fields(line);
        GoldenRow row{std::atoll(fields[1].c_str()), line.find(" | PASS") != std::string::npos, {}};
        for (size_t i = 7; i < 10 && i < fields.size() && fields[i] != "-"; i++){
            row.ratios.push_back(std::atof(fields[i].c_str()));
        }
        rows[fields[0]] = std::move(row);
    }
    return rows;
}

std::string bench_golden_path(){
    const char* path = std::getenv("PA_ROUTE_BENCH_GOLDEN");
    if (path != nullptr && path[0] != '\0'){
        return path;
    }
    const std::string self = __FILE__;
    const size_t slash = self.find_last_of("/\\");
    return (slash == std::string::npos ? std::string() : self.substr(0, slash + 1)) + "Data/route_bench.golden.txt";
}

//  Whether any of `now`'s human ratios grew from `was`'s.
bool ratio_grew(const GoldenRow& was, const GoldenRow& now){
    for (size_t i = 0; i < std::min(was.ratios.size(), now.ratios.size()); i++){
        if (now.ratios[i] > was.ratios[i] + 0.005){
            return true;
        }
    }
    return false;
}

//  Each route of `now` that scores worse than in `golden`.
std::string worse_routes(const std::string& golden, const std::string& now){
    const std::map<std::string, GoldenRow> before = golden_rows(golden);
    std::string out;
    for (const auto& [name, row] : golden_rows(now)){
        const auto was = before.find(name);
        if (was == before.end()){
            continue;
        }
        if ((was->second.pass && !row.pass) || row.expected_ms > was->second.expected_ms || ratio_grew(was->second, row)){
            out += std::format("\n  {}: {} ms{} -> {} ms{}", name, was->second.expected_ms, was->second.pass ? "" : " (not PASS)",
                row.expected_ms, row.pass ? "" : " (not PASS)");
        }
    }
    return out;
}

UnitTestResult test_route_bench(Logger& logger){
    const std::string path = bench_golden_path();
    const std::string now = bench_text();
    logger.log("Route bench:\n" + now);
    const char* update = std::getenv("PA_UPDATE_ROUTE_BENCH");
    if (update != nullptr && update[0] != '\0' && update[0] != '0'){
        FileIO file;
        if (!file.open(path, FileMode::WRITE | FileMode::BINARY) || file.write(now) != now.size()){
            return "Could not write " + path + ".";
        }
        logger.log("Wrote " + path + ".");
        return true;
    }
    std::string golden;
    if (!file_to_string(path, golden)){
        return "Could not read " + path + " (set PA_ROUTE_BENCH_GOLDEN to its path).";
    }
    const std::string worse = worse_routes(golden, now);
    if (worse.empty()){
        return true;
    }
    return "Routes score worse than " + path + " (rerun with PA_UPDATE_ROUTE_BENCH=1 if meant):" + worse;
}


struct RouteBenchInput{
    std::string name;
    UnitTestResult (*test)(Logger&);
};

struct RouteBenchExpectation{
    const char* description;
};

struct RouteBenchCase{
    RouteBenchInput input;
    RouteBenchExpectation expectation;
};

class Test_AutoStoryRouteBench : public UnitTest{
public:
    explicit Test_AutoStoryRouteBench(RouteBenchCase scenario)
        : UnitTest("PokemonBDSP::AutoStory - Route bench: " + scenario.input.name)
        , m_case(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.test(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RouteBenchCase m_case;
};

const RouteBenchCase ROUTE_BENCH_CASES[] = {
    {
        .input = {.name = "no route scores worse than Data/route_bench.golden.txt", .test = test_route_bench},
        .expectation = {.description = "every route's verdict and expected time at least as good as the golden file's"},
    },
};

} // namespace


void add_tests_AutoStory_RouteBench(UnitTestDatabase& database){
    for (const auto& scenario : ROUTE_BENCH_CASES){
        database.add<Test_AutoStoryRouteBench>(scenario);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

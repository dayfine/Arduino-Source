/*  AutoStory Sim Golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the overworld sim (OverworldSim, through OverworldStepSim) does
 *  with each row of the story, kept as goldens so that the shared sim core
 *  (seam 3) can move FRLG's sim onto it and show that nothing changed:
 *
 *  - sim_golden_rows.txt: each row with a start, run clean from it: where
 *    each step leaves the player, the maps it warped from, its failure,
 *    and for each stretch on one sim its clock, counters and the screens
 *    it showed. A step the sim does not play puts the player on its
 *    pose_after, on a new sim, as the fault sweep does.
 *  - sim_golden_faults.txt: the fault sweep's outcome for each row and
 *    fault class (PokemonFRLG_AutoStoryFaultSweep_Tests.cpp): its result
 *    and the lines it logs.
 *
 *  Rewrite both with PA_UPDATE_SIM_GOLDENS=1 (Pokemon_SimGolden.h).
 *
 */

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimGolden.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFaultSweep_Tests.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStorySimGolden_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The maps as the fault sweep loads them: flagged after the Hall of Fame
//  and for a new game, the same set for every row.
const std::vector<MapData>& golden_maps(){
    static const std::vector<MapData> maps = []{
        const std::vector<MapData> all = overworld_maps();
        std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
        flags.merge(map_flag_set(all, "new_game"));
        return overworld_maps(flags);
    }();
    return maps;
}

bool map_loaded(const std::string& map){
    const std::vector<MapData>& maps = golden_maps();
    return std::any_of(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == map; });
}

//  Whether the overworld sim plays `step` (OverworldStepSim): the rest is stubbed.
bool sim_plays(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
    case StoryStepKind::STEP:
    case StoryStepKind::BATTLE:
        return true;
    case StoryStepKind::GAME:
        return step.action == CUT_TREE_VARIANT;
    default:
        return false;
    }
}

//  A script the sim does not have, which the fault sweep lets pass: a
//  scene's trigger stepped onto, or a leader's battle after a talk.
bool unscripted(const std::string& failure){
    return failure.find(": no trainer saw the player.") != std::string::npos
        || failure.find(": no_battle.") != std::string::npos;
}

std::string pose_text(const MapPosition& position, Direction facing){
    return std::format("{} facing {}", to_string(position), Pokemon::direction_name(facing));
}

//  What one sim showed and counted, from its start.
std::string sim_line(const SimState& state){
    std::string out = std::format(
        "  sim: {} ms, {} steps, {} warps, {} wild, {} signs, {} turn taps dropped, {} step taps ignored, {} moved step taps",
        state.time.count(), state.steps, state.warps, state.wild_battles, state.signs_read,
        state.turn_taps_dropped, state.step_taps_ignored, state.moved_step_taps
    );
    for (const std::string& trainer : state.trainers_beaten){
        out += "; beat " + trainer;
    }
    for (const std::string& violation : state.violations){
        out += "; violation: " + violation;
    }
    out += "; screens";
    for (const SimShown& shown : state.shown){
        out += std::format(" {}@{}", sim_screen_name(shown.screen), shown.at.count());
    }
    return out;
}

//  Row `row` run clean from its start, a line per step and per sim.
void golden_row(const StoryCheckpoint& row, std::vector<std::string>& lines){
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = golden_maps();
    const Direction facing = at.facing.value_or(Direction::SOUTH);
    lines.push_back(std::format("{} from {}", row.id, pose_text(MapPosition{at.map, at.tile}, facing)));
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, facing);
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        const size_t warps_before = sim->state().warped_from.size();
        std::string failure = row_step_on_sim(*sim, at, step, row.id);
        const bool let_pass = unscripted(failure);
        std::string line = std::format("  {} {}: {}", step.name, Pokemon::story_step_kind_name(step.kind), pose_text(sim->state().position, sim->state().facing));
        const std::vector<std::string>& warped = sim->state().warped_from;
        for (size_t w = warps_before; w < warped.size(); w++){
            line += " (warped from " + warped[w] + ")";
        }
        line += let_pass ? " [unscripted, let pass]" : sim_plays(step) ? "" : " [stubbed]";
        if (!failure.empty() && !let_pass){
            lines.push_back(line + " FAILED: " + failure);
            break;
        }
        lines.push_back(line);
        if ((sim_plays(step) && !let_pass) || !step.pose_after){
            continue;
        }
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        const std::optional<std::string> hidden = let_pass ? talk_battle_hide_flag(step) : std::nullopt;
        if (hidden){
            for (MapData& map : maps){
                hide_objects(map, {*hidden});
            }
        }
        const MapPosition land{at.map, at.tile};
        if (!map_loaded(land.map)){
            lines.push_back("  ends on " + land.map + ", which has no map");
            break;
        }
        if (!(sim->state().position == land) || hidden){
            lines.push_back(sim_line(sim->state()));
            sim = std::make_unique<OverworldSim>(maps, land, at.facing.value_or(sim->state().facing));
        }
    }
    lines.push_back(sim_line(sim->state()));
}

UnitTestResult test_rows_golden(Logger& logger, CancellableScope&){
    std::vector<std::string> lines;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.start && story_pose(Pokemon::pose_of(*row.start)) && map_loaded(story_pose(Pokemon::pose_of(*row.start))->map)){
            golden_row(row, lines);
        }
    }
    const std::string text = Pokemon::sim_golden_text(
        "FRLG AutoStory rows on the overworld sim, clean (PokemonFRLG_AutoStorySimGolden_Tests.cpp)", lines
    );
    return Pokemon::check_sim_golden(logger, Pokemon::sim_golden_path(__FILE__, "sim_golden_rows.txt"), text);
}


//  Keeps the lines a sweep logs.
class LineLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
    std::vector<std::string> lines;
};

std::string result_name(UnitTestResult::Result result){
    switch (result){
    case UnitTestResult::PASSED:
        return "passed";
    case UnitTestResult::FAILED:
        return "failed";
    case UnitTestResult::SKIPPED:
        return "skipped";
    default:
        return "not run";
    }
}

//  `text`'s lines, each indented by `indent`.
void add_indented(const std::string& text, const std::string& indent, std::vector<std::string>& lines){
    size_t begin = 0;
    while (begin <= text.size()){
        const size_t end = std::min(text.find('\n', begin), text.size());
        lines.push_back(indent + text.substr(begin, end - begin));
        begin = end + 1;
    }
}

UnitTestResult test_faults_golden(Logger& logger, CancellableScope& scope){
    UnitTestDatabase sweep;
    add_tests_AutoStoryFaultSweep(sweep);
    std::vector<std::string> lines;
    for (const auto& item : sweep){
        LineLogger captured;
        UnitTestResult result;
        try{
            result = item.second->run(captured, scope);
        }catch (Exception& e){
            result = std::string(e.name()) + ": " + e.message();
        }
        lines.push_back(item.first + ": " + result_name(result.result));
        if (!result.message.empty()){
            add_indented(result.message, "  | ", lines);
        }
        for (const std::string& line : captured.lines){
            add_indented(line, "  ", lines);
        }
    }
    const std::string text = Pokemon::sim_golden_text(
        "FRLG AutoStory fault sweep outcomes (PokemonFRLG_AutoStorySimGolden_Tests.cpp)", lines
    );
    return Pokemon::check_sim_golden(logger, Pokemon::sim_golden_path(__FILE__, "sim_golden_faults.txt"), text);
}


struct SimGoldenInput{
    std::string name;
    UnitTestResult (*body)(Logger& logger, CancellableScope& scope);
};

struct SimGoldenExpectation{
    std::string_view description;
};

struct SimGoldenTestCase{
    SimGoldenInput input;
    SimGoldenExpectation expectation;
};

class Test_AutoStorySimGolden : public UnitTest{
public:
    explicit Test_AutoStorySimGolden(SimGoldenTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Sim golden: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        try{
            return m_scenario.input.body(logger, scope);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    SimGoldenTestCase m_scenario;
};

const SimGoldenTestCase SIM_GOLDEN_CASES[] = {
    {.input = {.name = "rows on the overworld sim, clean", .body = test_rows_golden}, .expectation = {.description = "each row run clean matches sim_golden_rows.txt"}},
    {.input = {.name = "fault sweep outcomes", .body = test_faults_golden}, .expectation = {.description = "each fault sweep's outcome matches sim_golden_faults.txt"}},
};

} // namespace


void add_tests_AutoStorySimGolden(UnitTestDatabase& database){
    for (const SimGoldenTestCase& scenario : SIM_GOLDEN_CASES){
        database.add<Test_AutoStorySimGolden>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

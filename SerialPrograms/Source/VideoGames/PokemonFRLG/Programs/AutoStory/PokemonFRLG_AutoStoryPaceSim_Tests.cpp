/*  AutoStory Pace Sim Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pacing on the OverworldSim (SimPatch) where something other than a wild
 *  battle stops it.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPaceSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::Direction;

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

class LinesLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& line, Color) override{ lines.push_back(line); }
    std::vector<std::string> lines;
};

//  A capture on the sim: battles found by pacing `patch`, each the target,
//  caught with the first ball.
class SimCapture : public CaptureActions{
public:
    SimCapture(Logger& logger, OverworldSim& sim, SimPatch& patch)
        : m_logger(logger), m_sim(sim), m_patch(patch)
    {}
    virtual bool find_encounter(Direction) override{
        return pace_until_battle(m_logger, m_patch, PACE_MAX_STEPS);
    }
    virtual EncounterSeen read_encounter() override{ return EncounterSeen{.species = "oddish"}; }
    virtual bool flee() override{ return m_sim.handle_battle(SimBattleChoice::RUN) == SimBattleEnd::ENDED; }
    virtual FoeHp read_foe_hp() override{ return FoeHp::UNKNOWN; }
    virtual TurnEnd attack(size_t) override{ return TurnEnd::FAILED; }
    virtual ThrowEnd throw_ball() override{ return ThrowEnd::CAUGHT; }
    virtual bool finish_catch(CatchDestination) override{
        return m_sim.handle_battle(SimBattleChoice::FIGHT) == SimBattleEnd::ENDED;
    }
private:
    Logger& m_logger;
    OverworldSim& m_sim;
    SimPatch& m_patch;
};

//  Console B, 2026-11-03 (m63, 00.20 run 2): the capture paced on Route
//  24's (5, 19), in Camper Shane's sight; after the first wild battle he
//  walked up ("I saw your feat from the grass!"), and pacing ended
//  "no-battle-not-overworld" under his message. On the sim, a capture
//  pacing east from (4, 19) onto (5, 19) fights him, walks back onto the
//  patch from either tile, as pace_trainer_battle() does, and goes on to
//  its catch.
UnitTestResult test_capture_paced_in_trainer_sight(){
    const MapPosition patch{"Route24", {4, 19}};
    const CapturePlan plan{.species = "oddish", .max_encounters = 5, .pace = Direction::EAST};
    const StoryStep capture = capture_step("catch_in_sight", "", plan);
    SimScript script;
    //  The first step onto (5, 19) is seen; the battle comes on a later one.
    script.encounters.push_back(SimEncounter{.grass_step = 3});
    OverworldSim sim(story_maps(), patch, Direction::SOUTH, script);
    std::string walk_failure;
    SimPatch pacing(sim, Direction::EAST, Direction::WEST, [&]{
        const Pokemon::Pose paced = paced_pose(Pokemon::tile_pose(patch.map, {patch.point}), capture);
        const std::optional<std::vector<RouteStep>> route = plan_story_route(
            story_maps(), paced, std::nullopt, RouteGoal{patch, std::nullopt}
        );
        const RoutePlanning planning{.maps = &story_maps(), .start = std::nullopt, .label = "back_after_trainer"};
        walk_failure = route ? OverworldSimWalker(sim).run(*route, planning) : "no route back onto the patch.";
    });
    LinesLogger logger;
    SimCapture actions(logger, sim, pacing);
    const CaptureResult result = run_capture(logger, actions, plan, 2);
    if (result.end != CaptureEnd::CAUGHT || !walk_failure.empty()){
        return capture_message(plan, result) + " " + walk_failure + joined(logger.lines) + joined(sim.state().log);
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"camper"} || !sim.state().violations.empty()){
        return std::format("Expected Camper Shane beaten and no violations; got {} beaten.", sim.state().trainers_beaten.size())
            + joined(sim.state().violations);
    }
    const bool logged = std::any_of(logger.lines.begin(), logger.lines.end(), [](const std::string& line){
        return line.starts_with("Pacing: trainer battle during pacing: camper");
    });
    if (!logged || result.encounters != 1){
        return std::format("Expected the trainer's battle logged and 1 wild encounter, got {}.", result.encounters) + joined(logger.lines);
    }
    return true;
}


struct AutoStoryPaceSimInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryPaceSimExpectation{
    const char* description;
};

struct AutoStoryPaceSimTestCase{
    AutoStoryPaceSimInput input;
    AutoStoryPaceSimExpectation expectation;
};

class Test_AutoStoryPaceSim : public UnitTest{
public:
    explicit Test_AutoStoryPaceSim(AutoStoryPaceSimTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Pace Sim: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryPaceSimTestCase m_scenario;
};

const AutoStoryPaceSimTestCase AUTO_STORY_PACE_SIM_CASES[] = {
    {
        .input = {
            .name = "a capture paced in a trainer's sight fights them and goes on",
            .body = test_capture_paced_in_trainer_sight,
        },
        .expectation = {
            .description = "handles trainer encounter while pacing and resumes capture sequence",
        },
    },
};

} // namespace


void add_tests_AutoStoryPaceSim(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_PACE_SIM_CASES){
        database.add<Test_AutoStoryPaceSim>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  FRLG AutoStory - Console failure replay tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Every fault file (Pokemon_LegReplay.h) of a console failure runs here on
 *  the overworld simulation: the maps give the walls, trainers and signs,
 *  only the file's faults are injected, and the walk must end where its
 *  route says. No case has code of its own: a new console failure is a new
 *  file in ConsoleFailures/.
 *
 *  The walk is the file's "walk": the event's name and its legs, as the
 *  story's "legs" arguments give them, from the file's "start".
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/Navigation/Pokemon_LegReplay.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"
#include "PokemonFRLG_AutoStoryConsoleFailureData.h"
#include "PokemonFRLG_AutoStoryConsoleFailure_Tests.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

using Pokemon::ConsoleFailure;
using Pokemon::ReplayFault;
using Pokemon::ReplayFaultKind;

Direction direction_of(const std::string& text){
    for (const Direction direction : Pokemon::ALL_DIRECTIONS){
        if (direction_name(direction) == text){
            return direction;
        }
    }
    throw ParseException("console failure: unknown direction \"" + text + "\"");
}

//  The "walk" of a fault file: its event, and its legs from the file's start.
struct FaultWalk{
    std::string event;
    std::vector<Pokemon::StoryLeg> legs;
};

FaultWalk walk_of(const JsonObject& json){
    const JsonObject& walk = json.get_object_throw("walk");
    FaultWalk out{.event = walk.get_string_throw("event"), .legs = {}};
    for (const JsonValue& leg : walk.get_array_throw("legs")){
        const JsonArray& pair = leg.to_array_throw();
        out.legs.push_back({direction_of(pair[0].to_string_throw()), (int)pair[1].to_integer_throw()});
    }
    return out;
}

//  The faults as the sim's script: the holds that moved less.
SimScript script_of(const ConsoleFailure& failure){
    SimScript script;
    for (const ReplayFault& fault : failure.faults){
        if (fault.kind != ReplayFaultKind::MOVED_LESS){
            throw ParseException(std::format(
                "{}: the fault \"{}\" is not one the overworld simulation injects",
                failure.name, Pokemon::replay_fault_kind_name(fault.kind)
            ));
        }
        script.short_holds[(int)fault.index] = (int)fault.tiles;
    }
    return script;
}

struct ReplayInput{
};
struct ReplayExpectation{
    std::string description;
    //  The walk must end where its route says: the walker stopping itself
    //  is not enough, for a failure the maps hold what it needs to recover.
    bool arrives = false;
};
const std::string& fault_text(const std::string& name){
    for (const auto& [file, text] : console_failure_json()){
        if (file == name){
            return text;
        }
    }
    throw ParseException(name + ": no such file in ConsoleFailures/");
}

struct ReplayCase{
    std::string name;
    ReplayInput input;
    ReplayExpectation expectation;
};

class Test_ConsoleFailureReplay : public UnitTest{
public:
    explicit Test_ConsoleFailureReplay(ReplayCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Console failure replay: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return replay();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }catch (std::exception& e){
            return std::string(e.what());
        }
    }

private:
    UnitTestResult replay() const{
        const std::string& name = m_case.name;
        const std::string text = fault_text(name);
        const ConsoleFailure failure = Pokemon::parse_console_failure(name, text);
        const JsonValue json = parse_json(text);
        const FaultWalk walk = walk_of(json.to_object_throw());
        if (!failure.start || !failure.start_facing){
            return name + ": a fault file for the overworld walk names its start";
        }
        const MapPosition start{failure.start->map, failure.start->tile};
        const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), start, walk.legs);
        if (!route || route->empty()){
            for (size_t n = 1; n <= walk.legs.size(); n++){
                const std::vector<Pokemon::StoryLeg> prefix(walk.legs.begin(), walk.legs.begin() + n);
                if (!fixed_route(story_maps(), start, prefix)){
                    return name + ": leg " + std::to_string(n) + " is blocked";
                }
            }
            return name + ": the walk's legs are not a route from " + to_string(start);
        }
        OverworldSim sim(story_maps(), start, *failure.start_facing, script_of(failure));
        OverworldSimWalker walker(sim);
        const RoutePlanning planning{.maps = &story_maps(), .start = start, .label = walk.event, .checkpoint = failure.checkpoint};
        const std::string failed = walker.run(*route, planning);
        const bool right = failed.empty() && sim.state().position == route->back().end;
        //  The walker may stop itself instead: it did not take the move as seen.
        const bool noticed = !failed.empty();
        if (m_case.expectation.arrives && !right){
            return missed_arrival(name, failed, route->back().end, sim, walker);
        }
        const std::string fix = json.to_object_throw().get_string_default("known_failing", "");
        if (fix.empty()){
            return right || noticed ? UnitTestResult(true) : UnitTestResult(wrong_end(name, route->back().end, sim, walker));
        }
        if (right || noticed){
            return std::format("{} now passes: take its \"known_failing\" off the file", name);
        }
        return true;
    }

    static std::string wrong_end(const std::string& name, const MapPosition& planned, const OverworldSim& sim, const OverworldSimWalker& walker){
        std::string text = std::format(
            "{}: the walk ended as done on {}, the player stands on {}",
            name, to_string(planned), to_string(sim.state().position)
        );
        for (const std::string& line : walker.log()){
            text += "\n  " + line;
        }
        return text;
    }

    static std::string missed_arrival(const std::string& name, const std::string& failed, const MapPosition& planned, const OverworldSim& sim, const OverworldSimWalker& walker){
        std::string text = std::format(
            "{}: the walk must arrive on {}; the player stands on {}, and the walker {}",
            name, to_string(planned), to_string(sim.state().position), failed.empty() ? "took it as done" : "stopped: " + failed
        );
        for (const std::string& line : walker.log()){
            text += "\n  " + line;
        }
        return text;
    }

    ReplayCase m_case;
};

//  One row per file in ConsoleFailures/, named as the file without ".json".
const ReplayCase CONSOLE_FAILURE_CASES[] = {
    {
        .name = "12.03-22-22-pallet-shore-east-unmoved",
        .input = {},
        .expectation = {.description = "a hold that moved nothing must not be walked as if it had"},
    },
    {
        .name = "14.01-05-48-quiz-2-west-short",
        .input = {},
        .expectation = {.description = "an open leg that ends the walk and stopped short must not end it as arrived"},
    },
    {
        .name = "14.01-07-16-door-5-north-into-dusty",
        .input = {},
        .expectation = {
            .description = "a re-check hold that moves nothing under a person finds the player below them",
            .arrives = true,
        },
    },
};

} // namespace

void add_tests_AutoStoryConsoleFailure(UnitTestDatabase& database){
    for (const auto& scenario : CONSOLE_FAILURE_CASES){
        database.add<Test_ConsoleFailureReplay>(scenario);
    }
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

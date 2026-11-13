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
#include "PokemonFRLG_AutoStoryPacedRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

using Pokemon::ConsoleFailure;
using Pokemon::ReplayFault;
using Pokemon::ReplayFaultKind;

Direction direction_of(const std::string& text){
    for (const Direction direction : ALL_DIRECTIONS){
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
    std::string name;
    std::string text;
};
struct ReplayExpectation{
};
struct ReplayCase{
    ReplayInput input;
    ReplayExpectation expectation;
};

//  The cases are discovered from the embedded fault JSONs, so a new console
//  failure needs no C++ edit; the code-health test-case table rule does not
//  fit a data-driven test.
class Test_ConsoleFailureReplay : public UnitTest{
public:
    explicit Test_ConsoleFailureReplay(ReplayCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Console failure replay: {}", test_case.input.name))
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
        const std::string& name = m_case.input.name;
        const ConsoleFailure failure = Pokemon::parse_console_failure(name, m_case.input.text);
        const JsonValue json = parse_json(m_case.input.text);
        const FaultWalk walk = walk_of(json.to_object_throw());
        if (!failure.start || !failure.start_facing){
            return name + ": a fault file for the overworld walk names its start";
        }
        const MapPosition start{failure.start->map, failure.start->tile};
        const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), start, walk.legs);
        if (!route || route->empty()){
            return name + ": the walk's legs are not a route from " + to_string(start);
        }
        OverworldSim sim(story_maps(), start, *failure.start_facing, script_of(failure));
        OverworldSimWalker walker(sim);
        const RoutePlanning planning{.maps = &story_maps(), .start = start, .label = walk.event, .checkpoint = failure.checkpoint};
        const std::string failed = walker.run(*route, planning);
        const bool right = failed.empty() && sim.state().position == route->back().end;
        //  The walker may stop itself instead: it did not take the move as seen.
        const bool noticed = !failed.empty();
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

    ReplayCase m_case;
};

} // namespace

void add_tests_AutoStoryConsoleFailure(UnitTestDatabase& database){
    for (const auto& [name, text] : console_failure_json()){
        database.add<Test_ConsoleFailureReplay>(ReplayCase{.input = {name, text}, .expectation = {}});
    }
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

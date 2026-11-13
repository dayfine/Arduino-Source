/*  Story RNG Handoff Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryRngHandoff.h"
#include "Pokemon_StoryRngHandoff_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  rng_handoff_mode()'s two options.
struct ModeInput{
    bool stop;
    bool run;
};

struct ModeCase{
    ModeInput input;
    RngHandoffMode expected;
};

const ModeCase MODE_CASES[] = {
    {.input = {.stop = false, .run = false}, .expected = RngHandoffMode::OFF},
    {.input = {.stop = true, .run = false}, .expected = RngHandoffMode::STOP},
    {.input = {.stop = false, .run = true}, .expected = RngHandoffMode::RUN},
    {.input = {.stop = true, .run = true}, .expected = RngHandoffMode::RUN},
};

class Test_RngHandoffMode : public UnitTest{
public:
    Test_RngHandoffMode()
        : UnitTest("Pokemon::StoryRngHandoff - RUN takes precedence over STOP")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const ModeCase& item : MODE_CASES){
            if (rng_handoff_mode(item.input.stop, item.input.run) != item.expected){
                return std::format(
                    "stop={}, run={}: wrong mode", item.input.stop, item.input.run
                );
            }
        }
        return true;
    }
};

//  A success is an ok Outcome and calls the program once.
class Test_RngHandoffSuccess : public UnitTest{
public:
    Test_RngHandoffSuccess()
        : UnitTest("Pokemon::StoryRngHandoff - a success is ok")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        int calls = 0;
        const Outcome outcome = run_rng_handoff(logger, "Test RNG", [&]{
            calls++;
            return RngHandoffResult{.summary = "2 resets"};
        });
        if (!outcome.succeeded()){
            return "Expected an ok outcome";
        }
        if (calls != 1){
            return std::format("Expected 1 call, got {}", calls);
        }
        return true;
    }
};

//  A failure throws, once: nothing reruns the program.
class Test_RngHandoffFailure : public UnitTest{
public:
    Test_RngHandoffFailure()
        : UnitTest("Pokemon::StoryRngHandoff - a failure throws without a retry")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        int calls = 0;
        try{
            run_rng_handoff(logger, "Test RNG", [&]{
                calls++;
                return RngHandoffResult{.failure = "out of resets"};
            });
        }catch (OperationFailedException&){
            if (calls != 1){
                return std::format("Expected 1 call, got {}", calls);
            }
            return true;
        }
        return "Expected OperationFailedException";
    }
};

//  The user's turn is returned, not thrown, once.
class Test_RngHandoffUserTurn : public UnitTest{
public:
    Test_RngHandoffUserTurn()
        : UnitTest("Pokemon::StoryRngHandoff - a user's turn is returned, not an error")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        int calls = 0;
        const std::optional<std::string> turn = run_rng_handoff_or_user_turn(logger, "Test RNG", [&]{
            calls++;
            return RngHandoffResult{.user_turn = "save by hand"};
        });
        if (!turn || *turn != "save by hand"){
            return "Expected the user's turn";
        }
        if (calls != 1){
            return std::format("Expected 1 call, got {}", calls);
        }
        const std::optional<std::string> none = run_rng_handoff_or_user_turn(logger, "Test RNG", []{
            return RngHandoffResult{.summary = "shiny"};
        });
        if (none){
            return "A success has no user's turn";
        }
        try{
            run_rng_handoff_or_user_turn(logger, "Test RNG", []{
                return RngHandoffResult{.failure = "out of resets", .user_turn = "ignored"};
            });
        }catch (OperationFailedException&){
            return true;
        }
        return "A failure must throw even with a user's turn";
    }
};

} // namespace


void add_tests_StoryRngHandoff(UnitTestDatabase& database){
    database.add<Test_RngHandoffMode>();
    database.add<Test_RngHandoffSuccess>();
    database.add<Test_RngHandoffFailure>();
    database.add<Test_RngHandoffUserTurn>();
}


} // namespace Pokemon
} // namespace PokemonAutomation

/*  AutoStory RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the run says after a shiny, and the rows it takes then.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStoryRngLoop : public UnitTest{
public:
    Test_AutoStoryRngLoop(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStoryRngLoop - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};



//  What the run says after a shiny, by what it tells of the SID.
UnitTestResult test_shiny_messages(){
    struct Case{
        StarterRngLoopResult result;
        std::string expected;
    };
    const std::vector<Case> cases = {
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 2, .remaining_sids = {300}, .sid = uint16_t(300)},
            .expected = "Starter RNG: shiny Squirtle. The SID is 300: enter it as the RNG plan's SID.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {100, 200}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle. The SID is one of 100,200.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY, .attempts_run = 1, .remaining_sids = {}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle, for none of the SID candidates left: the SID isn't one of SID Helper's candidates.",
        },
        {
            .result = {.end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = 1, .remaining_sids = {100, 200}, .sid = std::nullopt},
            .expected = "Starter RNG: shiny Squirtle, but not the planned target, so it doesn't tell the SID. "
                "SIDs left: 100,200.",
        },
    };
    for (const Case& item : cases){
        const std::string got = starter_rng_loop_message(item.result);
        if (got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + got + "\".";
        }
    }
    return true;
}


//  After a shiny, 00.04b picks up at the rival's trigger and runs on.
UnitTestResult test_rows_after_shiny(){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    size_t ball_row = 0;
    size_t parcel_row = 0;
    for (size_t c = 0; c < checkpoints.size(); c++){
        if (checkpoints[c].id == "00.04"){
            ball_row = c;
        }
        if (checkpoints[c].id == "00.05"){
            parcel_row = c;
        }
    }
    struct Case{
        size_t last;
        std::string ids;
    };
    const std::vector<Case> cases = {
        {.last = ball_row, .ids = "00.04b"},
        {.last = ball_row + 1, .ids = "00.04b"},
        {.last = parcel_row, .ids = "00.04b,00.05"},
    };
    for (const Case& item : cases){
        const std::vector<StoryCheckpoint> rows = rows_after_starter_rng(checkpoints, ball_row, item.last);
        std::string ids;
        for (const StoryCheckpoint& row : rows){
            ids += (ids.empty() ? "" : ",") + row.id;
        }
        const std::string first_step = rows.front().steps.empty() ? "" : rows.front().steps.front().name;
        if (ids != item.ids || first_step != "above_rival_trigger"){
            return std::format(
                "Expected rows {} starting at above_rival_trigger; got {} starting at {}", item.ids, ids, first_step
            );
        }
    }
    return true;
}

}


void add_tests_AutoStoryRngLoop(UnitTestDatabase& database){
    database.add<Test_AutoStoryRngLoop>("shiny messages", test_shiny_messages);
    database.add<Test_AutoStoryRngLoop>("rows after a shiny", test_rows_after_shiny);
}



}
}
}

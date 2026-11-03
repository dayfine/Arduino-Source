/*  AutoStory RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Starter RNG loop's decisions on scripted attempt outcomes and a fake
 *  clock, and the rows a run takes after a shiny.
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


class LineLogger : public Logger{
public:
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
    bool has(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }

    std::vector<std::string> lines;
};

//  Each attempt takes `minutes_per_attempt` on the clock and comes out as
//  the next of `outcomes`.
class ScriptedSteps : public StarterRngLoopSteps{
public:
    ScriptedSteps(std::vector<StarterRngOutcome> outcomes, int minutes_per_attempt)
        : m_outcomes(std::move(outcomes))
        , m_minutes(minutes_per_attempt)
    {}

    virtual WallClock now() override{
        return m_now;
    }
    virtual StarterRngOutcome run_attempt(size_t index, const RngAttempt&, WallClock deadline) override{
        if (m_next >= m_outcomes.size()){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "More attempts than scripted.");
        }
        indices.push_back(index);
        deadlines_min.push_back(int(std::chrono::duration_cast<std::chrono::minutes>(deadline - START).count()));
        m_now += std::chrono::minutes(m_minutes);
        return m_outcomes[m_next++];
    }

    std::vector<size_t> indices;
    //  Each attempt's deadline, in minutes from the start.
    std::vector<int> deadlines_min;

private:
    static inline const WallClock START = WallClock{} + std::chrono::hours(1000);
    std::vector<StarterRngOutcome> m_outcomes;
    int m_minutes;
    WallClock m_now = START;
    size_t m_next = 0;
};

//  SID Helper's candidates 100, 200, 300; attempt 1 is shiny for 100 and
//  200, attempt 2 for 300.
const std::vector<SidCandidate> CANDIDATES = {{100, 1}, {200, 2}, {300, 1}};

std::vector<RngAttempt> attempts(){
    RngAttempt first{};
    first.sids = {100, 200};
    RngAttempt second{};
    second.sids = {300};
    return {first, second};
}

//  60-minute attempts, 12 hours in all.
const StarterRngLoopLimits LIMITS{
    .attempt_time = std::chrono::minutes(60),
    .total_time = std::chrono::hours(12),
};

std::string end_name(StarterRngLoopEnd end){
    switch (end){
    case StarterRngLoopEnd::SHINY:
        return "SHINY";
    case StarterRngLoopEnd::TARGET:
        return "TARGET";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return "OUT_OF_ATTEMPTS";
    case StarterRngLoopEnd::OUT_OF_RETRIES:
        return "OUT_OF_RETRIES";
    case StarterRngLoopEnd::OUT_OF_TIME:
        return "OUT_OF_TIME";
    }
    return "?";
}

std::string join(const std::vector<size_t>& values){
    std::string text;
    for (size_t value : values){
        text += (text.empty() ? "" : ",") + std::to_string(value + 1);
    }
    return text;
}


struct LoopRow{
    std::string name;
    std::vector<StarterRngOutcome> outcomes;
    int minutes_per_attempt;
    bool shiny_required;
    StarterRngLoopEnd end;
    //  The attempts run, from 1, as "1,1,2".
    std::string attempts_run;
    //  The SID confirmed, or 0 for none.
    uint16_t sid;
    //  A log line the loop must write; empty for none.
    std::string line;
};

const std::vector<LoopRow> LOOP_ROWS = {
    {
        .name = "hit, then shiny confirms the SID",
        .outcomes = {StarterRngOutcome::target_hit, StarterRngOutcome::shiny},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,2", .sid = 300,
        .line = "RngPlan: confirmed tid=12345 sid=300 candidate=3 of=3",
    },
    {
        .name = "shiny for two SIDs confirms none",
        .outcomes = {StarterRngOutcome::shiny},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1", .sid = 0,
        .line = "RngPlan: result attempt=1 outcome=SHINY",
    },
    {
        .name = "misses retry the same attempt",
        .outcomes = {StarterRngOutcome::out_of_resets, StarterRngOutcome::no_matches, StarterRngOutcome::shiny},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,1,1", .sid = 0,
        .line = "RngPlan: result attempt=1 outcome=MISSED",
    },
    {
        .name = "too many misses stop",
        .outcomes = {StarterRngOutcome::out_of_resets, StarterRngOutcome::out_of_resets, StarterRngOutcome::wild_shiny},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::OUT_OF_RETRIES, .attempts_run = "1,1,1", .sid = 0,
        .line = "",
    },
    {
        .name = "every attempt hit, none shiny",
        .outcomes = {StarterRngOutcome::target_hit, StarterRngOutcome::target_hit},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::OUT_OF_ATTEMPTS, .attempts_run = "1,2", .sid = 0,
        .line = "RngPlan: result attempt=2 outcome=HIT_NOT_SHINY",
    },
    {
        .name = "a hit ends the plan with shiny not required",
        .outcomes = {StarterRngOutcome::target_hit},
        .minutes_per_attempt = 30, .shiny_required = false,
        .end = StarterRngLoopEnd::TARGET, .attempts_run = "1", .sid = 0,
        .line = "",
    },
    {
        .name = "no attempt starts after the total time",
        .outcomes = {StarterRngOutcome::out_of_resets, StarterRngOutcome::out_of_resets},
        .minutes_per_attempt = 400, .shiny_required = true,
        .end = StarterRngLoopEnd::OUT_OF_TIME, .attempts_run = "1,1", .sid = 0,
        .line = "RngPlan: out of time before attempt 1",
    },
};

UnitTestResult run_loop_row(const LoopRow& row){
    RngPlanTracker tracker(attempts(), CANDIDATES, row.shiny_required, 2);
    ScriptedSteps steps(row.outcomes, row.minutes_per_attempt);
    LineLogger logger;
    StarterRngLoopResult result = run_starter_rng_plan(logger, tracker, steps, LIMITS, 12345, CANDIDATES);

    const std::string expected = std::format("end {}, attempts {}, sid {}", end_name(row.end), row.attempts_run, row.sid);
    const std::string got = std::format(
        "end {}, attempts {}, sid {}", end_name(result.end), join(steps.indices), result.sid.value_or(0)
    );
    if (expected != got){
        return "Expected " + expected + "; got " + got;
    }
    if (!row.line.empty() && !logger.has(row.line)){
        return "Expected the log line \"" + row.line + "\"";
    }
    if (result.attempts_run != steps.indices.size()){
        return std::format("Expected attempts_run {}; got {}", steps.indices.size(), result.attempts_run);
    }
    return true;
}


//  An attempt's deadline is an hour after it starts, or the end of the
//  total time if sooner: attempts of 400 minutes start at 0 and 400, and
//  none at 800, past the total of 430.
UnitTestResult test_deadlines(){
    RngPlanTracker tracker(attempts(), CANDIDATES, true, 5);
    ScriptedSteps steps({StarterRngOutcome::out_of_resets, StarterRngOutcome::out_of_resets}, 400);
    LineLogger logger;
    const StarterRngLoopLimits limits{.attempt_time = std::chrono::minutes(60), .total_time = std::chrono::minutes(430)};
    run_starter_rng_plan(logger, tracker, steps, limits, 12345, CANDIDATES);
    std::string got;
    for (int deadline : steps.deadlines_min){
        got += (got.empty() ? "" : ",") + std::to_string(deadline);
    }
    if (got != "60,430"){
        return "Expected deadlines 60,430 minutes; got " + got;
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
    for (const LoopRow& row : LOOP_ROWS){
        database.add<Test_AutoStoryRngLoop>(row.name, [row]{ return run_loop_row(row); });
    }
    database.add<Test_AutoStoryRngLoop>("attempt deadlines", test_deadlines);
    database.add<Test_AutoStoryRngLoop>("rows after a shiny", test_rows_after_shiny);
}



}
}
}

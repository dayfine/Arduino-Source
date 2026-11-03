/*  AutoStory RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Starter RNG loop's decisions and log lines on scripted attempt
 *  outcomes and a fake clock, and the rows a run takes after a shiny.
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

//  Each try takes `minutes_per_attempt` on the clock, makes one reset at
//  its end with calibration 1.5/-2/0.25 frames, and comes out as the next
//  of `outcomes`. An empty one is Starter RNG refusing the timings: a
//  StarterRngTimingsRefused at once.
class ScriptedSteps : public StarterRngLoopSteps{
public:
    ScriptedSteps(std::vector<std::optional<StarterRngOutcome>> outcomes, int minutes_per_attempt)
        : m_outcomes(std::move(outcomes))
        , m_minutes(minutes_per_attempt)
    {}

    virtual WallClock now() override{
        return m_now;
    }
    virtual StarterRngResult run_attempt(const RngAttempt&, const StarterRngAttemptCall& call) override{
        if (m_next >= m_outcomes.size()){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "More attempts than scripted.");
        }
        indices.push_back(call.index);
        deadlines_min.push_back(int(std::chrono::duration_cast<std::chrono::minutes>(call.deadline - START).count()));
        const std::optional<StarterRngOutcome> outcome = m_outcomes[m_next++];
        if (!outcome){
            throw StarterRngTimingsRefused("Refused timings.");
        }
        m_now += std::chrono::minutes(m_minutes);
        call.on_reset(StarterRngProgress{.resets = 1, .calibrations = {1.5, -2, 0.25}});
        return StarterRngResult{.outcome = *outcome, .pokemon = std::nullopt};
    }

    std::vector<size_t> indices;
    //  Each attempt's deadline, in minutes from the start.
    std::vector<int> deadlines_min;

private:
    static inline const WallClock START = WallClock{} + std::chrono::hours(1000);
    std::vector<std::optional<StarterRngOutcome>> m_outcomes;
    int m_minutes;
    LineLogger m_logger;
    WallClock m_now = START;
    size_t m_next = 0;
};

//  A setup problem Starter RNG hits on every attempt alike, such as no
//  audio input.
class SetupErrorSteps : public StarterRngLoopSteps{
public:
    virtual WallClock now() override{
        return WallClock{} + std::chrono::hours(1000);
    }
    virtual StarterRngResult run_attempt(const RngAttempt&, const StarterRngAttemptCall&) override{
        calls++;
        throw UserSetupError(m_logger, "No sound detected.");
    }

    size_t calls = 0;

private:
    LineLogger m_logger;
};

//  SID Helper's candidates 100, 200, 300; attempt 1 is shiny for 100 and
//  200, attempt 2 for 300.
const std::vector<SidCandidate> CANDIDATES = {{100, 1}, {200, 2}, {300, 1}};

std::vector<RngAttempt> attempts(){
    RngAttempt first{};
    first.target.seed.seed = 0xceff;
    first.target.advances = 983;
    first.sids = {100, 200};
    RngAttempt second{};
    second.target.seed.seed = 0x2ef6;
    second.target.advances = 29364;
    second.sids = {300};
    return {first, second};
}

//  60-minute tries, 12 hours in all, 50 resets a try, 2 retries.
const StarterRngLoopLimits LIMITS{
    .attempt_time = std::chrono::minutes(60),
    .total_time = std::chrono::hours(12),
    .max_resets = 50,
    .max_retries = 2,
};

std::string end_name(StarterRngLoopEnd end){
    switch (end){
    case StarterRngLoopEnd::SHINY:
        return "SHINY";
    case StarterRngLoopEnd::SHINY_OFF_TARGET:
        return "SHINY_OFF_TARGET";
    case StarterRngLoopEnd::TARGET:
        return "TARGET";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return "OUT_OF_ATTEMPTS";
    case StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT:
        return "MISSED_EVERY_ATTEMPT_LEFT";
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
    //  Empty: Starter RNG refuses the attempt's timings.
    std::vector<std::optional<StarterRngOutcome>> outcomes;
    int minutes_per_attempt;
    bool shiny_required;
    StarterRngLoopEnd end;
    //  The attempts run, from 1, as "1,1,2".
    std::string attempts_run;
    //  The SID confirmed, or 0 for none; with it, an "RngPlan: confirmed" line.
    uint16_t sid;
    //  Log lines the loop must write.
    std::vector<std::string> lines;
};

using enum StarterRngOutcome;
const std::optional<StarterRngOutcome> REFUSED = std::nullopt;

const std::vector<LoopRow> LOOP_ROWS = {
    {
        .name = "hit, then shiny confirms the SID",
        .outcomes = {target_hit, shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,2", .sid = 300,
        .lines = {
            "RngPlan: state attempt=1/2 try=1/3 reset=0/50 seed=CEFF advances=983 sids=100,200 ruled_out=- "
                "attempt_time=00:00:00/01:00:00 total_time=00:00:00/12:00:00 calibration=-",
            "RngPlan: state attempt=1/2 try=1/3 reset=1/50 seed=CEFF advances=983 sids=100,200 ruled_out=- "
                "attempt_time=00:30:00/01:00:00 total_time=00:30:00/12:00:00 calibration=1.50/-2.00/0.25",
            "RngPlan: result attempt=1 try=1 outcome=hit_not_shiny starter_rng=target_hit nature=- ivs=- pid=- next=next_attempt",
            "RngPlan: state attempt=2/2 try=1/3 reset=0/50 seed=2EF6 advances=29364 sids=300 ruled_out=100,200 "
                "attempt_time=00:00:00/01:00:00 total_time=00:30:00/12:00:00 calibration=-",
            "RngPlan: result attempt=2 try=1 outcome=shiny starter_rng=shiny_on_target nature=- ivs=- pid=- next=confirm",
            "RngPlan: confirmed tid=12345 sid=300 candidate=3 of=3",
        },
    },
    {
        .name = "shiny on the target for two SIDs confirms none",
        .outcomes = {shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1", .sid = 0,
        .lines = {"RngPlan: result attempt=1 try=1 outcome=shiny starter_rng=shiny_on_target nature=- ivs=- pid=- next=confirm"},
    },
    {
        .name = "shiny off the target confirms nothing",
        .outcomes = {target_hit, shiny_off_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = "1,2", .sid = 0,
        .lines = {"RngPlan: result attempt=2 try=1 outcome=shiny_off_target starter_rng=shiny_off_target nature=- ivs=- pid=- next=stop"},
    },
    {
        .name = "a shiny not known to be the target confirms nothing",
        .outcomes = {shiny},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY_OFF_TARGET, .attempts_run = "1", .sid = 0,
        .lines = {"RngPlan: result attempt=1 try=1 outcome=shiny_off_target starter_rng=shiny nature=- ivs=- pid=- next=stop"},
    },
    {
        .name = "misses retry the same attempt",
        .outcomes = {out_of_resets, no_matches, shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,1,1", .sid = 0,
        .lines = {
            "RngPlan: result attempt=1 try=1 outcome=missed starter_rng=out_of_resets nature=- ivs=- pid=- next=retry",
            "RngPlan: result attempt=1 try=2 outcome=missed starter_rng=no_matches nature=- ivs=- pid=- next=retry",
            "RngPlan: result attempt=1 try=3 outcome=shiny starter_rng=shiny_on_target nature=- ivs=- pid=- next=confirm",
        },
    },
    {
        .name = "an attempt past its deadline",
        .outcomes = {out_of_resets, shiny_on_target},
        .minutes_per_attempt = 90, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,1", .sid = 0,
        .lines = {"RngPlan: result attempt=1 try=1 outcome=missed starter_rng=attempt_deadline nature=- ivs=- pid=- next=retry"},
    },
    {
        .name = "retries used up, then the next attempt",
        .outcomes = {out_of_resets, no_matches, wild_shiny, shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,1,1,2", .sid = 300,
        .lines = {
            "RngPlan: result attempt=1 try=3 outcome=missed starter_rng=wild_shiny nature=- ivs=- pid=- next=next_attempt",
            "RngPlan: result attempt=2 try=1 outcome=shiny starter_rng=shiny_on_target nature=- ivs=- pid=- next=confirm",
            "RngPlan: confirmed tid=12345 sid=300 candidate=3 of=3",
        },
    },
    {
        .name = "every attempt left missed too often",
        .outcomes = {out_of_resets, out_of_resets, out_of_resets, no_matches, no_matches, no_matches},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT, .attempts_run = "1,1,1,2,2,2", .sid = 0,
        .lines = {"RngPlan: result attempt=2 try=3 outcome=missed starter_rng=no_matches nature=- ivs=- pid=- next=stop"},
    },
    {
        .name = "miss, hit, miss, then shiny",
        .outcomes = {out_of_resets, target_hit, no_matches, shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,1,2,2", .sid = 300,
        .lines = {
            "RngPlan: result attempt=1 try=2 outcome=hit_not_shiny starter_rng=target_hit nature=- ivs=- pid=- next=next_attempt",
            "RngPlan: result attempt=2 try=2 outcome=shiny starter_rng=shiny_on_target nature=- ivs=- pid=- next=confirm",
        },
    },
    {
        .name = "an attempt whose timings Starter RNG refuses is skipped",
        .outcomes = {REFUSED, shiny_on_target},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::SHINY, .attempts_run = "1,2", .sid = 300,
        .lines = {
            "RngPlan: attempt 1 can't run: Refused timings.",
            "RngPlan: result attempt=1 try=1 outcome=unusable starter_rng=setup_error nature=- ivs=- pid=- next=next_attempt",
        },
    },
    {
        .name = "no attempt Starter RNG will run",
        .outcomes = {REFUSED, REFUSED},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT, .attempts_run = "1,2", .sid = 0,
        .lines = {"RngPlan: result attempt=2 try=1 outcome=unusable starter_rng=setup_error nature=- ivs=- pid=- next=stop"},
    },
    {
        .name = "every attempt hit, none shiny",
        .outcomes = {target_hit, target_hit},
        .minutes_per_attempt = 30, .shiny_required = true,
        .end = StarterRngLoopEnd::OUT_OF_ATTEMPTS, .attempts_run = "1,2", .sid = 0,
        .lines = {"RngPlan: result attempt=2 try=1 outcome=hit_not_shiny starter_rng=target_hit nature=- ivs=- pid=- next=stop"},
    },
    {
        .name = "a hit ends the plan with shiny not required",
        .outcomes = {target_hit},
        .minutes_per_attempt = 30, .shiny_required = false,
        .end = StarterRngLoopEnd::TARGET, .attempts_run = "1", .sid = 0,
        .lines = {},
    },
    {
        .name = "no attempt starts after the total time",
        .outcomes = {out_of_resets, out_of_resets},
        .minutes_per_attempt = 400, .shiny_required = true,
        .end = StarterRngLoopEnd::OUT_OF_TIME, .attempts_run = "1,1", .sid = 0,
        .lines = {
            "RngPlan: state attempt=1/2 try=2/3 reset=1/50 seed=CEFF advances=983 sids=100,200 ruled_out=- "
                "attempt_time=06:40:00/01:00:00 total_time=13:20:00/12:00:00 calibration=1.50/-2.00/0.25",
            "RngPlan: out of time before attempt 1",
        },
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
    for (const std::string& line : row.lines){
        if (!logger.has(line)){
            return "Expected the log line \"" + line + "\"";
        }
    }
    const bool confirmed = std::any_of(logger.lines.begin(), logger.lines.end(), [](const std::string& line){
        return line.starts_with("RngPlan: confirmed");
    });
    if (confirmed != (row.sid != 0)){
        return std::format("Expected {} \"RngPlan: confirmed\" line", row.sid != 0 ? "a" : "no");
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
    const StarterRngLoopLimits limits{
        .attempt_time = std::chrono::minutes(60), .total_time = std::chrono::minutes(430), .max_resets = 50, .max_retries = 5,
    };
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


//  A setup error other than refused timings stops the loop at once: it
//  reaches the program, and the attempt is neither recorded nor skipped.
UnitTestResult test_setup_error_stops(){
    RngPlanTracker tracker(attempts(), CANDIDATES, true, 2);
    SetupErrorSteps steps;
    LineLogger logger;
    const size_t sids = tracker.remaining_sids().size();
    bool thrown = false;
    try{
        run_starter_rng_plan(logger, tracker, steps, LIMITS, 12345, CANDIDATES);
    }catch (UserSetupError&){
        thrown = true;
    }
    const bool result_line = std::any_of(logger.lines.begin(), logger.lines.end(), [](const std::string& line){
        return line.starts_with("RngPlan: result") || line.starts_with("RngPlan: attempt");
    });
    const std::string expected = std::format("thrown 1, calls 1, attempt 1, sids {}, result line 0", sids);
    const std::string got = std::format(
        "thrown {}, calls {}, attempt {}, sids {}, result line {}",
        int(thrown), steps.calls, tracker.attempt_index() + 1, tracker.remaining_sids().size(), int(result_line)
    );
    if (got != expected){
        return "Expected " + expected + "; got " + got;
    }
    return true;
}


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


//  The state line on its own: hours, empty lists and a negative offset.
UnitTestResult test_state_line(){
    const std::string got = rng_log_state(RngLoopState{
        .attempt = 3, .attempts = 5, .try_number = 2, .tries = 3, .resets = 41, .max_resets = 50,
        .seed = 0x70fe, .advances = 1200, .sids = {}, .ruled_out = {7, 8},
        .attempt_time = std::chrono::seconds(3 * 3600 + 61), .attempt_cap = std::chrono::hours(3),
        .total_time = std::chrono::minutes(605), .total_cap = std::chrono::hours(12),
        .calibration = RngCalibrations{-0.5, 12, -3.125},
    });
    const std::string expected =
        "RngPlan: state attempt=3/5 try=2/3 reset=41/50 seed=70FE advances=1200 sids=- ruled_out=7,8 "
        "attempt_time=03:01:01/03:00:00 total_time=10:05:00/12:00:00 calibration=-0.50/12.00/-3.12";
    if (got != expected){
        return "Expected \"" + expected + "\", got \"" + got + "\".";
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
    database.add<Test_AutoStoryRngLoop>("a setup error stops the loop", test_setup_error_stops);
    database.add<Test_AutoStoryRngLoop>("state line", test_state_line);
    database.add<Test_AutoStoryRngLoop>("shiny messages", test_shiny_messages);
    database.add<Test_AutoStoryRngLoop>("rows after a shiny", test_rows_after_shiny);
}



}
}
}

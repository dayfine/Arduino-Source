/*  Starter RNG Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Starter RNG loop's decisions and log lines on scripted attempt
 *  outcomes and a fake clock.
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
#include "PokemonFRLG_StarterRngLoop.h"
#include "PokemonFRLG_StarterRngLoop_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct StarterRngLoopInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct StarterRngLoopExpectation{
    std::string_view description;
};

struct StarterRngLoopTestCase{
    StarterRngLoopInput input;
    StarterRngLoopExpectation expectation;
};

class Test_StarterRngLoop : public UnitTest{
public:
    explicit Test_StarterRngLoop(StarterRngLoopTestCase scenario)
        : UnitTest("PokemonFRLG::StarterRngLoop - " + scenario.input.name)
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
    StarterRngLoopTestCase m_scenario;
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



AdvRngState state(uint16_t seed, uint64_t advance){
    AdvRngState hit{};
    hit.seed = seed;
    hit.advance = advance;
    return hit;
}

//  Target: seed 9cf3 at advance 1670, between 7425 a frame earlier and
//  e66e a frame later.
struct ResetLineRow{
    std::string name;
    std::string program;
    RngResetRead read;
    std::string expected;
};

const std::vector<ResetLineRow> RESET_LINE_ROWS = {
    {
        .name = "seed on the target", .program = "Gift RNG",
        .read = {.resets = 28, .hits = {state(0x9cf3, 1669)}, .hit = state(0x9cf3, 1669), .shiny = false},
        .expected = "Gift RNG try 28/50: seed 9cf3 = target, advance 1669 (target 1670, -1). Missed.",
    },
    {
        .name = "seed a frame late", .program = "Starter RNG",
        .read = {.resets = 27, .hits = {state(0xe66e, 1677), state(0x7425, 1690)}, .hit = state(0xe66e, 1677), .shiny = false},
        .expected = "Starter RNG try 27/50: seed e66e (+1 frame late), advance 1677 (target 1670, +7). Missed.",
    },
    {
        .name = "seed a frame early", .program = "Gift RNG",
        .read = {.resets = 4, .hits = {state(0x7425, 1670)}, .hit = std::nullopt, .shiny = false},
        .expected = "Gift RNG try 4/50: seed 7425 (-1 frame early), advance 1670. Missed.",
    },
    {
        .name = "seed not near the target", .program = "Gift RNG",
        .read = {.resets = 5, .hits = {state(0x1234, 1670)}, .hit = state(0x1234, 1670), .shiny = false},
        .expected = "Gift RNG try 5/50: seed 1234 (not near the target), advance 1670. Missed.",
    },
    {
        .name = "no match", .program = "Gift RNG",
        .read = {.resets = 19, .hits = {}, .hit = std::nullopt, .shiny = false},
        .expected = "Gift RNG try 19/50: no match read (stats fit no seed/advance).",
    },
    {
        .name = "several matches", .program = "Gift RNG",
        .read = {
            .resets = 3, .hits = {state(0x9cf3, 1668), state(0x9cf3, 1668), state(0xe66e, 1671), state(0x7425, 1675)},
            .hit = std::nullopt, .shiny = false,
        },
        .expected = "Gift RNG try 3/50: 3 possible matches, can't tell which.",
    },
    {
        .name = "target hit", .program = "Starter RNG",
        .read = {.resets = 12, .hits = {state(0x9cf3, 1670)}, .hit = state(0x9cf3, 1670), .shiny = false},
        .expected = "Starter RNG try 12/50: seed 9cf3 = target, advance 1670. HIT, not shiny.",
    },
    {
        .name = "shiny on the target", .program = "Gift RNG",
        .read = {.resets = 31, .hits = {state(0xe66e, 1671), state(0x9cf3, 1670)}, .hit = std::nullopt, .shiny = true},
        .expected = "Gift RNG try 31/50: seed 9cf3 = target, advance 1670. HIT, shiny!",
    },
    {
        .name = "shiny off the target", .program = "Gift RNG",
        .read = {.resets = 9, .hits = {state(0xe66e, 1671)}, .hit = std::nullopt, .shiny = true},
        .expected = "Gift RNG try 9/50: shiny, not on the target.",
    },
};

UnitTestResult test_reset_line(const ResetLineRow& row){
    const std::string got = rng_reset_line(RngResetLine{
        .program = row.program, .max_resets = 50, .target_seed = 0x9cf3, .target_advances = 1670,
        .seed_values = {0x1111, 0x7425, 0x9cf3, 0xe66e, 0x2222}, .seed_position = 2,
        .read = row.read,
    });
    if (got != row.expected){
        return "Expected \"" + row.expected + "\", got \"" + got + "\".";
    }
    return true;
}

UnitTestResult test_loop_rows(){
    for (const LoopRow& row : LOOP_ROWS){
        UnitTestResult res = run_loop_row(row);
        if (res.result != UnitTestResult::PASSED){
            return row.name + ": " + res.message;
        }
    }
    return true;
}

UnitTestResult test_reset_line_rows(){
    for (const ResetLineRow& row : RESET_LINE_ROWS){
        UnitTestResult res = test_reset_line(row);
        if (res.result != UnitTestResult::PASSED){
            return row.name + ": " + res.message;
        }
    }
    return true;
}

const StarterRngLoopTestCase STARTER_RNG_LOOP_CASES[] = {
    {
        .input = {.name = "attempt deadlines", .body = test_deadlines},
        .expectation = {.description = "verifies attempt timeout and deadline handling"},
    },
    {
        .input = {.name = "a setup error stops the loop", .body = test_setup_error_stops},
        .expectation = {.description = "aborts loop when setup verification fails"},
    },
    {
        .input = {.name = "state line", .body = test_state_line},
        .expectation = {.description = "formats loop state line output correctly"},
    },
    {
        .input = {.name = "loop row scenarios", .body = test_loop_rows},
        .expectation = {.description = "evaluates simulated starter RNG loop rows"},
    },
    {
        .input = {.name = "reset line scenarios", .body = test_reset_line_rows},
        .expectation = {.description = "evaluates RNG reset summary lines"},
    },
};

} // namespace


void add_tests_StarterRngLoop(UnitTestDatabase& database){
    for (const auto& scenario : STARTER_RNG_LOOP_CASES){
        database.add<Test_StarterRngLoop>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

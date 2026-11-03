/*  AutoStory RNG Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The attempt order, and the step after each attempt's outcome, on made-up
 *  targets; SID Helper's candidates and a Starter RNG plan from the FireRed
 *  seed list.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryRngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

class Test_AutoStoryRngPlan : public UnitTest{
public:
    Test_AutoStoryRngPlan(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonFRLG::AutoStoryRngPlan - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};


std::string join(const std::vector<uint16_t>& values){
    std::string text;
    for (uint16_t value : values){
        text += (text.empty() ? "" : ",") + std::to_string(value);
    }
    return "{" + text + "}";
}


//  SIDs 100, 200 and 300 weigh 3, 2 and 1.
const std::vector<SidCandidate> CANDIDATES = {{100, 3}, {200, 2}, {300, 1}};

//  A target reached "seconds" after the reset, shiny for the candidates at
//  "shiny" (indices into CANDIDATES).
RngTarget make_target(uint32_t state, uint64_t seconds, std::vector<size_t> shiny){
    RngTarget target{};
    target.state = state;
    target.seed = {uint16_t(state), seconds * 1000, SeedButton::A, BlackoutButton::None};
    target.advances = 0;
    target.pokemon = method1_pokemon(state);
    for (size_t candidate : shiny){
        target.shiny_for.push_back({candidate, AdvShinyType::Star});
    }
    return target;
}

//  By expected wait per shiny over all three (weight 6): 1 = 50 s / (5/6) = 60,
//  2 = 20 s / (1/6) = 120, 3 = 40 s / (3/6) = 80. Then over SID 300 alone, 2 is
//  the only one shiny for it. 4 is the quickest and shiny for none.
const std::vector<RngTarget> TARGETS = {
    make_target(1, 50, {0, 1}),
    make_target(2, 20, {2}),
    make_target(3, 40, {0}),
    make_target(4, 5, {}),
};


UnitTestResult test_sid_helper_candidates(Logger&){
    const uint16_t tid = 43771;
    const uint32_t target_advances = 3001;
    const std::vector<SidCandidate> candidates = sid_helper_candidates(tid, target_advances, 5);

    //  The SID is the top half of the state after the TID seed is advanced
    //  once, then "advances" more times.
    auto sid_at = [&](uint32_t advances){
        uint32_t state = tid;
        for (uint32_t c = 0; c <= advances; c++){
            state = lcrng_next(state);
        }
        return uint16_t(state >> 16);
    };
    const std::vector<uint32_t> advances = {2997, 2999, 3001, 3003, 3005};
    const std::vector<double> weights = {1, 2, 3, 2, 1};
    if (candidates.size() != advances.size()){
        return std::format("Expected {} candidates, got {}.", advances.size(), candidates.size());
    }
    for (size_t c = 0; c < candidates.size(); c++){
        if (candidates[c].sid != sid_at(advances[c]) || candidates[c].weight != weights[c]){
            return std::format(
                "Candidate {}: expected SID {} weight {}, got SID {} weight {}.",
                c, sid_at(advances[c]), weights[c], candidates[c].sid, candidates[c].weight
            );
        }
    }

    const std::vector<SidCandidate> even = sid_helper_candidates(tid, target_advances, 4);
    const std::vector<double> even_weights = {1, 2, 3, 2};
    for (size_t c = 0; c < even.size() && c < even_weights.size(); c++){
        if (even.size() != even_weights.size() || even[c].sid != candidates[c].sid || even[c].weight != even_weights[c]){
            return std::format("Four candidates: expected the first four of five, weighing 1, 2, 3, 2; candidate {} differs.", c);
        }
    }
    return true;
}


UnitTestResult test_parse_trainer_id(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "64694", .expected = "64694"},
        {.text = " 0 ", .expected = "0"},
        {.text = "65535", .expected = "65535"},
        {.text = "65536", .expected = "none"},
        {.text = "", .expected = "none"},
        {.text = "-1", .expected = "none"},
        {.text = "12a", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<uint16_t> sid = parse_trainer_id(item.text);
        const std::string got = sid ? std::to_string(*sid) : "none";
        if (got != item.expected){
            return std::format("SID \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}

UnitTestResult test_parse_ivs(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "31/14/7/29/1/27", .expected = "31/14/7/29/1/27"},
        {.text = " 0 / 0/0/0/0/31 ", .expected = "0/0/0/0/0/31"},
        {.text = "31/0/0", .expected = "none"},
        {.text = "32/0/0/0/0/0", .expected = "none"},
        {.text = "a/0/0/0/0/0", .expected = "none"},
        {.text = "-1/0/0/0/0/0", .expected = "none"},
        {.text = "", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<AdvIVs> ivs = parse_ivs(item.text);
        const std::string got = ivs ? ivs_to_string(*ivs) : "none";
        if (got != item.expected){
            return std::format("IVs \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}

UnitTestResult test_parse_natures(Logger&){
    struct Case{
        std::string text;
        std::string expected;
    };
    const Case cases[] = {
        {.text = "", .expected = "any"},
        {.text = "  ", .expected = "any"},
        {.text = "Modest", .expected = "Modest"},
        {.text = "Modest, Timid", .expected = "Modest Timid"},
        {.text = "Modset", .expected = "none"},
        {.text = "Modest,", .expected = "none"},
    };
    for (const Case& item : cases){
        const std::optional<std::vector<AdvNature>> natures = parse_natures(item.text);
        std::string got;
        if (!natures){
            got = "none";
        }else if (natures->empty()){
            got = "any";
        }else{
            for (AdvNature nature : *natures){
                got += (got.empty() ? "" : " ") + nature_to_string(nature);
            }
        }
        if (got != item.expected){
            return std::format("Natures \"{}\": expected {}, got {}.", item.text, item.expected, got);
        }
    }
    return true;
}


struct PlanCase{
    std::string name;
    std::vector<RngTarget> targets;
    bool shiny_required;
    size_t max_attempts;
    std::vector<uint32_t> states;
    std::vector<std::vector<uint16_t>> sids;
};
std::vector<PlanCase> plan_cases(){
    const std::vector<RngTarget> no_300 = {TARGETS[0], TARGETS[2], TARGETS[3]};
    return {
        {
            .name = "two candidates in one target first, then the last",
            .targets = TARGETS, .shiny_required = true, .max_attempts = 5,
            .states = {1, 2}, .sids = {{100, 200}, {300}},
        },
        {
            .name = "capped at one attempt",
            .targets = TARGETS, .shiny_required = true, .max_attempts = 1,
            .states = {1}, .sids = {{100, 200}},
        },
        {
            .name = "no target shiny for SID 300",
            .targets = no_300, .shiny_required = true, .max_attempts = 5,
            .states = {1}, .sids = {{100, 200}},
        },
        {
            .name = "shiny not required: the quickest",
            .targets = TARGETS, .shiny_required = false, .max_attempts = 5,
            .states = {4}, .sids = {{}},
        },
        {
            .name = "no targets",
            .targets = {}, .shiny_required = true, .max_attempts = 5,
            .states = {}, .sids = {},
        },
    };
}

UnitTestResult test_plan_order(Logger&){
    for (const PlanCase& item : plan_cases()){
        const std::vector<RngAttempt> attempts = plan_rng_attempts(item.targets, CANDIDATES, item.shiny_required, item.max_attempts);
        std::vector<uint32_t> states;
        std::vector<std::vector<uint16_t>> sids;
        for (const RngAttempt& attempt : attempts){
            states.push_back(attempt.target.state);
            sids.push_back(attempt.sids);
        }
        if (states != item.states || sids != item.sids){
            std::string got;
            for (size_t c = 0; c < attempts.size(); c++){
                got += std::format(" target {} {}", states[c], join(sids[c]));
            }
            return item.name + ": got" + (got.empty() ? " no attempts" : got) + ".";
        }
    }
    return true;
}


UnitTestResult test_classify(Logger&){
    const RngTarget hit = TARGETS[0];
    AdvIVs other_ivs = hit.pokemon.ivs;
    other_ivs.speed = uint8_t((other_ivs.speed + 1) % 32);
    const AdvNature other_nature = hit.pokemon.nature == AdvNature::Hardy ? AdvNature::Lonely : AdvNature::Hardy;

    struct Case{
        std::string name;
        RngAttemptResult result;
        RngAttemptOutcome expected;
    };
    const Case cases[] = {
        {.name = "shiny, the target", .result = {.shiny = true, .nature = hit.pokemon.nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::SHINY},
        {.name = "shiny, not the target", .result = {.shiny = true, .nature = other_nature, .ivs = other_ivs}, .expected = RngAttemptOutcome::SHINY_OFF_TARGET},
        {.name = "same nature and IVs", .result = {.shiny = false, .nature = hit.pokemon.nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::HIT_NOT_SHINY},
        {.name = "other IVs", .result = {.shiny = false, .nature = hit.pokemon.nature, .ivs = other_ivs}, .expected = RngAttemptOutcome::MISSED},
        {.name = "other nature", .result = {.shiny = false, .nature = other_nature, .ivs = hit.pokemon.ivs}, .expected = RngAttemptOutcome::MISSED},
    };
    for (const Case& item : cases){
        if (classify_attempt(hit, item.result) != item.expected){
            return item.name + ": classified wrong.";
        }
    }
    return true;
}


const char* step_name(RngPlanStep step){
    switch (step){
    case RngPlanStep::RUN_ATTEMPT:     return "RUN_ATTEMPT";
    case RngPlanStep::GOT_SHINY:       return "GOT_SHINY";
    case RngPlanStep::GOT_SHINY_OFF_TARGET: return "GOT_SHINY_OFF_TARGET";
    case RngPlanStep::GOT_TARGET:      return "GOT_TARGET";
    case RngPlanStep::OUT_OF_ATTEMPTS: return "OUT_OF_ATTEMPTS";
    case RngPlanStep::MISSED_EVERY_ATTEMPT_LEFT: return "MISSED_EVERY_ATTEMPT_LEFT";
    }
    return "?";
}

struct TrackerCase{
    std::string name;
    bool shiny_required;
    std::vector<RngAttemptOutcome> outcomes;
    //  The step after each outcome, and the attempt run next.
    std::vector<RngPlanStep> steps;
    std::vector<size_t> attempts;
    std::vector<uint16_t> remaining;
    std::optional<uint16_t> confirmed;
};
std::vector<TrackerCase> tracker_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "shiny at once: one of two SIDs",
            .shiny_required = true, .outcomes = {SHINY},
            .steps = {GOT_SHINY}, .attempts = {0},
            .remaining = {100, 200}, .confirmed = std::nullopt,
        },
        {
            .name = "hit, then shiny: SID confirmed",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, SHINY},
            .steps = {RUN_ATTEMPT, GOT_SHINY}, .attempts = {1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "a miss retries the same target",
            .shiny_required = true, .outcomes = {MISSED, MISSED, HIT_NOT_SHINY, MISSED},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT}, .attempts = {0, 0, 1, 1},
            .remaining = {300}, .confirmed = std::nullopt,
        },
    };
}
//  Misses: retried, then the next attempt, mixed with hits and shinies.
std::vector<TrackerCase> tracker_retry_cases(){
    using enum RngAttemptOutcome;
    using enum RngPlanStep;
    return {
        {
            .name = "retries used up, then the next attempt",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, GOT_SHINY}, .attempts = {0, 0, 1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "every attempt left missed too often",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, MISSED, MISSED, MISSED},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, MISSED_EVERY_ATTEMPT_LEFT},
            .attempts = {0, 0, 1, 1, 1, 1},
            .remaining = {100, 200, 300}, .confirmed = std::nullopt,
        },
        {
            .name = "shiny off the target confirms nothing",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, SHINY_OFF_TARGET, SHINY},
            .steps = {RUN_ATTEMPT, GOT_SHINY_OFF_TARGET, GOT_SHINY_OFF_TARGET}, .attempts = {1, 1, 1},
            .remaining = {300}, .confirmed = std::nullopt,
        },
        {
            .name = "miss, hit, misses, then shiny",
            .shiny_required = true, .outcomes = {MISSED, HIT_NOT_SHINY, MISSED, MISSED, SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, GOT_SHINY}, .attempts = {0, 1, 1, 1, 1},
            .remaining = {300}, .confirmed = uint16_t(300),
        },
        {
            .name = "misses move on, then a hit ends the plan",
            .shiny_required = true, .outcomes = {MISSED, MISSED, MISSED, MISSED, HIT_NOT_SHINY},
            .steps = {RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, RUN_ATTEMPT, OUT_OF_ATTEMPTS}, .attempts = {0, 0, 1, 1, 1},
            .remaining = {100, 200}, .confirmed = std::nullopt,
        },
        {
            .name = "every attempt hit, none shiny",
            .shiny_required = true, .outcomes = {HIT_NOT_SHINY, HIT_NOT_SHINY},
            .steps = {RUN_ATTEMPT, OUT_OF_ATTEMPTS}, .attempts = {1, 1},
            .remaining = {}, .confirmed = std::nullopt,
        },
        {
            .name = "shiny not required: a hit ends it",
            .shiny_required = false, .outcomes = {HIT_NOT_SHINY},
            .steps = {GOT_TARGET}, .attempts = {0},
            .remaining = {100, 200, 300}, .confirmed = std::nullopt,
        },
    };
}

UnitTestResult check_tracker_case(const TrackerCase& item){
    RngPlanTracker tracker(plan_rng_attempts(TARGETS, CANDIDATES, true, 5), CANDIDATES, item.shiny_required, 2);
    if (tracker.step() != RngPlanStep::RUN_ATTEMPT){
        return item.name + ": expected to start with RUN_ATTEMPT, got " + step_name(tracker.step()) + ".";
    }
    for (size_t c = 0; c < item.outcomes.size(); c++){
        const RngPlanStep step = tracker.record(item.outcomes[c]);
        if (step != item.steps[c] || tracker.attempt_index() != item.attempts[c]){
            return std::format(
                "{}: after outcome {}, expected {} at attempt {}, got {} at attempt {}.",
                item.name, c, step_name(item.steps[c]), item.attempts[c], step_name(step), tracker.attempt_index()
            );
        }
    }
    if (tracker.remaining_sids() != item.remaining || tracker.confirmed_sid() != item.confirmed){
        return std::format(
            "{}: expected SIDs {} (confirmed {}), got {} (confirmed {}).",
            item.name, join(item.remaining), item.confirmed ? std::to_string(*item.confirmed) : "none",
            join(tracker.remaining_sids()), tracker.confirmed_sid() ? std::to_string(*tracker.confirmed_sid()) : "none"
        );
    }
    return true;
}

UnitTestResult test_tracker(Logger&){
    std::vector<TrackerCase> cases = tracker_cases();
    for (TrackerCase& item : tracker_retry_cases()){
        cases.push_back(std::move(item));
    }
    for (const TrackerCase& item : cases){
        const UnitTestResult result = check_tracker_case(item);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    RngPlanTracker empty({}, CANDIDATES, true, 2);
    if (empty.step() != RngPlanStep::OUT_OF_ATTEMPTS){
        return std::string("An empty plan: expected OUT_OF_ATTEMPTS, got ") + step_name(empty.step()) + ".";
    }
    //  A shiny hit for a SID that is no candidate: none left, none confirmed.
    RngPlanTracker outside({RngAttempt{make_target(1, 50, {}), {999}}}, CANDIDATES, true, 2);
    outside.record(RngAttemptOutcome::SHINY);
    if (outside.step() != RngPlanStep::GOT_SHINY || !outside.remaining_sids().empty() || outside.confirmed_sid()){
        return std::format(
            "Shiny for no candidate: expected GOT_SHINY with no SID left, got {} with SIDs {}.",
            step_name(outside.step()), join(outside.remaining_sids())
        );
    }
    return true;
}


//  The "RngPlan:" lines the candidate odds are rebuilt from.
UnitTestResult test_log_lines(Logger&){
    RngAttempt attempt{make_target(1, 50, {0, 1}), {100, 200}};
    attempt.target.seed.seed = 0x2ef6;
    attempt.target.advances = 29364;
    struct Case{
        std::string got;
        std::string expected;
    };
    const Case cases[] = {
        {.got = rng_log_tid(43771, true), .expected = "RngPlan: tid=43771 source=card"},
        {.got = rng_log_tid(43771, false), .expected = "RngPlan: tid=43771 source=option"},
        {.got = rng_log_sid_helper(43771, 3001, CANDIDATES), .expected = "RngPlan: sid_helper tid=43771 target_advances=3001 candidates=100,200,300"},
        {.got = rng_log_attempt(0, attempt), .expected = "RngPlan: attempt=1 seed=2EF6 advances=29364 sids=100,200"},
        {
            .got = rng_log_result(RngResultLog{
                .index = 1, .try_number = 2, .outcome = RngAttemptOutcome::HIT_NOT_SHINY, .detail = "target_hit",
                .pokemon = attempt.target.pokemon, .next = RngPlanNext::NEXT_ATTEMPT,
            }),
            .expected = std::format(
                "RngPlan: result attempt=2 try=2 outcome=hit_not_shiny starter_rng=target_hit nature={} ivs={} pid={:08X} next=next_attempt",
                nature_to_string(attempt.target.pokemon.nature), ivs_to_string(attempt.target.pokemon.ivs), attempt.target.pokemon.pid
            ),
        },
        {
            .got = rng_log_result(RngResultLog{
                .index = 0, .try_number = 1, .outcome = RngAttemptOutcome::MISSED, .detail = "out_of_resets",
                .pokemon = std::nullopt, .next = RngPlanNext::RETRY,
            }),
            .expected = "RngPlan: result attempt=1 try=1 outcome=missed starter_rng=out_of_resets nature=- ivs=- pid=- next=retry",
        },
        {
            .got = rng_log_result(RngResultLog{
                .index = 2, .try_number = 3, .outcome = RngAttemptOutcome::SHINY_OFF_TARGET, .detail = "shiny_off_target",
                .pokemon = std::nullopt, .next = RngPlanNext::STOP,
            }),
            .expected = "RngPlan: result attempt=3 try=3 outcome=shiny_off_target starter_rng=shiny_off_target nature=- ivs=- pid=- next=stop",
        },
        {.got = rng_log_confirmed(43771, 300, CANDIDATES), .expected = "RngPlan: confirmed tid=43771 sid=300 candidate=3 of=3"},
        {.got = rng_log_confirmed(43771, 7, CANDIDATES), .expected = "RngPlan: confirmed tid=43771 sid=7 candidate=0 of=3"},
    };
    for (const Case& item : cases){
        if (item.got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + item.got + "\".";
        }
    }
    return true;
}


//  A plan from the FireRed (English) list: every attempt starts from a seed
//  Starter RNG finds, takes at least its smallest Advances, and is shiny for
//  the SIDs it lists, which no earlier attempt listed.
UnitTestResult test_starter_plan(Logger&){
    const uint16_t tid = 43771;
    StarterRngPlanInputs inputs{
        .firered = true,
        .language = Language::English,
        .sound = SoundSetting::Mono,
        .tid = tid,
        .candidates = sid_helper_candidates(tid, 3001, 5),
        .filters = {},
        .max_attempts = 5,
    };
    const std::vector<RngAttempt> attempts = plan_starter_rng(inputs);
    if (attempts.empty()){
        return "Expected attempts for TID 43771 with five SID candidates, got none.";
    }

    const SeedsDatabase database(seeds_json_path(true, Language::English));
    std::vector<uint16_t> listed;
    for (size_t c = 0; c < attempts.size(); c++){
        const RngAttempt& attempt = attempts[c];
        if (!database.find_seed(attempt.target.seed.seed, SoundSetting::Mono, 5).found){
            return std::format("Attempt {}: Starter RNG can't find Target Seed {:04X}.", c + 1, attempt.target.seed.seed);
        }
        if (attempt.target.advances < STARTER_RNG_MIN_ADVANCES){
            return std::format("Attempt {}: Advances {} below Starter RNG's {}.", c + 1, attempt.target.advances, STARTER_RNG_MIN_ADVANCES);
        }
        if (attempt.sids.empty()){
            return std::format("Attempt {}: shiny for no SID candidate.", c + 1);
        }
        for (uint16_t sid : attempt.sids){
            if (shiny_type(attempt.target.pokemon.pid, tid, sid) == AdvShinyType::Normal){
                return std::format("Attempt {}: PID {:08X} not shiny for SID {}.", c + 1, attempt.target.pokemon.pid, sid);
            }
            if (std::find(listed.begin(), listed.end(), sid) != listed.end()){
                return std::format("Attempt {}: SID {} already listed by an earlier attempt.", c + 1, sid);
            }
            listed.push_back(sid);
        }
    }
    if (rng_attempt_lines(attempts).size() != attempts.size()){
        return "Expected one line per attempt.";
    }
    return true;
}


}


void add_tests_AutoStoryRngPlan(UnitTestDatabase& database){
    database.add<Test_AutoStoryRngPlan>("SID Helper's candidates and weights", test_sid_helper_candidates);
    database.add<Test_AutoStoryRngPlan>("TID and SID text", test_parse_trainer_id);
    database.add<Test_AutoStoryRngPlan>("IVs text", test_parse_ivs);
    database.add<Test_AutoStoryRngPlan>("natures text", test_parse_natures);
    database.add<Test_AutoStoryRngPlan>("attempt order", test_plan_order);
    database.add<Test_AutoStoryRngPlan>("attempt outcome", test_classify);
    database.add<Test_AutoStoryRngPlan>("step after each outcome", test_tracker);
    database.add<Test_AutoStoryRngPlan>("Starter RNG plan from the FireRed list", test_starter_plan);
    database.add<Test_AutoStoryRngPlan>("RngPlan log lines", test_log_lines);
}



}
}
}

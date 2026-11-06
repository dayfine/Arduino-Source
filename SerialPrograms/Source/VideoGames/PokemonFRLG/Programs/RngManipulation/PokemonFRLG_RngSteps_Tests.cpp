/*  RNG Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  SID Helper's intro timing per language, and Starter RNG's per-starter
 *  tables, as SID Helper and Starter RNG computed them before the steps
 *  moved out of the programs; and whether a shiny was the target.
 *
 */

#include <format>
#include <optional>
#include <utility>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "PokemonFRLG_SidSteps.h"
#include "PokemonFRLG_StarterRngAttempts.h"
#include "PokemonFRLG_RngSteps_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

struct SidTimingInput{
    Language language;
    uint32_t target_advances;
};

struct SidTimingExpectation{
    uint64_t delay_ms;
    bool extra_press_at_end;
};

struct SidTimingRow{
    SidTimingInput input;
    SidTimingExpectation expectation;
};

//  (target_advances - 2 * final text frames + 7) frames of 1000 / 59.999977 ms,
//  halved since advances pass 2 by 2.
const std::vector<SidTimingRow> SID_TIMING_ROWS = {
    {.input = {.language = Language::English,  .target_advances = 3001}, .expectation = {.delay_ms = 20916, .extra_press_at_end = false}},
    {.input = {.language = Language::English,  .target_advances = 2275}, .expectation = {.delay_ms = 14866, .extra_press_at_end = false}},
    {.input = {.language = Language::Japanese, .target_advances = 3001}, .expectation = {.delay_ms = 21833, .extra_press_at_end = true}},
    {.input = {.language = Language::Italian,  .target_advances = 3001}, .expectation = {.delay_ms = 21133, .extra_press_at_end = false}},
    {.input = {.language = Language::French,   .target_advances = 3001}, .expectation = {.delay_ms = 21650, .extra_press_at_end = true}},
    {.input = {.language = Language::German,   .target_advances = 3001}, .expectation = {.delay_ms = 21600, .extra_press_at_end = true}},
    {.input = {.language = Language::Spanish,  .target_advances = 2275}, .expectation = {.delay_ms = 15650, .extra_press_at_end = true}},
};

class Test_SidTiming : public UnitTest{
public:
    explicit Test_SidTiming(const SidTimingRow& row)
        : UnitTest(std::format(
            "PokemonFRLG::RngSteps - SID timing {} {}",
            language_data(row.input.language).name, row.input.target_advances
        ))
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        SidTiming timing = sid_timing(m_row.input.language, m_row.input.target_advances);
        if (timing.delay_ms != m_row.expectation.delay_ms || timing.extra_press_at_end != m_row.expectation.extra_press_at_end){
            return std::format(
                "Expected delay {} ms, extra press {}; got {} ms, extra press {}",
                m_row.expectation.delay_ms, m_row.expectation.extra_press_at_end,
                timing.delay_ms, timing.extra_press_at_end
            );
        }
        return true;
    }

private:
    SidTimingRow m_row;
};


struct StarterInput{
    StarterSpecies starter;
    std::string name;
};

struct StarterExpectation{
    BaseStats base_stats;
    EVs rival_ev_yield;
    int steps_left;
};

struct StarterRow{
    StarterInput input;
    StarterExpectation expectation;
};

const std::vector<StarterRow> STARTER_ROWS = {
    {
        .input = {.starter = StarterSpecies::bulbasaur, .name = "Bulbasaur"},
        .expectation = {
            .base_stats = {45, 49, 49, 65, 65, 45},
            .rival_ev_yield = {0, 0, 0, 0, 0, 1},
            .steps_left = 2,
        },
    },
    {
        .input = {.starter = StarterSpecies::squirtle, .name = "Squirtle"},
        .expectation = {
            .base_stats = {44, 48, 65, 50, 64, 43},
            .rival_ev_yield = {0, 0, 0, 1, 0, 0},
            .steps_left = 3,
        },
    },
    {
        .input = {.starter = StarterSpecies::charmander, .name = "Charmander"},
        .expectation = {
            .base_stats = {39, 52, 43, 60, 50, 65},
            .rival_ev_yield = {0, 0, 1, 0, 0, 0},
            .steps_left = 4,
        },
    },
};

std::string to_string(const BaseStats& stats){
    return std::format("{}/{}/{}/{}/{}/{}", stats.hp, stats.attack, stats.defense, stats.spatk, stats.spdef, stats.speed);
}
std::string to_string(const EVs& evs){
    return std::format("{}/{}/{}/{}/{}/{}", evs.hp, evs.attack, evs.defense, evs.spatk, evs.spdef, evs.speed);
}

class Test_StarterTables : public UnitTest{
public:
    explicit Test_StarterTables(const StarterRow& row)
        : UnitTest("PokemonFRLG::RngSteps - Starter tables " + row.input.name)
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::string expected = std::format(
            "base stats {}, rival EVs {}, steps left {}",
            to_string(m_row.expectation.base_stats), to_string(m_row.expectation.rival_ev_yield), m_row.expectation.steps_left
        );
        std::string got = std::format(
            "base stats {}, rival EVs {}, steps left {}",
            to_string(starter_base_stats(m_row.input.starter)),
            to_string(rival_ev_yield(m_row.input.starter)),
            steps_left_to_lab_door(m_row.input.starter)
        );
        if (expected != got){
            return "Expected " + expected + "; got " + got;
        }
        return true;
    }

private:
    StarterRow m_row;
};



//  A state's (seed, advance).
using SeedAdvance = std::pair<uint16_t, uint64_t>;

//  Target: seed 0x70FE, advance 1000.
struct ShinyInput{
    std::string name;
    std::vector<SeedAdvance> hits;
};

struct ShinyExpectation{
    StarterRngOutcome expected;
};

struct ShinyRow{
    ShinyInput input;
    ShinyExpectation expectation;
};

const std::vector<ShinyRow> SHINY_ROWS = {
    {.input = {.name = "the target alone", .hits = {{0x70fe, 1000}}}, .expectation = {.expected = StarterRngOutcome::shiny_on_target}},
    {.input = {.name = "the target among others", .hits = {{0x70fe, 998}, {0x70fe, 1000}, {0x70ff, 1000}}}, .expectation = {.expected = StarterRngOutcome::shiny_on_target}},
    {.input = {.name = "another advance", .hits = {{0x70fe, 1002}}}, .expectation = {.expected = StarterRngOutcome::shiny_off_target}},
    {.input = {.name = "another seed", .hits = {{0x70fd, 1000}}}, .expectation = {.expected = StarterRngOutcome::shiny_off_target}},
    {.input = {.name = "no match", .hits = {}}, .expectation = {.expected = StarterRngOutcome::shiny}},
};

const char* outcome_name(StarterRngOutcome outcome){
    switch (outcome){
    case StarterRngOutcome::shiny:              return "shiny";
    case StarterRngOutcome::shiny_on_target:    return "shiny_on_target";
    case StarterRngOutcome::shiny_off_target:   return "shiny_off_target";
    case StarterRngOutcome::target_hit:         return "target_hit";
    case StarterRngOutcome::wild_shiny:         return "wild_shiny";
    case StarterRngOutcome::out_of_resets:      return "out_of_resets";
    case StarterRngOutcome::no_matches:         return "no_matches";
    }
    return "?";
}

class Test_IdentifyShiny : public UnitTest{
public:
    explicit Test_IdentifyShiny(const ShinyRow& row)
        : UnitTest("PokemonFRLG::RngSteps - Shiny " + row.input.name)
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<AdvRngState> hits;
        for (const auto& [seed, advance] : m_row.input.hits){
            AdvRngState hit{};
            hit.seed = seed;
            hit.advance = advance;
            hits.push_back(hit);
        }
        const StarterRngOutcome got = identify_shiny(0x70fe, 1000, hits);
        if (got != m_row.expectation.expected){
            return std::string("Expected ") + outcome_name(m_row.expectation.expected) + ", got " + outcome_name(got);
        }
        return true;
    }

private:
    ShinyRow m_row;
};


AdvRngState state(uint16_t seed, uint64_t advance){
    AdvRngState hit{};
    hit.seed = seed;
    hit.advance = advance;
    return hit;
}


std::string state_text(const std::optional<SeedAdvance>& state){
    return state ? std::format("{:04x}/{}", state->first, state->second) : "none";
}

struct ResetHitInput{
    std::string name;
    //  The reset's hits, and what update_history() left after it.
    std::vector<AdvRngState> hits;
    size_t uncertain = 0;
    std::vector<AdvRngState> calibrated;
};

struct ResetHitExpectation{
    //  The (seed, advance) taken as hit, if any.
    std::optional<SeedAdvance> expected;
};

struct ResetHitRow{
    ResetHitInput input;
    ResetHitExpectation expectation;
};

const std::vector<ResetHitRow> RESET_HIT_ROWS = {
    {
        .input = {.name = "one hit", .hits = {state(0x70fe, 1001)}, .uncertain = 0, .calibrated = {state(0x70fe, 1001)}},
        .expectation = {.expected = SeedAdvance{0x70fe, 1001}},
    },
    {
        .input = {.name = "settled by the history", .hits = {state(0x70fe, 998), state(0x70ff, 1003)}, .uncertain = 0, .calibrated = {state(0x70fd, 1004), state(0x70ff, 1003)}},
        .expectation = {.expected = SeedAdvance{0x70ff, 1003}},
    },
    {
        .input = {.name = "a tie", .hits = {state(0x70fe, 998), state(0x70ff, 1003)}, .uncertain = 2, .calibrated = {state(0x70fd, 1004)}},
        .expectation = {.expected = std::nullopt},
    },
    {
        .input = {.name = "no hits", .hits = {}, .uncertain = 0, .calibrated = {state(0x70fd, 1004)}},
        .expectation = {.expected = std::nullopt},
    },
};

class Test_ResetHit : public UnitTest{
public:
    explicit Test_ResetHit(const ResetHitRow& row)
        : UnitTest("PokemonFRLG::RngSteps - Reset hit " + row.input.name)
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        RngUncertainHistory uncertain;
        uncertain.results.resize(m_row.input.uncertain);
        RngCalibrationHistory calibration;
        calibration.results = m_row.input.calibrated;
        const std::optional<AdvRngState> hit = reset_hit(uncertain, calibration, m_row.input.hits);
        std::optional<SeedAdvance> got;
        if (hit){
            got = SeedAdvance{hit->seed, hit->advance};
        }
        if (got != m_row.expectation.expected){
            return "Expected " + state_text(m_row.expectation.expected) + ", got " + state_text(got);
        }
        return true;
    }

private:
    ResetHitRow m_row;
};

} // namespace


void add_tests_RngSteps(UnitTestDatabase& database){
    for (const SidTimingRow& row : SID_TIMING_ROWS){
        database.add<Test_SidTiming>(row);
    }
    for (const StarterRow& row : STARTER_ROWS){
        database.add<Test_StarterTables>(row);
    }
    for (const ShinyRow& row : SHINY_ROWS){
        database.add<Test_IdentifyShiny>(row);
    }
    for (const ResetHitRow& row : RESET_HIT_ROWS){
        database.add<Test_ResetHit>(row);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

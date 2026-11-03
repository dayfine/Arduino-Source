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

struct SidTimingRow{
    Language language;
    uint32_t target_advances;
    uint64_t delay_ms;
    bool extra_press_at_end;
};

//  (target_advances - 2 * final text frames + 7) frames of 1000 / 59.999977 ms,
//  halved since advances pass 2 by 2.
const std::vector<SidTimingRow> SID_TIMING_ROWS = {
    {.language = Language::English,  .target_advances = 3001, .delay_ms = 20916, .extra_press_at_end = false},
    {.language = Language::English,  .target_advances = 2275, .delay_ms = 14866, .extra_press_at_end = false},
    {.language = Language::Japanese, .target_advances = 3001, .delay_ms = 21833, .extra_press_at_end = true},
    {.language = Language::Italian,  .target_advances = 3001, .delay_ms = 21133, .extra_press_at_end = false},
    {.language = Language::French,   .target_advances = 3001, .delay_ms = 21650, .extra_press_at_end = true},
    {.language = Language::German,   .target_advances = 3001, .delay_ms = 21600, .extra_press_at_end = true},
    {.language = Language::Spanish,  .target_advances = 2275, .delay_ms = 15650, .extra_press_at_end = true},
};

class Test_SidTiming : public UnitTest{
public:
    explicit Test_SidTiming(const SidTimingRow& row)
        : UnitTest(std::format(
            "PokemonFRLG::RngSteps - SID timing {} {}",
            language_data(row.language).name, row.target_advances
        ))
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        SidTiming timing = sid_timing(m_row.language, m_row.target_advances);
        if (timing.delay_ms != m_row.delay_ms || timing.extra_press_at_end != m_row.extra_press_at_end){
            return std::format(
                "Expected delay {} ms, extra press {}; got {} ms, extra press {}",
                m_row.delay_ms, m_row.extra_press_at_end,
                timing.delay_ms, timing.extra_press_at_end
            );
        }
        return true;
    }

private:
    SidTimingRow m_row;
};


struct StarterRow{
    StarterSpecies starter;
    std::string name;
    BaseStats base_stats;
    EVs rival_ev_yield;
    int steps_left;
};

const std::vector<StarterRow> STARTER_ROWS = {
    {
        .starter = StarterSpecies::bulbasaur,
        .name = "Bulbasaur",
        .base_stats = {45, 49, 49, 65, 65, 45},
        .rival_ev_yield = {0, 0, 0, 0, 0, 1},
        .steps_left = 2,
    },
    {
        .starter = StarterSpecies::squirtle,
        .name = "Squirtle",
        .base_stats = {44, 48, 65, 50, 64, 43},
        .rival_ev_yield = {0, 0, 0, 1, 0, 0},
        .steps_left = 3,
    },
    {
        .starter = StarterSpecies::charmander,
        .name = "Charmander",
        .base_stats = {39, 52, 43, 60, 50, 65},
        .rival_ev_yield = {0, 0, 1, 0, 0, 0},
        .steps_left = 4,
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
        : UnitTest("PokemonFRLG::RngSteps - Starter tables " + row.name)
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::string expected = std::format(
            "base stats {}, rival EVs {}, steps left {}",
            to_string(m_row.base_stats), to_string(m_row.rival_ev_yield), m_row.steps_left
        );
        std::string got = std::format(
            "base stats {}, rival EVs {}, steps left {}",
            to_string(starter_base_stats(m_row.starter)),
            to_string(rival_ev_yield(m_row.starter)),
            steps_left_to_lab_door(m_row.starter)
        );
        if (expected != got){
            return "Expected " + expected + "; got " + got;
        }
        return true;
    }

private:
    StarterRow m_row;
};



//  Target: seed 0x70FE, advance 1000.
struct ShinyRow{
    std::string name;
    //  The (seed, advance) of each state the shiny's stats match.
    std::vector<std::pair<uint16_t, uint64_t>> hits;
    StarterRngOutcome expected;
};

const std::vector<ShinyRow> SHINY_ROWS = {
    {.name = "the target alone", .hits = {{0x70fe, 1000}}, .expected = StarterRngOutcome::shiny_on_target},
    {.name = "the target among others", .hits = {{0x70fe, 998}, {0x70fe, 1000}, {0x70ff, 1000}}, .expected = StarterRngOutcome::shiny_on_target},
    {.name = "another advance", .hits = {{0x70fe, 1002}}, .expected = StarterRngOutcome::shiny_off_target},
    {.name = "another seed", .hits = {{0x70fd, 1000}}, .expected = StarterRngOutcome::shiny_off_target},
    {.name = "no match", .hits = {}, .expected = StarterRngOutcome::shiny},
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
        : UnitTest("PokemonFRLG::RngSteps - Shiny " + row.name)
        , m_row(row)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<AdvRngState> hits;
        for (const auto& [seed, advance] : m_row.hits){
            AdvRngState hit{};
            hit.seed = seed;
            hit.advance = advance;
            hits.push_back(hit);
        }
        const StarterRngOutcome got = identify_shiny(0x70fe, 1000, hits);
        if (got != m_row.expected){
            return std::string("Expected ") + outcome_name(m_row.expected) + ", got " + outcome_name(got);
        }
        return true;
    }

private:
    ShinyRow m_row;
};

}


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
}


}
}
}

/*  Gift RNG Attempts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Gift RNG's per-target base stats, gender thresholds and levels, as Gift
 *  RNG set them before its attempts moved out of the program; and whether
 *  a shiny gift was the target.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "PokemonFRLG_GiftRngAttempts.h"
#include "PokemonFRLG_GiftRngAttempts_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

struct GiftSpeciesInput{
    PokemonFRLG_RngTarget target = PokemonFRLG_RngTarget::magikarp;
    std::string name = "";
};

struct GiftSpeciesExpectation{
    BaseStats base_stats{};
    int16_t gender_threshold = -1;
    uint8_t level = 0;
};

struct GiftSpeciesCase{
    GiftSpeciesInput input;
    GiftSpeciesExpectation expectation;
};

const GiftSpeciesCase GIFT_SPECIES_CASES[] = {
    {
        .input = {.target = PokemonFRLG_RngTarget::magikarp, .name = "Magikarp"},
        .expectation = {.base_stats = {20, 10, 55, 15, 20, 80}, .gender_threshold = 126, .level = 5},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::hitmonchan, .name = "Hitmonchan"},
        .expectation = {.base_stats = {50, 105, 79, 35, 110, 76}, .gender_threshold = -1, .level = 25},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::hitmonlee, .name = "Hitmonlee"},
        .expectation = {.base_stats = {50, 120, 53, 35, 110, 87}, .gender_threshold = -1, .level = 25},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::eevee, .name = "Eevee"},
        .expectation = {.base_stats = {55, 55, 50, 45, 65, 55}, .gender_threshold = 30, .level = 25},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::lapras, .name = "Lapras"},
        .expectation = {.base_stats = {130, 85, 80, 85, 95, 60}, .gender_threshold = 126, .level = 25},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::omanyte, .name = "Omanyte"},
        .expectation = {.base_stats = {35, 40, 100, 90, 55, 35}, .gender_threshold = 30, .level = 5},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::kabuto, .name = "Kabuto"},
        .expectation = {.base_stats = {30, 80, 90, 55, 45, 55}, .gender_threshold = 30, .level = 5},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::aerodactyl, .name = "Aerodactyl"},
        .expectation = {.base_stats = {80, 105, 65, 60, 75, 130}, .gender_threshold = 30, .level = 5},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerabra, .name = "Game Corner Abra"},
        .expectation = {.base_stats = {25, 20, 15, 105, 55, 90}, .gender_threshold = 63, .level = 0},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerclefairy, .name = "Game Corner Clefairy"},
        .expectation = {.base_stats = {70, 45, 48, 60, 65, 35}, .gender_threshold = 190, .level = 0},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerdratinifr, .name = "Game Corner Dratini FR"},
        .expectation = {.base_stats = {41, 64, 45, 50, 50, 50}, .gender_threshold = 126, .level = 0},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerdratinilg, .name = "Game Corner Dratini LG"},
        .expectation = {.base_stats = {41, 64, 45, 50, 50, 50}, .gender_threshold = 126, .level = 0},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerscyther, .name = "Game Corner Scyther"},
        .expectation = {.base_stats = {70, 110, 80, 55, 80, 105}, .gender_threshold = 126, .level = 25},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerpinsir, .name = "Game Corner Pinsir"},
        .expectation = {.base_stats = {65, 125, 100, 55, 70, 85}, .gender_threshold = 126, .level = 18},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::gamecornerporygon, .name = "Game Corner Porygon"},
        .expectation = {.base_stats = {65, 60, 70, 85, 75, 40}, .gender_threshold = -1, .level = 0},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::togepi, .name = "Togepi"},
        .expectation = {.base_stats = {35, 20, 65, 40, 65, 20}, .gender_threshold = 30, .level = 5},
    },
    {
        .input = {.target = PokemonFRLG_RngTarget::togepifast, .name = "Togepi pre-approved"},
        .expectation = {.base_stats = {35, 20, 65, 40, 65, 20}, .gender_threshold = 30, .level = 5},
    },
};

std::string to_string(const BaseStats& stats){
    return std::format("{}/{}/{}/{}/{}/{}", stats.hp, stats.attack, stats.defense, stats.spatk, stats.spdef, stats.speed);
}
std::string to_string(const GiftRngSpecies& species){
    return std::format(
        "base stats {}, gender threshold {}, level {}",
        to_string(species.base_stats), species.gender_threshold, species.level
    );
}

class Test_GiftSpecies : public UnitTest{
public:
    explicit Test_GiftSpecies(GiftSpeciesCase test_case)
        : UnitTest("PokemonFRLG::GiftRngAttempts - Gift species " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string expected = to_string(GiftRngSpecies{
            .base_stats = m_case.expectation.base_stats,
            .gender_threshold = m_case.expectation.gender_threshold,
            .level = m_case.expectation.level,
        });
        const std::string got = to_string(gift_rng_species(m_case.input.target));
        if (expected != got){
            return "Expected " + expected + "; got " + got;
        }
        return true;
    }

private:
    GiftSpeciesCase m_case;
};


//  Target: seed 0x70FE, advance 1000.
struct GiftShinyInput{
    std::string name = "";
    //  The (seed, advance) of each state the shiny's stats match.
    std::vector<std::pair<uint16_t, uint64_t>> hits = {};
};

struct GiftShinyExpectation{
    GiftRngOutcome expected = GiftRngOutcome::shiny;
};

struct GiftShinyCase{
    GiftShinyInput input;
    GiftShinyExpectation expectation;
};

const GiftShinyCase GIFT_SHINY_CASES[] = {
    {
        .input = {.name = "the target alone", .hits = {{0x70fe, 1000}}},
        .expectation = {.expected = GiftRngOutcome::shiny_on_target},
    },
    {
        .input = {.name = "the target among others", .hits = {{0x70fe, 998}, {0x70fe, 1000}, {0x70ff, 1000}}},
        .expectation = {.expected = GiftRngOutcome::shiny_on_target},
    },
    {
        .input = {.name = "another advance", .hits = {{0x70fe, 1002}}},
        .expectation = {.expected = GiftRngOutcome::shiny_off_target},
    },
    {
        .input = {.name = "another seed", .hits = {{0x70fd, 1000}}},
        .expectation = {.expected = GiftRngOutcome::shiny_off_target},
    },
    {
        .input = {.name = "no match", .hits = {}},
        .expectation = {.expected = GiftRngOutcome::shiny},
    },
};

std::string outcome_name(GiftRngOutcome outcome){
    switch (outcome){
    case GiftRngOutcome::shiny:             return "shiny";
    case GiftRngOutcome::shiny_on_target:   return "shiny_on_target";
    case GiftRngOutcome::shiny_off_target:  return "shiny_off_target";
    case GiftRngOutcome::target_hit:        return "target_hit";
    case GiftRngOutcome::out_of_resets:     return "out_of_resets";
    case GiftRngOutcome::no_matches:        return "no_matches";
    }
    return "?";
}

class Test_GiftShinyOutcome : public UnitTest{
public:
    explicit Test_GiftShinyOutcome(GiftShinyCase test_case)
        : UnitTest("PokemonFRLG::GiftRngAttempts - Shiny " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<AdvRngState> hits;
        for (const auto& [seed, advance] : m_case.input.hits){
            AdvRngState hit{};
            hit.seed = seed;
            hit.advance = advance;
            hits.push_back(hit);
        }
        const GiftRngOutcome got = gift_shiny_outcome(0x70fe, 1000, hits);
        if (got != m_case.expectation.expected){
            return std::string("Expected ") + outcome_name(m_case.expectation.expected) + ", got " + outcome_name(got);
        }
        return true;
    }

private:
    GiftShinyCase m_case;
};

} // namespace


void add_tests_GiftRngAttempts(UnitTestDatabase& database){
    for (const auto& item : GIFT_SPECIES_CASES){
        database.add<Test_GiftSpecies>(item);
    }
    for (const auto& item : GIFT_SHINY_CASES){
        database.add<Test_GiftShinyOutcome>(item);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

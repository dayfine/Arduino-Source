/*  Isolated Hunt Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */

#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntPlan.h"
#include "PokemonSV_IsolatedHunt_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

struct PlanCase{
    std::string name;
    std::vector<IsolatedHuntRequest> requests;
    std::vector<IsolatedHuntRequest> expected;
};

std::vector<PlanCase> plan_cases(){
    const IsolatedHuntRequest chespin{
        .pokemon = "chespin", .type = IsolatedEncounterType::GRASS,
        .location = IsolatedHuntLocation::CHARGESTONE_CAVE,
        .ball = "friend-ball", .ball_limit = 20,
    };
    const IsolatedHuntRequest charcadet{
        .pokemon = "charcadet", .type = IsolatedEncounterType::FIRE,
        .location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL,
        .ball = "luxury-ball", .ball_limit = 15,
    };
    auto wrong_type = chespin;
    wrong_type.type = IsolatedEncounterType::FIRE;
    auto wrong_location = chespin;
    wrong_location.location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL;
    auto no_ball = chespin;
    no_ball.ball.clear();
    auto zero_balls = chespin;
    zero_balls.ball_limit = 0;
    auto disabled = wrong_type;
    disabled.enabled = false;
    return {
        {.name = "Grass Chespin", .requests = {chespin}, .expected = {chespin}},
        {.name = "Fire Charcadet", .requests = {charcadet}, .expected = {charcadet}},
        {.name = "Mixed queue preserves order and catch settings", .requests = {chespin, charcadet}, .expected = {chespin, charcadet}},
        {.name = "Fire Chespin rejected", .requests = {wrong_type}, .expected = {}},
        {.name = "Wrong habitat rejected", .requests = {wrong_location}, .expected = {}},
        {.name = "Invalid later row rejects whole plan", .requests = {chespin, wrong_type}, .expected = {}},
        {.name = "Disabled invalid row ignored", .requests = {disabled, chespin}, .expected = {chespin}},
        {.name = "Empty ball rejected", .requests = {no_ball}, .expected = {}},
        {.name = "Zero ball limit rejected", .requests = {zero_balls}, .expected = {}},
        {.name = "Empty queue rejected", .requests = {}, .expected = {}},
        {.name = "Disabled-only queue rejected", .requests = {disabled}, .expected = {}},
    };
}

}

UnitTestResult test_isolated_hunt_plan(){
    for (const auto& test : plan_cases()){
        const auto plan = make_isolated_hunt_plan(test.requests);
        if (plan.error.empty() != !test.expected.empty() || plan.hunts.size() != test.expected.size()){
            return test.name + ": expected hunts=" + std::to_string(test.expected.size()) +
                "; read hunts=" + std::to_string(plan.hunts.size()) + ", error=" + plan.error;
        }
        for (size_t c = 0; c < plan.hunts.size(); c++){
            const auto& hunt = plan.hunts[c];
            const auto& expected = test.expected[c];
            if (hunt.pokemon != expected.pokemon || hunt.ball != expected.ball || hunt.ball_limit != expected.ball_limit){
                return test.name + ": expected " + expected.pokemon + "/" + expected.ball +
                    "/" + std::to_string(expected.ball_limit) + "; read " + hunt.pokemon +
                    "/" + hunt.ball + "/" + std::to_string(hunt.ball_limit);
            }
        }
    }
    return true;
}

UnitTestResult test_isolated_hunt_locations(){
    struct Case{
        IsolatedEncounterType type;
        std::string expected_pokemon;
        IsolatedHuntLocation expected_location;
    };
    const std::vector<Case> cases{
        {.type = IsolatedEncounterType::GRASS, .expected_pokemon = "chespin", .expected_location = IsolatedHuntLocation::CHARGESTONE_CAVE},
        {.type = IsolatedEncounterType::FIRE, .expected_pokemon = "charcadet", .expected_location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL},
    };
    for (const auto& test : cases){
        const auto locations = isolated_locations_for_type(test.type);
        if (locations.size() != 1){
            return "Expected one location for " + test.expected_pokemon + "; read " + std::to_string(locations.size());
        }
        const auto& location = locations.front();
        if (location.type != test.type || location.pokemon != test.expected_pokemon || location.location != test.expected_location){
            return "Expected compatible location for " + test.expected_pokemon + "; read " + location.pokemon + "/" + location.label;
        }
    }
    return true;
}

}
}
}

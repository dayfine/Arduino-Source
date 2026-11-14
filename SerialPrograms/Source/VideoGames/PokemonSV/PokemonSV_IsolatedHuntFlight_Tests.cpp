/*  Isolated Hunt Flight Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <vector>
#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntFlight.h"
#include "PokemonSV_IsolatedHuntFlight_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
struct FlightCase{
    std::string name;
    Language language = Language::ChineseTraditional;
    bool selected = true;
    std::vector<std::string> reads = {};
    bool prompt = true;
    bool landed = true;
    bool expected = false;
    std::string steps;
};
class ScriptedFlight : public IsolatedFlightActions{
public:
    ScriptedFlight(const FlightCase& test) : m_test(test){}
    std::string steps;
    virtual bool open_confirmation() override{ steps += "select "; return m_test.selected; }
    virtual std::string read_destination() override{
        steps += "read ";
        return m_next < m_test.reads.size() ? m_test.reads[m_next++] : "";
    }
    virtual bool confirmation_visible() override{ steps += "prompt "; return m_test.prompt; }
    virtual bool confirm_and_wait() override{ steps += "confirm "; return m_test.landed; }
private:
    const FlightCase& m_test;
    size_t m_next = 0;
};
std::vector<FlightCase> rejection_cases(){
    return {
        {.name = "Observed North pilot must never confirm", .reads = {"西第１區：北"}, .steps = "select read "},
        {.name = "Missing name", .reads = {""}, .steps = "select read "},
        {.name = "Partial name", .reads = {"西第１區"}, .steps = "select read "},
        {.name = "Other area", .reads = {"西第２區：中央"}, .steps = "select read "},
        {.name = "Conflicting second read", .reads = {"西第１區：中央", "西第１區：北"}, .steps = "select read read "},
        {.name = "Unreadable second read", .reads = {"西第１區：中央", ""}, .steps = "select read read "},
        {.name = "English North", .language = Language::English,
            .reads = {"West Province (Area One) - North"}, .steps = "select read "},
        {.name = "Unsupported language spends no input", .language = Language::Japanese, .steps = ""},
        {.name = "No confirmation found", .selected = false, .steps = "select "},
    };
}
std::vector<FlightCase> confirmation_cases(){
    return {
        {.name = "Central name twice before confirmation", .reads = {"西第１區：中央", "西第 1 區 : 中央"},
            .expected = true, .steps = "select read read prompt confirm "},
        {.name = "English Central", .language = Language::English,
            .reads = {"West Province (Area One) - Central", "West Province (Area One) - Central\n"},
            .expected = true, .steps = "select read read prompt confirm "},
        {.name = "Prompt disappeared after reads", .reads = {"西第１區：中央", "西第１區：中央"},
            .prompt = false, .steps = "select read read prompt "},
        {.name = "Verified name without landing is failure", .reads = {"西第１區：中央", "西第１區：中央"},
            .landed = false, .steps = "select read read prompt confirm "},
    };
}
}
UnitTestResult test_isolated_hunt_flight(){
    auto cases = rejection_cases();
    const auto confirmations = confirmation_cases();
    cases.insert(cases.end(), confirmations.begin(), confirmations.end());
    for (const auto& test : cases){
        ScriptedFlight actions(test);
        const bool actual = fly_to_isolated_central(actions, test.language);
        if (actual != test.expected || actions.steps != test.steps){
            return test.name + ": expected success=" + std::to_string(test.expected) + " [" + test.steps +
                "]; read success=" + std::to_string(actual) + " [" + actions.steps + "]";
        }
    }
    return true;
}
}
}
}

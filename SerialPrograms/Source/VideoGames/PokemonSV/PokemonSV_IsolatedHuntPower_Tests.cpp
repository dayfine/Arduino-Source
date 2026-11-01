#include <vector>
#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntPower.h"
#include "PokemonSV_IsolatedHuntPower_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
struct PowerCase{
    std::string name;
    Language language = Language::ChineseTraditional;
    IsolatedEncounterType type = IsolatedEncounterType::FIRE;
    uint8_t level = 2;
    std::vector<std::string> reads;
    bool opened = true;
    bool closed = true;
    bool expected = false;
    size_t expected_reads = 0;
    bool expected_close = false;
};
class ScriptedPower : public IsolatedPowerActions{
public:
    ScriptedPower(const PowerCase& test) : m_test(test){}
    size_t reads = 0;
    bool closed = false;
    virtual bool open_panel() override{ return m_test.opened; }
    virtual std::string read_encounter_power() override{
        return reads < m_test.reads.size() ? m_test.reads[reads++] : "";
    }
    virtual bool close_panel() override{ closed = true; return m_test.closed; }
private:
    const PowerCase& m_test;
};
std::vector<PowerCase> power_cases(){
    return {
        {.name = "Actual ordinary Fire level two panel", .reads = {"遭遇力：火 Lv. 2", "遭遇力：火 Lv. 2"},
            .expected = true, .expected_reads = 2, .expected_close = true},
        {.name = "English ordinary Fire level two", .language = Language::English,
            .reads = {"Encounter Power: Fire Lv. 2", "Encounter Power: Fire Lv. 2"},
            .expected = true, .expected_reads = 2, .expected_close = true},
        {.name = "Earlier Fire level one pilot is rejected", .reads = {"遭遇力：火 Lv. 1"}, .expected_reads = 1},
        {.name = "Wrong type", .reads = {"遭遇力：草 Lv. 2"}, .expected_reads = 1},
        {.name = "Other power cannot substitute", .reads = {"團戰力：火 Lv. 2"}, .expected_reads = 1},
        {.name = "Unreadable panel", .reads = {""}, .expected_reads = 1},
        {.name = "Conflicting second read", .reads = {"遭遇力：火 Lv. 2", "遭遇力：火 Lv. 1"}, .expected_reads = 2},
        {.name = "Missing second read", .reads = {"遭遇力：火 Lv. 2", ""}, .expected_reads = 2},
        {.name = "Panel opening failed", .opened = false},
        {.name = "Panel closure failed", .reads = {"遭遇力：火 Lv. 2", "遭遇力：火 Lv. 2"},
            .closed = false, .expected_reads = 2, .expected_close = true},
        {.name = "Unsupported language", .language = Language::Japanese},
        {.name = "Unsupported requested level", .level = 1},
        {.name = "Grass ordinary recipe", .type = IsolatedEncounterType::GRASS,
            .reads = {"遭遇力：草 Lv. 2", "遭遇力：草 Lv. 2"}, .expected = true, .expected_reads = 2, .expected_close = true},
        {.name = "Explicit herb power", .level = 3, .reads = {"遭遇力：火 Lv. 3", "遭遇力：火 Lv. 3"},
            .expected = true, .expected_reads = 2, .expected_close = true},
        {.name = "Full width digit", .reads = {"遭遇力：火 Lv. ２", "遭遇力：火 Lv. 2"},
            .expected = true, .expected_reads = 2, .expected_close = true},
    };
}
}
UnitTestResult test_isolated_hunt_power(){
    const auto cases = power_cases();
    for (const auto& test : cases){
        ScriptedPower actions(test);
        const bool accepted = verify_isolated_power(actions, test.language, test.type, test.level);
        if (accepted != test.expected || actions.reads != test.expected_reads || actions.closed != test.expected_close){
            return test.name + ": expected accepted=" + std::to_string(test.expected) +
                ", reads=" + std::to_string(test.expected_reads) + ", close=" + std::to_string(test.expected_close) +
                "; read accepted=" + std::to_string(accepted) + ", reads=" + std::to_string(actions.reads) +
                ", close=" + std::to_string(actions.closed);
        }
    }
    return true;
}
}
}
}

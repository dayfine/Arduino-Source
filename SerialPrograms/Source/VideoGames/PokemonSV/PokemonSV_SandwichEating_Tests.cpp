#include <algorithm>
#include <vector>
#include "Programs/Sandwiches/PokemonSV_SandwichEating.h"
#include "PokemonSV_SandwichEating_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
struct EatingCase{
    const char* name;
    uint64_t command_ms;
    uint64_t picnic_at_ms;
    bool expected;
    uint64_t expected_ms;
};
class ScriptedEating : public SandwichEatingActions{
public:
    ScriptedEating(const EatingCase& test) : m_test(test){}
    virtual uint64_t now_ms() const override{ return m_clock; }
    virtual bool confirm_and_observe(uint64_t observation_ms) override{
        m_clock += std::max(m_test.command_ms, observation_ms);
        const bool detected = m_clock >= m_test.picnic_at_ms;
        if (detected){ m_clock = m_test.picnic_at_ms; }
        return detected;
    }
private:
    const EatingCase& m_test;
    uint64_t m_clock = 0;
};
}
UnitTestResult test_sandwich_eating(){
    const std::vector<EatingCase> cases{
        {.name = "Early command completion must not truncate eating observation",
            .command_ms = 1240, .picnic_at_ms = 28000, .expected = true, .expected_ms = 28000},
        {.name = "Normal confirmation observes picnic immediately",
            .command_ms = 160, .picnic_at_ms = 2000, .expected = true, .expected_ms = 2000},
        {.name = "Long result transition still reaches picnic",
            .command_ms = 1240, .picnic_at_ms = 60000, .expected = true, .expected_ms = 60000},
        {.name = "Missing picnic stops at real time bound despite fast commands",
            .command_ms = 160, .picnic_at_ms = UINT64_MAX, .expected = false, .expected_ms = 80000},
        {.name = "Slow commands do not multiply the observation timeout",
            .command_ms = 5000, .picnic_at_ms = UINT64_MAX, .expected = false, .expected_ms = 80000},
    };
    for (const auto& test : cases){
        ScriptedEating actions(test);
        const bool actual = wait_for_sandwich_picnic(actions);
        if (actual != test.expected || actions.now_ms() != test.expected_ms){
            return std::string(test.name) + ": expected picnic=" + std::to_string(test.expected) +
                " at ms=" + std::to_string(test.expected_ms) + "; read picnic=" + std::to_string(actual) +
                " at ms=" + std::to_string(actions.now_ms());
        }
    }
    return true;
}
}
}
}

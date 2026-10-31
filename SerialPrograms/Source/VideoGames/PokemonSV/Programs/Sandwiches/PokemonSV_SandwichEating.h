#ifndef PokemonAutomation_PokemonSV_SandwichEating_H
#define PokemonAutomation_PokemonSV_SandwichEating_H

#include <cstdint>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class SandwichEatingActions{
public:
    virtual ~SandwichEatingActions() = default;
    virtual uint64_t now_ms() const = 0;
    // Confirm once and observe for picnic, allowing at least this much real time.
    virtual bool confirm_and_observe(uint64_t observation_ms) = 0;
};

bool wait_for_sandwich_picnic(SandwichEatingActions& actions);

}
}
}
#endif

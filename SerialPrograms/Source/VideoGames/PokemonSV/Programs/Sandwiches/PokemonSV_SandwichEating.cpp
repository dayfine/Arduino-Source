#include <algorithm>
#include "PokemonSV_SandwichEating.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

bool wait_for_sandwich_picnic(SandwichEatingActions& actions){
    const uint64_t deadline = actions.now_ms() + 80000;
    while (true){
        const uint64_t now = actions.now_ms();
        if (now >= deadline){ return false; }
        const uint64_t remaining = deadline - now;
        if (actions.confirm_and_observe(std::min<uint64_t>(4000, remaining))){ return true; }
    }
}

}
}
}

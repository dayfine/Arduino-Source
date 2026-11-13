/*  Story Checkpoint Gates
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace Pokemon{


GateState checkpoint_gates(const StoryCheckpoint& checkpoint, size_t steps_done){
    GateState gates = checkpoint.start_gates.value_or(GateState());
    const size_t done = std::min(steps_done, checkpoint.steps.size());
    for (size_t c = 0; c < done; c++){
        if (checkpoint.steps[c].gates_after){
            apply_gate_change(gates, *checkpoint.steps[c].gates_after);
        }
    }
    return gates;
}



} // namespace Pokemon
} // namespace PokemonAutomation

#include <cassert>
#include <vector>
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BBQQuestRetries.h"
namespace PokemonAutomation::NintendoSwitch::PokemonSV{
enum class BBQuests{lion, photo, other};
}
using namespace PokemonAutomation::NintendoSwitch::PokemonSV;
int main(){
    BBQQuestRetries state;
    auto lion = BBQuests::lion, photo = BBQuests::photo;
    assert(state.can_attempt(lion, 1));
    state.record_attempt(lion);
    assert(state.can_attempt(lion, 1));
    state.record_attempt(lion);
    assert(!state.can_attempt(lion, 1)); // Two attempts total, even across batches.
    state.observe_board({photo, lion}, false);
    assert(!state.can_attempt(lion, 1)); // Reordering cannot refresh the budget.
    state.record_attempt(photo);
    state.completed(photo);
    assert(state.can_attempt(photo, 0)); // Completing another quest doesn't reset lion.
    assert(!state.can_attempt(lion, 1));
    state.observe_board({photo}, false);
    assert(!state.can_attempt(lion, 1)); // Uncertain OCR cannot reset the budget.
    state.observe_board({photo}, true);
    assert(state.can_attempt(lion, 1)); // Observed removal, then same kind can be new.
    state.record_attempt(lion);
    assert(!state.can_attempt(lion, 0));
    assert(state.can_attempt(lion, 1)); // Live option changes use actual attempts.
    state.completed(lion);
    assert(state.can_attempt(lion, 0));
}

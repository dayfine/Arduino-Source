/*  Lab Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <random>
#include "LabTransition.h"

namespace PokemonAutomation{


std::string new_lab_uuid(){
    static thread_local std::mt19937_64 generator(std::random_device{}());
    const uint64_t high = (generator() & 0xffffffffffff0fffull) | 0x0000000000004000ull;
    const uint64_t low = (generator() & 0x3fffffffffffffffull) | 0x8000000000000000ull;
    return std::format(
        "{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
        high >> 32, (high >> 16) & 0xffff, high & 0xffff,
        low >> 48, low & 0xffffffffffffull
    );
}

bool is_lab_uuid(std::string_view text){
    if (text.size() != 36){
        return false;
    }
    for (size_t c = 0; c < text.size(); c++){
        const char ch = text[c];
        if (c == 8 || c == 13 || c == 18 || c == 23){
            if (ch != '-'){
                return false;
            }
        }else if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))){
            return false;
        }
    }
    return true;
}


std::optional<ProgramTransition> lab_transition_since(
    std::optional<ProgramTransition> last, WallClock selected_at
){
    if (last && last->at < selected_at){
        return std::nullopt;
    }
    return last;
}



}  // namespace PokemonAutomation

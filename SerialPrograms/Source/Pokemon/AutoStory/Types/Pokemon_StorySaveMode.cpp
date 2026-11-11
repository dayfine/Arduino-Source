/*  Story Save Mode
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_save_mode_name(StorySaveMode mode){
    switch (mode){
    case StorySaveMode::NONE:
        return "none";
    case StorySaveMode::AFTER_VERIFICATION:
        return "after_verification";
    case StorySaveMode::PER_CHECKPOINT:
        return "per_checkpoint";
    }
    return "?";
}

bool story_save_allowed(StorySaveMode mode, CheckpointVerdict verdict, SavePoint point){
    if (verdict != CheckpointVerdict::PASSED){
        return false;
    }
    switch (mode){
    case StorySaveMode::NONE:
        return false;
    case StorySaveMode::AFTER_VERIFICATION:
        return point == SavePoint::RUN_END;
    case StorySaveMode::PER_CHECKPOINT:
        return true;
    }
    return false;
}



} // namespace Pokemon
} // namespace PokemonAutomation

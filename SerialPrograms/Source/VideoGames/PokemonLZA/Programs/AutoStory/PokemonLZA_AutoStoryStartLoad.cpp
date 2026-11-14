/*  PokemonLZA AutoStory - Start load
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryStartLoad.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


StartLoadAction next_start_load_action(bool backup_first, std::span<const StartLoad> loads){
    if (loads.empty()){
        return backup_first ? StartLoadAction::LOAD_BACKUP : StartLoadAction::LOAD_LATEST;
    }
    const StartLoad& last = loads.back();
    if (!last.at_start || *last.at_start){
        return StartLoadAction::START;
    }
    if (last.save == StartSave::LATEST){
        return StartLoadAction::LOAD_BACKUP;
    }
    return StartLoadAction::STOP;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

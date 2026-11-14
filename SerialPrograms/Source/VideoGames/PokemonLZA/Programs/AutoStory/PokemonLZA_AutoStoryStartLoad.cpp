/*  PokemonLZA AutoStory - Start load
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryStartLoad.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


Pokemon::StoryLaunchRequest start_launch_request(StartSave save){
    using Pokemon::StoryLaunchRequest;
    switch (save){
    case StartSave::LATEST:
        return {.save = StoryLaunchRequest::Save::LATEST, .running = StoryLaunchRequest::Running::RESTART};
    case StartSave::BACKUP:
        return {.save = StoryLaunchRequest::Save::BACKUP, .running = StoryLaunchRequest::Running::RESTART};
    case StartSave::RUNNING:
        return {.save = StoryLaunchRequest::Save::LATEST, .running = StoryLaunchRequest::Running::KEEP};
    }
    return {};
}

StartLoadAction next_start_load_action(StartFrom from, std::span<const StartLoad> loads){
    if (loads.empty()){
        switch (from){
        case StartFrom::LATEST_SAVE:
            return StartLoadAction::LOAD_LATEST;
        case StartFrom::BACKUP_SAVE:
            return StartLoadAction::LOAD_BACKUP;
        case StartFrom::RUNNING_GAME:
            return StartLoadAction::RESUME;
        }
        return StartLoadAction::STOP;
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

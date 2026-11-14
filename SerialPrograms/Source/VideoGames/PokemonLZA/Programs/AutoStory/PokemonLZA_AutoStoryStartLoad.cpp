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
        const bool after_latest_failed = loads.size() >= 2 && loads[loads.size() - 2].save == StartSave::LATEST;
        if (last.save == StartSave::BACKUP && last.at_start && after_latest_failed){
            return StartLoadAction::REPORT_THEN_START;
        }
        return StartLoadAction::START;
    }
    if (last.save == StartSave::LATEST){
        return StartLoadAction::LOAD_BACKUP;
    }
    return StartLoadAction::STOP;
}

std::optional<StartSign> checkpoint_start_sign(std::string_view row){
    if (row == "03.06" || row == "03.07d"){
        return StartSign{.event = ScreenEvent::INTERACT_PROMPT, .place = "the Pokemon Center counter"};
    }
    return std::nullopt;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

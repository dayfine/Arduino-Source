/*  AutoStory Field Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryFieldSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PokemonCenterRoom.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

StoryStep game_step(std::string name, std::string action){
    StoryStep step;
    step.kind = StoryStepKind::GAME;
    step.name = std::move(name);
    step.action = std::move(action);
    return step;
}

} // namespace

StoryStep poster_switch_step(std::string name){
    return game_step(std::move(name), POSTER_SWITCH_VARIANT);
}

StoryStep lift_floor_step(std::string name, std::string_view from, std::string_view to){
    StoryStep step = game_step(std::move(name), LIFT_FLOOR_VARIANT);
    step.args = {{"from", std::string(from)}, {"to", std::string(to)}};
    if (const std::optional<LiftRide> ride = parse_lift_floor_step(step)){
        const MapPosition& exit = lift_exits(ride->lift)[ride->to];
        step.pose_after = Pokemon::TileDeclared{exit.map, {exit.point}, Direction::SOUTH};
    }
    return step;
}

StoryStep center_enter_step(std::string name, std::string_view center){
    StoryStep step = game_step(std::move(name), CENTER_ENTER_VARIANT);
    step.pose_after = Pokemon::TileDeclared{std::string(center), {CENTER_NURSE_FRONT}, Direction::NORTH};
    return step;
}

StoryStep center_exit_step(std::string name, Pokemon::GridPose below_door){
    StoryStep step = game_step(std::move(name), CENTER_EXIT_VARIANT);
    step.pose_after = Pokemon::TileDeclared{std::move(below_door.map), std::move(below_door.tiles), below_door.facing};
    return step;
}

StoryStep use_item_step(std::string name, std::string_view item, std::optional<Pokemon::GridPose> pose_after){
    StoryStep step = game_step(std::move(name), USE_ITEM_VARIANT);
    step.args = {{"item", std::string(item)}};
    if (pose_after){
        step.pose_after = Pokemon::TileDeclared{std::move(pose_after->map), std::move(pose_after->tiles), pose_after->facing};
    }
    return step;
}

StoryStep ride_bike_step(std::string name){
    return game_step(std::move(name), RIDE_BIKE_VARIANT);
}

StoryStep game_step_of(std::string_view text, std::span<const std::string_view> positional){
    std::vector<std::string_view> words;
    while (!text.empty()){
        const size_t space = text.find(' ');
        if (space != 0){
            words.push_back(text.substr(0, space));
        }
        if (space == std::string_view::npos){
            break;
        }
        text.remove_prefix(space + 1);
    }
    StoryStep step = game_step("", words.empty() ? std::string() : std::string(words[0]));
    size_t next = 0;
    for (size_t c = 1; c < words.size(); c++){
        const std::string_view word = words[c];
        const size_t equals = word.find('=');
        if (equals != std::string_view::npos && equals != 0){
            step.args[std::string(word.substr(0, equals))] = std::string(word.substr(equals + 1));
        }else if (next < positional.size()){
            step.args[std::string(positional[next++])] = std::string(word);
        }else{
            //  A word with no key: the step's action no longer matches.
            step.action = std::string(text);
        }
    }
    return step;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

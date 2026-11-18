/*  AutoStory Field Move Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_FlyMap.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryFieldMoveSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::Direction;
using Pokemon::StoryStep;


StoryStep fly_step(std::string name, size_t slot, std::string_view town){
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.action = FLY_VARIANT;
    step.args = {{"town", std::string(town)}, {"slot", (int64_t)slot}};
    if (const std::optional<size_t> index = fly_town_index(town)){
        const MapPosition& landing = kanto_fly_towns()[*index].landing;
        step.pose_after = Pokemon::TileDeclared{landing.map, {landing.point}, Direction::SOUTH};
    }
    return step;
}

StoryStep surf_step(std::string name, Pokemon::GridPose water){
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.action = SURF_VARIANT;
    if (water.facing){
        step.args = {{"direction", *water.facing}};
    }
    step.pose_after = Pokemon::TileDeclared{std::move(water.map), std::move(water.tiles), water.facing};
    return step;
}




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

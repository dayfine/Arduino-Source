/*  AutoStory Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/Navigation/Pokemon_GateTiles.h"
#include "PokemonFRLG_AutoStoryBoulders.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


Pokemon::Outcome boulder_pushed(Pokemon::RunState& state, const StoryStep& step, const std::vector<MapData>& maps){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    const int64_t tiles = Pokemon::story_arg<int64_t>(step.args, "tiles");
    StoryPose at{here->map, here->tile, direction};
    for (int64_t c = 0; c < tiles; c++){
        at.tile = at.tile.next(direction);
    }
    //  The boulder stands a tile ahead; on a floor switch's button, the
    //  switch is on for good and its barrier open.
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& item){
        return item.name == at.map;
    });
    const Pokemon::MapSwitch* pressed = map == maps.end() ? nullptr : Pokemon::switch_with_button(*map, at.tile.next(direction));
    if (pressed != nullptr){
        state.gates.switches_on.insert(Pokemon::switch_key(*pressed));
    }
    return Pokemon::outcome_ok(to_pose(at));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

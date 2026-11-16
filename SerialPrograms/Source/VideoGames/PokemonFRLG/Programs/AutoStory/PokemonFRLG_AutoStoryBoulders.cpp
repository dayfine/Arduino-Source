/*  AutoStory Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/Navigation/Pokemon_GateTiles.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
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

namespace{

//  A tap, and once more if it did not do what `wanted` asks and something
//  that stopped it was cleared.
BoulderPressRead press_for(Direction direction, bool (*wanted)(const BoulderPressRead&), BoulderPresses& presses){
    const BoulderPressRead read = presses.press(direction);
    if (wanted(read) || !presses.clear_interrupt()){
        return read;
    }
    return presses.press(direction);
}

bool pushed(const BoulderPressRead& read){
    return !read.scrolled && read.boulder_moved;
}

bool walked(const BoulderPressRead& read){
    return read.scrolled;
}

}  // namespace

BoulderPush push_boulder_tiles(Direction direction, int64_t tiles, BoulderPresses& presses){
    BoulderPush push;
    for (int64_t tile = 1; tile <= tiles; tile++){
        const BoulderPressRead push_read = press_for(direction, pushed, presses);
        if (push_read.scrolled){
            push.walked++;
            push.failure = std::format("tile {} of {}: the tap to push walked the player a tile: no boulder ahead.", tile, tiles);
            return push;
        }
        if (!push_read.boulder_moved){
            push.failure = std::format("tile {} of {}: the tap to push moved nothing: the boulder is stuck, or Strength is off.", tile, tiles);
            return push;
        }
        if (!walked(press_for(direction, walked, presses))){
            push.failure = std::format("tile {} of {}: the tap onto the tile the boulder left did not move the player.", tile, tiles);
            return push;
        }
        push.walked++;
    }
    return push;
}

Pokemon::Outcome boulder_push_outcome(const StoryStep& step, const std::vector<MapData>& maps, const BoulderPush& push, Pokemon::RunState& state){
    if (!push.failure){
        return boulder_pushed(state, step, maps);
    }
    Pokemon::Outcome outcome = Pokemon::outcome_failed("boulder_push", *push.failure);
    if (const std::optional<StoryPose> here = story_pose(state.pose)){
        const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
        StoryPose at{here->map, here->tile, direction};
        for (int64_t c = 0; c < push.walked; c++){
            at.tile = at.tile.next(direction);
        }
        outcome.pose = to_pose(at);
    }
    return outcome;
}

Pokemon::Outcome tree_cut(Pokemon::RunState& state, const StoryStep& step, const std::vector<MapData>& maps){
    const std::optional<StoryPose> here = story_pose(state.pose);
    if (!here){
        return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
    }
    const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
    const StoryPose on_tree{here->map, here->tile.next(direction), direction};
    if (const std::optional<std::string> key = cut_gate_key(maps, MapPosition{on_tree.map, on_tree.tile})){
        state.gates.opened.insert(*key);
    }
    return Pokemon::outcome_ok(to_pose(on_tree));
}

Pokemon::Outcome regrow_on_warp(const std::vector<MapData>& maps, const std::function<Pokemon::Outcome()>& run, Pokemon::RunState& state){
    const std::string map_before = state.pose.map;
    Pokemon::Outcome outcome = run();
    if (state.pose.map != map_before){
        regrow_cut_gates(maps, state.gates);
    }
    return outcome;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

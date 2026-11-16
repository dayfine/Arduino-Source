/*  Overworld Trainer Crossing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_OverworldObjects.h"
#include "PokemonFRLG_OverworldTrainerCrossing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The farthest sight of the trainers on `maps` but those `beaten` names
//  who may see `tile`; 0 if none does.
int sight_onto(const std::vector<MapData>& maps, const MapPosition& tile, const std::set<std::string>& beaten){
    int sight = 0;
    for (const MapData& map : maps){
        if (map.name != tile.map){
            continue;
        }
        for (const MapObject& object : map.objects){
            if (trainer_may_see(map, object, tile.point) && !beaten.contains(trainer_id(map, object))){
                sight = std::max(sight, object.trainer->sight);
            }
        }
    }
    return sight;
}

} // namespace

std::optional<TrainerCrossing> trainer_crossing(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step,
    const std::set<std::string>& beaten
){
    if (step.action != RouteAction::WALK){
        return std::nullopt;
    }
    TrainerCrossing crossing;
    const std::vector<MapPosition> tiles = leg_tiles(maps, start, step);
    for (size_t c = 1; c < tiles.size(); c++){
        const int sight = sight_onto(maps, tiles[c], beaten);
        if (sight > 0){
            crossing.seen.push_back(tiles[c]);
            crossing.approach_tiles = std::max(crossing.approach_tiles, sight);
            crossing.tiles_after_seen = (int)(tiles.size() - 1 - c);
        }
    }
    if (crossing.seen.empty()){
        return std::nullopt;
    }
    return crossing;
}

std::vector<std::string> beaten_trainers_crossed(
    const std::vector<MapData>& maps, const MapPosition& start, const RouteStep& step,
    const std::set<std::string>& beaten
){
    std::vector<std::string> crossed;
    if (step.action != RouteAction::WALK || beaten.empty()){
        return crossed;
    }
    const std::vector<MapPosition> tiles = leg_tiles(maps, start, step);
    for (size_t c = 1; c < tiles.size(); c++){
        for (const MapData& map : maps){
            if (map.name != tiles[c].map){
                continue;
            }
            for (const MapObject& object : map.objects){
                if (!object.trainer){
                    continue;
                }
                const std::string id = trainer_id(map, object);
                const bool listed = std::find(crossed.begin(), crossed.end(), id) != crossed.end();
                if (!listed && beaten.contains(id) && trainer_may_see(map, object, tiles[c].point)){
                    crossed.push_back(id);
                }
            }
        }
    }
    return crossed;
}

Milliseconds trainer_full_watch(int approach_tiles){
    return TRAINER_NOTICE + MovementMechanics::TILE_TIME * approach_tiles + TRAINER_SPEAKS;
}

Milliseconds trainer_first_look(const TrainerCrossing& crossing){
    const Milliseconds held_on = MovementMechanics::RUN_TILE_TIME * crossing.tiles_after_seen;
    return std::max(TRAINER_FIRST_LOOK, trainer_full_watch(crossing.approach_tiles) - held_on);
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

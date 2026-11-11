/*  AutoStory Legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A leg's step onto a spinner: only its last (`last`), which slides
//  `player` on to where the slide ends, facing its way (spin_slide()).
//  False mid-leg, or for a slide that loops or leaves through a warp.
bool land_on_spinner(const MapData& map, PlayerState& player, bool last, RouteStep& step){
    const std::optional<SpinSlide> slide = spin_slide(map, player);
    if (!last || !slide){
        return false;
    }
    player = slide->end;
    step.spin_tiles = slide->tiles;
    step.facing_after = slide->facing;
    return true;
}

} // namespace

std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
){
    const MapData* map = nullptr;
    for (const MapData& candidate : maps){
        if (candidate.name == start.map){
            map = &candidate;
        }
    }
    if (map == nullptr || !in_bounds(*map, start.point)){
        return std::nullopt;
    }
    std::vector<RouteStep> route;
    PlayerState player{start.point, elevation_at(*map, start.point), false};
    for (const StoryLeg& leg : legs){
        RouteStep step;
        step.direction = leg.direction;
        step.tiles = leg.tiles;
        step.facing_after = leg.direction;
        step.walk_only = !map->allow_running;
        for (int c = 0; c < leg.tiles; c++){
            const StepKind kind = classify_step(*map, player, leg.direction);
            const TilePos next = player.position.next(leg.direction);
            if (kind != StepKind::WALK || !in_bounds(*map, next) || warps_on_step(*map, next)){
                return std::nullopt;
            }
            step.wild |= is_tall_grass(*map, next);
            player = after_step(*map, player, leg.direction, kind);
            if (spin_direction(*map, player.position) && !land_on_spinner(*map, player, c + 1 == leg.tiles, step)){
                return std::nullopt;
            }
        }
        step.to_wall = !step.spin_tiles && classify_step(*map, player, leg.direction) == StepKind::BLOCKED;
        step.end = MapPosition{map->name, player.position};
        route.push_back(step);
    }
    return route;
}

std::optional<RouteStep> warp_press(const std::vector<MapData>& maps, const MapPosition& from, Direction direction){
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == from.map; });
    if (map == maps.end() || !in_bounds(*map, from.point)){
        return std::nullopt;
    }
    const TilePos ahead = from.point.next(direction);
    TilePos warp_tile = ahead;
    if (exit_mat(*map, from.point) == direction){
        warp_tile = from.point;
    }else if (!in_bounds(*map, ahead) || !((direction == Direction::NORTH && is_door(*map, ahead)) || warps_on_step(*map, ahead))){
        return std::nullopt;
    }
    const MapWarp* warp = warp_at(*map, warp_tile);
    if (warp == nullptr){
        return std::nullopt;
    }
    RouteStep step;
    step.direction = direction;
    step.tiles = 1;
    step.warps = true;
    step.facing_after = direction;
    step.end = MapPosition{warp->to ? warp->to->map : map->name, warp_tile};
    return step;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BDSP Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "PokemonBDSP_AutoStory_WallRecovery.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{

using Pokemon::Direction;
using Pokemon::TilePos;

//  A wall ray must stay on the same map and walk one tile per step. A
//  ledge may be legal travel but must not become a localization anchor.
std::optional<WallHit> safe_wall(const StoryWorld& world, TilePos from, Direction direction){
    const auto map = world.to_local(from);
    if (!map || !standable(world, from)){
        return std::nullopt;
    }
    const WallHit wall = slide_to_wall(world, from, direction);
    if (!wall.anchored){
        return std::nullopt;
    }
    while (from != wall.stop){
        const auto next = step(world, from, direction);
        const auto local = next ? world.to_local(*next) : std::nullopt;
        if (!next || *next != from.next(direction) || !local || local->map != map->map){
            return std::nullopt;
        }
        from = *next;
    }
    return wall;
}

class WallModel{
public:
    using Hypothesis = TilePos;
    using Position = TilePos;
    using Hold = TileLeg;

    explicit WallModel(const StoryWorld& world) : m_world(&world){}
    Position position(const Hypothesis& at) const{ return at; }
    int tiles(const Hold& hold) const{ return (int)hold.tiles; }
    std::optional<Pokemon::WallRecoveryStep<Hypothesis, Hold>> hold(
        const std::vector<Hypothesis>& from, Direction direction
    ) const{
        std::vector<Hypothesis> ends;
        size_t longest = 0;
        for (const auto& start : from){
            const auto wall = safe_wall(*m_world, start, direction);
            if (!wall){
                return std::nullopt;
            }
            longest = std::max(longest, wall->tiles);
            if (std::find(ends.begin(), ends.end(), wall->stop) == ends.end()){
                ends.push_back(wall->stop);
            }
        }
        return Pokemon::WallRecoveryStep<Hypothesis, Hold>{
            .positions = std::move(ends),
            .hold = {.direction = direction, .tiles = longest, .ends_at_wall = true},
        };
    }

private:
    const StoryWorld* m_world;
};

}

std::vector<Pokemon::WallAnchor<Pokemon::TilePos, TileLeg>> wall_recovery_candidates(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    const Pokemon::WallRecoveryLimits& limits
){
    for (const auto& tile : from){
        if (!standable(world, tile)){
            return {};
        }
    }
    return Pokemon::find_wall_anchors(WallModel(world), from, limits);
}

}
}
}

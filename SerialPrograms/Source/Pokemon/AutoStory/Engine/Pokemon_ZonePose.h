/*  Zone Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The pose of a game with no tile grid (PLZA): the zone the player is in,
 *  by name, until the game has a reader of its position. A StoryPose
 *  (Pokemon_StoryPoseConcept.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_ZonePose_H
#define PokemonAutomation_Pokemon_ZonePose_H

#include <optional>
#include <string>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon_StoryPoseConcept.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where the player is: a zone, "LumioseCity" or "HotelZ".
struct ZonePose{
    std::string zone = {};

    bool operator==(const ZonePose& other) const = default;
};

//  Whether `actual` is in `target`'s zone; any zone when `target` names none.
bool pose_matches(const ZonePose& actual, const ZonePose& target);

//  "zone LumioseCity".
std::string pose_text(const ZonePose& pose);

//  No tiles: no built-in tile walks. A declared pose names its zone as its
//  map, with no tiles and no facing.
template <>
struct StoryPoseTraits<ZonePose>{
    static constexpr bool TILE_MOVES = false;
    static std::optional<ZonePose> from_declared(const GridPose& declared){
        if (!declared.tiles.empty() || declared.facing){
            return std::nullopt;
        }
        return ZonePose{.zone = declared.map};
    }
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

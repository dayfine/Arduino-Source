/*  Declared Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A pose a checkpoint declares in its data (its start, a step's
 *  declared_after), as plain data. A tile game (FRLG, BDSP) declares a
 *  map, tiles and a facing; a game with no tile grid (PLZA) declares a
 *  place, a spot in it and a heading. Each game reads only its own kind
 *  (StoryPoseTraits::Declared); the lint fails a checkpoint that declares
 *  the other.
 *
 */

#ifndef PokemonAutomation_Pokemon_DeclaredPose_H
#define PokemonAutomation_Pokemon_DeclaredPose_H

#include <optional>
#include <string>
#include <variant>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  On one of `tiles` of `map` (any tile when none), facing `facing` if set.
struct TileDeclared{
    std::string map;
    std::vector<TilePos> tiles;
    std::optional<Direction> facing;

    bool operator==(const TileDeclared& other) const = default;
};

//  In `place` ("LumioseCity"), at `spot` in it if set, heading `heading`
//  degrees if set.
struct ZoneDeclared{
    std::string place;
    std::optional<std::string> spot;
    std::optional<int> heading;

    bool operator==(const ZoneDeclared& other) const = default;
};

//  Either kind; a game reads only its own (StoryPoseTraits::Declared).
using DeclaredPose = std::variant<TileDeclared, ZoneDeclared>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

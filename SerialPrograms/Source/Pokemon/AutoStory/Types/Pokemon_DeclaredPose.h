/*  Declared Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A pose a checkpoint declares in its data (its start), as plain data. A
 *  game with no tile grid (PLZA) declares a place, a spot in it and a
 *  heading.
 *
 */

#ifndef PokemonAutomation_Pokemon_DeclaredPose_H
#define PokemonAutomation_Pokemon_DeclaredPose_H

#include <optional>
#include <string>
#include <variant>
#include "Pokemon_StoryActionArg.h"

namespace PokemonAutomation{
namespace Pokemon{


//  In `place` ("LumioseCity"), at `spot` in it if set, heading `heading`
//  degrees if set.
struct ZoneDeclared{
    std::string place = {};
    std::optional<std::string> spot = {};
    std::optional<int> heading = {};

    bool operator==(const ZoneDeclared& other) const = default;
};

//  Where a checkpoint declares the player to be: a zone, or a pose of a kind
//  the game registers (a tile game's map, tiles and facing), read by the
//  StoryArgKind the game passes to the reader (Data/Pokemon_StoryStepJson.h).
using DeclaredPose = std::variant<ZoneDeclared, StoryRegisteredArgValue>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

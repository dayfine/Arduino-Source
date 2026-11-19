/*  Declared Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a checkpoint's data declares the player to be.
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

//  A zone, or a pose of a kind the game registers (StoryArgKind).
using DeclaredPose = std::variant<ZoneDeclared, StoryRegisteredArgValue>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

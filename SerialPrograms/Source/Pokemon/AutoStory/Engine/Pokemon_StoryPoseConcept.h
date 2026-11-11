/*  Story Pose Concept
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story engine needs of a game's pose type: whether a pose read
 *  off the game is a target, a line of text for the log, and its traits
 *  (StoryPoseTraits): whether the player moves tile by tile, and how a pose
 *  a checkpoint declares (its start, a step's end) reads as this type. The
 *  tile games (FRLG, BDSP) use GridPose; a game that moves freely brings
 *  its own type (PLZA: ZonePose, a zone until it has a position reader).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPoseConcept_H
#define PokemonAutomation_Pokemon_StoryPoseConcept_H

#include <concepts>
#include <optional>
#include <string>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What the engine may do with a pose type, beyond the concept below. Each
//  pose type specializes it next to its definition:
//
//      static constexpr bool TILE_MOVES;
//          Whether the player moves tile by tile. Only then does a game
//          get the built-in "legs" and "face" actions: a tap is a tile.
//      static std::optional<PoseType> from_declared(const GridPose& declared);
//          A pose a checkpoint declares in its JSON ("start", a step's
//          "declared_after": a map, tiles, a facing) as this type; none
//          when this type cannot say it (tiles in a game with none). The
//          lint fails a checkpoint whose declared poses don't fit.
template <typename PoseType>
struct StoryPoseTraits;

//  A tile game's pose: tile moves; a declared pose is one as it is.
template <>
struct StoryPoseTraits<GridPose>{
    static constexpr bool TILE_MOVES = true;
    static std::optional<GridPose> from_declared(const GridPose& declared){
        return declared;
    }
};

//  A pose the engine can carry: pose_matches(actual, target) and
//  pose_text(pose), found by argument-dependent lookup, and its traits.
template <typename PoseType>
concept StoryPose = std::copyable<PoseType> && requires(const PoseType& actual, const PoseType& target, const GridPose& declared){
    { pose_matches(actual, target) } -> std::convertible_to<bool>;
    { pose_text(actual) } -> std::convertible_to<std::string>;
    { StoryPoseTraits<PoseType>::TILE_MOVES } -> std::convertible_to<bool>;
    { StoryPoseTraits<PoseType>::from_declared(declared) } -> std::same_as<std::optional<PoseType>>;
};

//  Whether `actual` is on `target`'s map, each of its tiles is one of
//  `target`'s (any tile when `target` names none), and it faces `target`'s
//  facing when `target` has one.
bool pose_matches(const GridPose& actual, const GridPose& target);

//  "PalletTown (3, 4) facing north".
std::string pose_text(const GridPose& pose);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

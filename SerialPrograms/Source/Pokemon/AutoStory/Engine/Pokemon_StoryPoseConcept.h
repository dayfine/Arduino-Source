/*  Story Pose Concept
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story engine needs of a game's pose type: whether a pose read
 *  off the game is a target, a line of text for the log, and its traits
 *  (StoryPoseTraits): whether the player moves tile by tile, and which kind
 *  of declared pose (Types/Pokemon_DeclaredPose.h) a checkpoint declares
 *  for it (its start, a step's end) and how that reads as this type. The
 *  tile games (FRLG, BDSP) use GridPose; a game that moves freely brings
 *  its own type (PLZA: ZonePose, a zone until it has a position reader).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPoseConcept_H
#define PokemonAutomation_Pokemon_StoryPoseConcept_H

#include <concepts>
#include <string>
#include <variant>
#include "Pokemon/AutoStory/Types/Pokemon_DeclaredPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What the engine may do with a pose type, beyond the concept below. Each
//  pose type specializes it next to its definition:
//
//      static constexpr bool TILE_MOVES;
//          Whether the player moves tile by tile. Only then does a game
//          get the built-in "legs" and "face" actions: a tap is a tile.
//          A game with no tiles may not use the tile args (DIRECTION, LEGS,
//          GRID_POSE): the lint fails a step or an action that does.
//      using Declared = TileDeclared or ZoneDeclared;
//          The kind of pose a checkpoint of this game declares in its JSON
//          ("start", a step's "declared_after"). The lint fails a
//          checkpoint that declares the other kind (check_declared()).
//      static PoseType from_declared(const Declared& declared);
//          That declared pose as this type.
template <typename PoseType>
struct StoryPoseTraits;

//  A tile game's pose: tile moves; it declares a map, tiles and a facing.
template <>
struct StoryPoseTraits<GridPose>{
    static constexpr bool TILE_MOVES = true;
    using Declared = TileDeclared;
    static GridPose from_declared(const TileDeclared& declared){
        return GridPose{.map = declared.map, .tiles = declared.tiles, .facing = declared.facing};
    }
};

//  A pose the engine can carry: pose_matches(actual, target) and
//  pose_text(pose), found by argument-dependent lookup, and its traits.
template <typename PoseType>
concept StoryPose = std::copyable<PoseType> && requires(
    const PoseType& actual, const PoseType& target,
    const typename StoryPoseTraits<PoseType>::Declared& declared
){
    { pose_matches(actual, target) } -> std::convertible_to<bool>;
    { pose_text(actual) } -> std::convertible_to<std::string>;
    { StoryPoseTraits<PoseType>::TILE_MOVES } -> std::convertible_to<bool>;
    { StoryPoseTraits<PoseType>::from_declared(declared) } -> std::same_as<PoseType>;
};

//  Whether `declared` is the kind of pose a game of `PoseType` declares.
template <StoryPose PoseType>
bool check_declared(const DeclaredPose& declared);

template <StoryPose PoseType>
bool check_declared(const DeclaredPose& declared){
    return std::holds_alternative<typename StoryPoseTraits<PoseType>::Declared>(declared);
}

//  Whether `actual` is on `target`'s map, each of its tiles is one of
//  `target`'s (any tile when `target` names none), and it faces `target`'s
//  facing when `target` has one.
bool pose_matches(const GridPose& actual, const GridPose& target);

//  "PalletTown (3, 4) facing north".
std::string pose_text(const GridPose& pose);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

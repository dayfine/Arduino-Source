/*  Story Pose Concept
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story engine needs of a game's pose type: whether a pose read
 *  off the game is a target, and a line of text for the log. The tile
 *  games (FRLG, BDSP) use GridPose; a game that moves freely brings its
 *  own type (PLZA: a zone until it has a position reader).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPoseConcept_H
#define PokemonAutomation_Pokemon_StoryPoseConcept_H

#include <concepts>
#include <string>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A pose the engine can carry: pose_matches(actual, target) and
//  pose_text(pose), found by argument-dependent lookup.
template <typename PoseType>
concept StoryPose = std::copyable<PoseType> && requires(const PoseType& actual, const PoseType& target){
    { pose_matches(actual, target) } -> std::convertible_to<bool>;
    { pose_text(actual) } -> std::convertible_to<std::string>;
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

/*  AutoStory Level Ups
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a level up brings: an evolution by level, and the answer to
 *  "Delete a move to make room for ...?".
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLevelUps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLevelUps_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_AutoStoryStats.h"
#include "PokemonFRLG_AutoStoryPartyMember.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The species `species` evolves into by level, and at which level.
struct LevelEvolution{
    std::string_view into;
    uint8_t level = 0;
};
//  Level-up evolution target if species evolves by level.
std::optional<LevelEvolution> level_evolution(std::string_view species);
//  The answer to "Delete a move to make room for <move>?".
struct LearnDecision{
    //  The move offered, if the table knows which one it is.
    std::optional<std::string> move = {};
    //  The slot to forget for it, or none to keep the moves as they are.
    std::optional<size_t> forget_slot = {};
};

//  At "Delete a move to make room for ...?": takes in the moves learned
//  into free slots since the last prompt, decides on the move offered, and
//  leaves `member` as that answer will.
LearnDecision learn_move(PartyMember& member);

//  The lead evolved: its species becomes what it evolves into.
void evolve(PartyMember& member);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

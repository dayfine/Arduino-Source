/*  AutoStory New Members
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A party member as the game gives it, as the story grows it, or as its
 *  summary shows it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryNewMembers_H
#define PokemonAutomation_PokemonFRLG_AutoStoryNewMembers_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_AutoStoryStats.h"
#include "PokemonFRLG_AutoStoryLevelUps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  As the game gives `species` at `level` (a starter, a gift): its last four
//  level-up moves up to `level`, at full PP.
PartyMember new_member(std::string_view species, uint8_t level);

//  The member as the story plays it up to `level`: each level-up move is
//  learned into a free slot or answered as learn_move() says, and it
//  evolves at its level. For the rows that start after a part of the story.
PartyMember grown_to(PartyMember member, uint8_t level);

//  Squirtle as the story leaves it after Brock (Lv13, Water Gun offered).
PartyMember story_lead_after_brock();

//  `species` at `level` with `moves` as its summary shows them, in slot
//  order, and its `stats` if POKEMON SKILLS was read. A PP that wasn't
//  read is taken as half the move's full PP: the game draws the PP in
//  color from half down (pokefirered pokemon_summary_screen.c), which the
//  reader may not read.
PartyMember member_from_summary(
    std::string_view species, uint8_t level, const std::vector<SummaryMove>& moves,
    const std::optional<Stats>& stats = std::nullopt
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

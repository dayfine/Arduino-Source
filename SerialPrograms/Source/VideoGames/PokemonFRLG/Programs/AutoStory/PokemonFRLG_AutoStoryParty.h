/*  AutoStory Party
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lead's moves as the story knows them, and the data its choices come
 *  from: level-up moves and evolutions (pret/pokefirered
 *  src/data/pokemon/level_up_learnsets.h, evolution.h), which new move
 *  takes which old one's slot, and which moves the lead attacks with
 *  first. Moves' PP, power and type, species' types and trainers' parties
 *  are the generated battle data (PokemonFRLG_AutoStoryBattleData.h).
 *
 *  The story starts from the moves read off the lead's summary when a run
 *  loads a save (member_from_summary()), else from the moves a checkpoint's
 *  row says the lead has, and follows the game: a move learned into a free
 *  slot shows no prompt, so at the next "Delete a move?" the moves due
 *  before it are taken as learned. Which move is offered is the next
 *  level-up move after the last one, so the level isn't needed.
 *
 *  Attacks are picked by name and PP, never by slot: a save's moves can sit
 *  in other slots than the plan's (console B, 2026-11-02: Water Gun took
 *  Tackle's slot, not Bubble's).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryParty_H
#define PokemonAutomation_PokemonFRLG_AutoStoryParty_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_AutoStoryStats.h"
#include "PokemonFRLG_AutoStoryNewMembers.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  The slot to attack with: the first of the plan's attacks for its line
//  that it knows with PP left, else its first damaging move with PP left,
//  else any move with PP left, else the top left (Struggle). None while the
//  story doesn't know all its moves (fewer than four, not read).
std::optional<size_t> attack_slot(const PartyMember& member);

//  The slot to attack `foes` with, any of them as likely to be out: the
//  damaging move with PP left whose type multiplier, summed over the foes,
//  is best; ties go to the plan's attacks, then slot order. A foe the data
//  doesn't have is left out; with none left, or while the story doesn't
//  know all the lead's moves, attack_slot() itself.
std::optional<size_t> attack_slot_against(const PartyMember& member, const std::vector<std::string>& foes);

//  The PP left in the damaging moves it knows.
size_t attack_pp(const PartyMember& member);

//  One use of the move in `slot`.
void spend_pp(PartyMember& member, size_t slot);

//  The game said the move in `slot` has no PP left.
void no_pp_left(PartyMember& member, size_t slot);

//  The nurse heals: every move back to full PP.
void restore_pp(PartyMember& member);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

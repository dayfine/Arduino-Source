/*  AutoStory Party
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lead's moves as the story knows them, and the data its choices come
 *  from: level-up moves and evolutions (pret/pokefirered
 *  src/data/pokemon/level_up_learnsets.h, evolution.h), each move's PP
 *  (src/data/battle_moves.h), which new move takes which old one's slot,
 *  and which moves the lead attacks with first.
 *
 *  Nothing reads the moves off the screen. The story starts from the moves
 *  a checkpoint's row says the lead has, and follows the game: a move
 *  learned into a free slot shows no prompt, so at the next "Delete a move?"
 *  the moves due before it are taken as learned. Which move is offered is
 *  the next level-up move after the last one, so the level isn't needed.
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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The move slots, in the order of the battle's move list and the summary.
inline constexpr size_t TOP_LEFT_MOVE = 0;
inline constexpr size_t BOTTOM_LEFT_MOVE = 2;
inline constexpr size_t MAX_MOVES = 4;

//  A move the lead knows, by slug ("water-gun"), and the PP the story
//  counts left.
struct KnownMove{
    std::string move;
    uint8_t pp = 0;
};

struct PartyMember{
    std::string species;
    //  In slot order. Fewer than four while moves learned into free slots
    //  haven't been taken in yet (see the top of this file).
    std::vector<KnownMove> moves;
    //  The level of the last level-up move offered to it, learned or not.
    uint8_t moves_through_level = 0;
};

//  The species `species` evolves into by level, and at which level.
struct LevelEvolution{
    std::string_view into;
    uint8_t level = 0;
};
std::optional<LevelEvolution> level_evolution(std::string_view species);

//  A move's full PP; 0 for a move the table doesn't have.
uint8_t max_pp(std::string_view move);

//  As the game gives `species` at `level` (a starter, a gift): its last four
//  level-up moves up to `level`, at full PP.
PartyMember new_member(std::string_view species, uint8_t level);

//  The member as the story plays it up to `level`: each level-up move is
//  learned into a free slot or answered as learn_move() says, and it
//  evolves at its level. For the rows that start after a part of the story.
PartyMember grown_to(PartyMember member, uint8_t level);

//  Squirtle as the story leaves it after Brock (Lv13, Water Gun offered).
PartyMember story_lead_after_brock();


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


//  The slot to attack with: the first of the plan's attacks for its line
//  that it knows with PP left, else any move with PP left, else the top
//  left (Struggle). None while the story doesn't know all four moves.
std::optional<size_t> attack_slot(const PartyMember& member);

//  The PP left in the plan's attacks it knows.
size_t attack_pp(const PartyMember& member);

//  One use of the move in `slot`.
void spend_pp(PartyMember& member, size_t slot);

//  The game said the move in `slot` has no PP left.
void no_pp_left(PartyMember& member, size_t slot);

//  The nurse heals: every move back to full PP.
void restore_pp(PartyMember& member);



}
}
}
#endif

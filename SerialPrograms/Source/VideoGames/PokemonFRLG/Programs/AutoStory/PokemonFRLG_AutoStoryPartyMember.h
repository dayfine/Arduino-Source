/*  AutoStory Party Member
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A party member as the story knows it: species, level, moves and their
 *  PP, and the moves' full PP and power.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPartyMember_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPartyMember_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_AutoStoryStats.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The move slots, in the order of the battle's move list and the summary.
inline constexpr size_t TOP_LEFT_MOVE = 0;
//  Move slot index for bottom-left move.
inline constexpr size_t BOTTOM_LEFT_MOVE = 2;
//  Maximum number of moves a Pokemon can know.
inline constexpr size_t MAX_MOVES = 4;

//  A move the lead knows, by slug ("water-gun"), and the PP the story
//  counts left.
struct KnownMove{
    std::string move;
    uint8_t pp = 0;
};

//  Party member Pokemon status and learned moves.
struct PartyMember{
    std::string species;
    //  In slot order. Fewer than four while moves learned into free slots
    //  haven't been taken in yet (see the top of this file).
    std::vector<KnownMove> moves;
    //  The level of the last level-up move offered to it, learned or not.
    uint8_t moves_through_level = 0;
    //  The moves were read off its summary: they are all it knows, even
    //  fewer than four.
    bool moves_read = false;
    //  Its level as its summary showed it, 0 if not read. Level-ups since
    //  aren't seen: battle_level() takes the higher of this and
    //  moves_through_level.
    uint8_t level = {};
    //  Its stats as POKEMON SKILLS showed them at `level`, as this
    //  species; none if not read, or once it evolves.
    std::optional<Stats> stats = {};
};

//  The level the story takes for `member`: its level, or the level its
//  moves were grown through if that is higher.
uint8_t battle_level(const PartyMember& member);
//  A move's full PP; 0 for a move the table doesn't have.
uint8_t max_pp(std::string_view move);

//  A move's power; 0 for one that does no damage, or that the table
//  doesn't have.
uint8_t move_power(std::string_view move);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

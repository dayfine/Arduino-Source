/*  BDSP AutoStory - Battle moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Which move to use: the move menu's names, read in the game's language,
 *  and PP, scored by the power of the early moves the starters and their
 *  foes have.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStoryBattleMoves_H
#define PokemonAutomation_PokemonBDSP_AutoStoryBattleMoves_H

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStoryBattleMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  One move on the move menu as read: the slug of the move name dictionary
//  ("" when the name was not read) and the PP left (-1 when not read).
struct MoveSlot{
    std::string slug;
    int pp = -1;

    //  What the move in 0-based slot `slot` is taken for, for the log.
    std::string describe(size_t slot) const;
};

//  The Pokemon a battle's foe may send out, each by its types in lower case
//  ("rock", "ground"). The rival's starter is the one strong against ours, so
//  its types are told from our moves: `rival_starter`.
struct FoeTeam{
    std::vector<std::vector<std::string_view>> members = {};
    bool rival_starter = false;
};

//  The foe team the story knows by `name` (a battle step's "foe"), if any.
std::optional<FoeTeam> known_foe_team(std::string_view name);

//  The best of `slots`, 1-based: the most powerful one we know against
//  `foe`, and on a tie one that is not Normal (an early starter's own
//  type). Which of the foe's Pokemon is out is not read, so a move counts
//  as it does against the one that takes it worst; with no foe known every
//  move counts at its power. A move with no PP left is passed over. A move
//  whose name was not read is told by what the starters have in its slot
//  and the PP it has left, which rules out moves with less most PP; one
//  that might still be either comes after every attack, earliest first.
size_t choose_move(const std::vector<MoveSlot>& slots, const FoeTeam& foe = {});

//  The names, in the story's language, and PP of the moves on the move
//  menu `moves` of `screen`.
std::vector<MoveSlot> read_move_menu(const ImageViewRGB32& screen, const BattleMoveMenuState& moves);

//  Updates `slots` from `cache` for unread names. If any newly-read slot
//  differs from `cache`, the active Pokemon changed (e.g. a switch after a
//  faint) and unread slots are cleared instead of keeping the old Pokemon's.
void apply_remembered_moves(std::vector<MoveSlot>& slots, std::array<std::string, 4>& cache);

//  Clears the cross-turn move-name cache used by pick_best_move().
void clear_remembered_moves();

//  Whether the currently cached active move list contains `slug`.
bool remembered_move_includes(std::string_view slug);

//  The best move against `foe` on the move menu `moves` of `screen`,
//  1-based, as choose_move() picks from the names read in the game's
//  language and the PP left. Keeps a screenshot of the menu, labelled `label`.
size_t pick_best_move(
    VideoStream& stream, const ImageViewRGB32& screen, const BattleMoveMenuState& moves,
    const FoeTeam& foe, const std::string& label
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

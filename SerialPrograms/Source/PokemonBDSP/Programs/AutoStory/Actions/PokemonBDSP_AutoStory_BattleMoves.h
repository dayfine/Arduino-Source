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

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BattleMoves_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BattleMoves_H

#include <string>
#include <vector>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h"

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

//  The best of `slots`, 1-based: the most powerful one we know, and on a
//  tie one that is not Normal (an early starter's own type). A move with
//  no PP left is passed over. A move whose name was not read is told by
//  what the starters have in its slot and the PP it has left, which rules
//  out moves with less most PP; one that might still be either comes after
//  every attack, earliest first.
size_t choose_move(const std::vector<MoveSlot>& slots);

//  The names, in the story's language, and PP of the moves on the move
//  menu `moves` of `screen`.
std::vector<MoveSlot> read_move_menu(const ImageViewRGB32& screen, const BattleMoveMenuState& moves);

//  The best move on the move menu `moves` of `screen`, 1-based, as
//  choose_move() picks from the names read in the game's language and the
//  PP left. Keeps a screenshot of the menu, labelled `label`.
size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen, const BattleMoveMenuState& moves, const std::string& label);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

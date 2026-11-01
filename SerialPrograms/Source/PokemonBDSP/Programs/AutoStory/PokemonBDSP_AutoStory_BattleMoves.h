/*  BDSP AutoStory - Battle moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Which move to use: the move menu's names and PP read with OCR, scored by
 *  the power of the early moves the starters and their foes have.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BattleMoves_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BattleMoves_H

#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The best move on the move menu `moves` of `screen`, 1-based: the most
//  powerful one we know, and on a tie one that is not Normal (an early
//  starter's own type). A move with no PP left is passed over; with no
//  attack read, the newest move that is not a status move. Keeps a
//  screenshot of the menu, labelled `label`.
size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen, const BattleMoveMenuState& moves, const std::string& label);



}
}
}
#endif

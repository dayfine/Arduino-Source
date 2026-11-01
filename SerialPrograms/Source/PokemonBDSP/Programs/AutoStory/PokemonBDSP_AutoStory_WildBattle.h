/*  BDSP AutoStory - Wild battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  The story's wild battles: the Starly the starter is chosen against, one
 *  found in the grass to fight or to catch, and running from one that
 *  interrupts a walk.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WildBattle_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WildBattle_H

#include <chrono>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The Starly battle at Lake Verity, from its first text to the overworld.
void fight_starly(VideoStream& stream, ProControllerContext& context);

//  Finds one wild Pokemon in the grass by walking up and down a column
//  between two tree walls, and fights it with the best move. Does not heal.
void battle_1_pokemon(VideoStream& stream, ProControllerContext& context);

//  Finds one wild Pokemon the same way and catches it with the first ball
//  in the Bag, up to ten throws; ends on the overworld.
void catch_1_pokemon(VideoStream& stream, ProControllerContext& context);

//  Runs from the wild battle on screen: Run on the battle menu until the
//  exit's black screen shows. Fails with a screenshot after `timeout`.
void escape_wild_battle(
    VideoStream& stream,
    ProControllerContext& context,
    BattleType battle_type = BattleType::STANDARD,
    std::chrono::seconds timeout = std::chrono::seconds(30)
);

//  escape_wild_battle() for a standard wild battle, as the walker takes it
//  (WildBattleEscape): how the story's walks leave the battles that
//  interrupt them.
void run_from_wild_battle(VideoStream& stream, ProControllerContext& context);



}
}
}
#endif

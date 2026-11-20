/*  PokemonLZA AutoStory - Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's battles on the console, over the LZA battle code
 *  (TrainerBattleState): what PlzaConsoleBackend::battle() runs.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBattles_H
#define PokemonAutomation_PokemonLZA_AutoStoryBattles_H

#include <chrono>
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A story battle, from its HUD or the talk before it: attacks with
//  TrainerBattleState (`battle_ai`'s moves) until a dialogue box shows (the
//  speech after a win or a loss). Until the first attack lands, a dialogue
//  box or a failed lock-on gets A instead: the trainer's lines and the
//  tutorial cards. Story battles show no Run prompt, which upstream's
//  battle loops start on. Throws OperationFailedException after `timeout`.
void fight_story_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

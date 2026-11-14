/*  PokemonLZA AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle and save actions of the story game (PokemonLZA_AutoStoryGame.h),
 *  with the dialogue ones in PokemonLZA_AutoStoryDialogs.h, the walks in
 *  PokemonLZA_AutoStoryMoves.h and the scenes in PokemonLZA_AutoStoryScenes.h.
 *  Each is a short function over the LZA battle and menu code that already
 *  exists (TrainerBattle, BasicNavigation, the dialog and selection-arrow
 *  detectors). An action the game has no code for yet throws
 *  Pokemon::StoryActionNotImplemented, naming what is missing, instead of
 *  pressing buttons blind.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActions_H
#define PokemonAutomation_PokemonLZA_AutoStoryActions_H

#include <chrono>
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a trainer's or a wild Pokemon's battle starting, fights it with
//  TrainerBattleState (`battle_ai`'s moves), pressing B through the text
//  around it, until the overworld is back. Throws OperationFailedException
//  if neither shows for two minutes.
void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
);

//  A story battle, from the talk before it: attacks with TrainerBattleState
//  until a dialog box shows (the speech after a win or a loss). Until the
//  first attack lands, a dialog box or a failed lock-on gets A instead: the
//  trainer's lines, the challenge cutscene's subtitle (no text box) and
//  tutorial cards (Taunie's battle in 03.02, console B 2026-11-10). For
//  story battles, which show no Run prompt for fight_battle() to start on.
//  Throws OperationFailedException after `timeout`.
void fight_battle_in_progress(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
);

//  From the overworld, saves the game (save_game_to_menu()) and returns to
//  the overworld. The first menu of a playthrough shows a save tutorial
//  card, which it closes with A. If the save fails, B back to the
//  overworld and one more try; throws OperationFailedException if that
//  fails too.
void save_story_game(ConsoleHandle& console, ProControllerContext& context);

//  From the overworld: the main menu, DOWN x3 to "Mable's Research" (as
//  recorded on 2026-11-09), A; through Mable's lines on the reward, back
//  to the overworld.
void claim_mable_research_reward(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

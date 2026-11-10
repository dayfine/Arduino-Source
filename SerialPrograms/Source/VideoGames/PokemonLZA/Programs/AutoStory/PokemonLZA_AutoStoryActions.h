/*  PokemonLZA AutoStory - Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console actions the story executor (PokemonLZA_AutoStoryExecutor.h)
 *  is made of, with the dialogue ones in PokemonLZA_AutoStoryDialogs.h and
 *  the walks in PokemonLZA_AutoStoryMoves.h. Each is a short function over the LZA navigation, battle and
 *  detector code that already exists (BasicNavigation, FastTravelNavigation,
 *  TrainerBattle, the dialog and selection-arrow detectors). An action the
 *  game has no code for yet throws StoryActionNotImplemented, naming what is
 *  missing, instead of pressing buttons blind.
 *
 *  None of these has run on a console as a story step yet.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActions_H
#define PokemonAutomation_PokemonLZA_AutoStoryActions_H

#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A story action this game has no code for yet. The story executor lets
//  it end the run instead of reloading and trying again, since every
//  attempt would fail the same way.
class StoryActionNotImplemented : public OperationFailedException{
public:
    explicit StoryActionNotImplemented(std::string message);

    virtual const char* name() const override{
        return "StoryActionNotImplemented";
    }
};

//  Throws StoryActionNotImplemented for `action`, with `todo` saying what
//  code it is waiting for.
[[noreturn]] void throw_not_implemented(ConsoleHandle& console, std::string_view action, std::string_view todo);


//  From a trainer's or a wild Pokemon's battle starting, fights it with
//  TrainerBattleState (`battle_ai`'s moves), pressing B through the text
//  around it, until the overworld is back. Throws OperationFailedException
//  if neither shows for two minutes.
void fight_battle(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai
);

//  In a battle that is already on screen: attacks with TrainerBattleState
//  until a dialog box shows (the speech after a win or a loss). For story
//  battles, which show no Run prompt for fight_battle() to start on.
//  Throws OperationFailedException after `timeout`.
void fight_battle_in_progress(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, std::chrono::seconds timeout
);

//  At a Pokemon Center: heal the party. Not implemented: throws
//  StoryActionNotImplemented.
void heal_at_pokemon_center(ConsoleHandle& console, ProControllerContext& context);

//  At a shop counter: buy `items`. Not implemented: throws
//  StoryActionNotImplemented.
void buy_items(ConsoleHandle& console, ProControllerContext& context, const std::vector<std::string>& items);

//  From the overworld, saves the game (save_game_to_menu()) and returns to
//  the overworld. The first menu of a playthrough shows a save tutorial
//  card, which it closes with A. If the save fails, B back to the
//  overworld and one more try; throws OperationFailedException if that
//  fails too.
void save_story_game(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

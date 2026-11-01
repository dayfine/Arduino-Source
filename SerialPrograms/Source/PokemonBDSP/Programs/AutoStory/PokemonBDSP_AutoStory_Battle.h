/*  BDSP AutoStory - Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  Fighting a battle with the best move on the menu: the move names are read
 *  and scored by power, so it works whatever the starter knows.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Battle_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Battle_H

#include <string>
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A battle's text box, as a watcher.
class BattleDialogWatcher : public DetectorToFinder<BattleDialogDetector>{
public:
    BattleDialogWatcher(Color color = COLOR_RED);
};

//  Mashes B until the overworld has shown with no dialogue for four seconds.
//  Fails with a screenshot after three minutes.
void confirm_battle_overworld(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  A story battle that may go badly: the best move each turn, Potions
//  under 35 %, the next Pokemon when one faints, and "Keep old moves"
//  on level-up prompts. `doubles` confirms the default target each
//  turn (tag and twin battles). Returns once the battle has ended,
//  won or lost; after a loss we stand in the last Pokemon Center.
void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, bool doubles = false);

//  Any trainer battle, whatever the starter: the best move on the menu each
//  turn until the battle ends, then the text after it.
void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label);

//  A wild battle at its battle menu, fought with the best move until it ends.
void fight_wild_battle(VideoStream& stream, ProControllerContext& context, const std::string& label);



}
}
}
#endif

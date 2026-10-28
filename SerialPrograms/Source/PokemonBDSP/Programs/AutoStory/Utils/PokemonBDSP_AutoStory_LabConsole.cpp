/*  BDSP AutoStory - Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch/Programs/LabConsole/NintendoSwitch_LabConsole.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP_AutoStory_LabConsole.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


void register_bdsp_lab_console(){
    static const bool registered = []{
        register_lab_console_detector("bdsp_dialog",      [](ConsoleHandle&){ return std::make_unique<ShortDialogDetector>(); });
        register_lab_console_detector("bdsp_battle_text", [](ConsoleHandle&){ return std::make_unique<BattleDialogDetector>(); });
        register_lab_console_detector("bdsp_menu",        [](ConsoleHandle&){ return std::make_unique<MenuDetector>(); });
        register_lab_console_detector("bdsp_battle_menu", [](ConsoleHandle&){ return std::make_unique<BattleMenuDetector>(BattleType::STANDARD); });
        register_lab_console_detector("bdsp_overworld",   [](ConsoleHandle&){ return std::make_unique<OverworldDetector>(); });
        return true;
    }();
    (void)registered;
}


}
}
}

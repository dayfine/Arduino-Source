/*  Command File Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_ExperienceGainDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MapDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP_CommandFileCommands.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  "bdsp_load_save": the lab's sequence from the Home menu to the overworld.
//  The waits are what the lab console needs; a shorter one presses A into a
//  loading screen and the next press lands on the wrong menu.
constexpr Milliseconds TAP = 160ms;
constexpr Milliseconds LAUNCH_WAIT = 35s;       //  Tile to the title screen.
constexpr Milliseconds TITLE_WAIT = 3s;         //  Title screen to the main menu.
constexpr Milliseconds CONTINUE_HOLD = 6s;      //  Through "Continue" and its confirmation.
constexpr Milliseconds LOAD_WAIT = 8s;          //  Loading the save into the overworld.

//  Closes the game, launches the tile HOME has highlighted and loads its save.
//  HOME highlights the last game played, so the operator checks that tile with
//  "shot" and moves to BDSP with "dpad" first.
void load_save(CommandFileSession& session, const std::vector<std::string>&){
    run_command_file_line(session, {"close_game"});
    ProControllerContext& context = session.context;
    pbf_press_button(context, BUTTON_A, TAP, TAP);              //  Launch the highlighted tile.
    pbf_press_button(context, BUTTON_A, TAP, LAUNCH_WAIT);      //  Pick the user.
    pbf_press_button(context, BUTTON_A, TAP, TITLE_WAIT);       //  Leave the title screen.
    pbf_press_button(context, BUTTON_A, TAP, TAP);              //  Continue.
    pbf_press_button(context, BUTTON_A, CONTINUE_HOLD, LOAD_WAIT);
    run_command_file_line(session, {"state"});
}

}


void register_command_file_commands(){
    static const bool registered = []{
        register_command_file_detector("bdsp_dialog",      [](ConsoleHandle&){ return std::make_unique<ShortDialogDetector>(); });
        register_command_file_detector("bdsp_battle_text", [](ConsoleHandle&){ return std::make_unique<BattleDialogDetector>(); });
        register_command_file_detector("bdsp_battle_menu", [](ConsoleHandle&){ return std::make_unique<BattleMenuDetector>(BattleType::STANDARD); });
        register_command_file_detector("bdsp_exp_gain",    [](ConsoleHandle&){ return std::make_unique<ExperienceGainDetector>(); });
        register_command_file_detector("bdsp_menu",        [](ConsoleHandle&){ return std::make_unique<MenuDetector>(); });
        register_command_file_detector("bdsp_map",         [](ConsoleHandle&){ return std::make_unique<MapDetector>(); });
        register_command_file_command(
            "bdsp_load_save",
            "bdsp_load_save   (close the game, launch the tile HOME highlights, load its save, print state)",
            load_save
        );
        return true;
    }();
    (void)registered;
}


}
}
}

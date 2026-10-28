/*  BDSP AutoStory - Lab Console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Programs/LabConsole/NintendoSwitch_LabConsole.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_NameListDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_LabConsole.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


void register_bdsp_lab_console(){
    static const bool registered = []{
        register_lab_console_detector("bdsp_dialog",      [](ConsoleHandle&){ return std::make_unique<ShortDialogDetector>(); });
        register_lab_console_detector("bdsp_battle_text", [](ConsoleHandle&){ return std::make_unique<BattleDialogDetector>(); });
        register_lab_console_detector("bdsp_menu",        [](ConsoleHandle&){ return std::make_unique<MenuDetector>(); });
        register_lab_console_detector("bdsp_battle_menu", [](ConsoleHandle&){ return std::make_unique<BattleMenuDetector>(BattleType::STANDARD); });
        register_lab_console_detector("bdsp_overworld",   [](ConsoleHandle&){ return std::make_unique<OverworldDetector>(); });
        register_lab_console_detector("bdsp_name_list",   [](ConsoleHandle&){ return std::make_unique<NameListDetector>(); });
        register_lab_console_command(
            "bdsp_load_save", "bdsp_load_save: from anywhere, restart BDSP and load its save into the overworld",
            [](SingleSwitchProgramEnvironment& env, ProControllerContext& context,
               const std::vector<std::string>&, std::ostream& out){
                go_home(env.console, context);
                out << "bdsp_load_save " << (reset_game_from_home(env, env.console, context, true, 1000ms) ? "in game" : "did not reach the game") << "\n";
            }
        );
        register_lab_console_command(
            "intro_screen_tests", "intro_screen_tests folder: classify <folder>/<expected>/*.png as the new-game intro does",
            [](SingleSwitchProgramEnvironment& env, ProControllerContext&,
               const std::vector<std::string>& words, std::ostream& out){
                if (words.size() < 2){
                    throw ParseException("intro_screen_tests: missing folder");
                }
                out << "intro_screen_tests failures " << run_intro_screen_tests(env.logger(), words[1]) << "\n";
            }
        );
        register_lab_console_command(
            "name_list_row", "name_list_row [image]: the row the preset-name list's arrow is on",
            [](SingleSwitchProgramEnvironment& env, ProControllerContext& context,
               const std::vector<std::string>& words, std::ostream& out){
                int row;
                if (words.size() > 1){
                    const std::string& file = words[1];
                    row = NameListDetector().selected_row(ImageRGB32(file[0] == '/'
                        ? file : USER_FILE_PATH() + "LabConsole/outbox/" + file));
                }else{
                    context.wait_for_all_requests();
                    VideoSnapshot screen = env.console.video().snapshot();
                    row = screen ? NameListDetector().selected_row(screen) : -1;
                }
                out << "name_list_row " << row << "\n";
            }
        );
        register_lab_console_command(
            "bdsp_walk", "bdsp_walk x y ms: walk_until() with the left stick at (x, y) for at most ms",
            [](SingleSwitchProgramEnvironment& env, ProControllerContext& context,
               const std::vector<std::string>& words, std::ostream& out){
                if (words.size() < 4){
                    throw ParseException("bdsp_walk: need x y ms");
                }
                const WalkResult result = walk_until(env.console, context,
                    std::stod(words[1]), std::stod(words[2]), Milliseconds(std::stoll(words[3])), "bdsp_walk");
                out << "bdsp_walk " << walk_stop_name(result.stop) << " " << result.walked.count()
                    << "ms " << result.wild_battles << " wild\n";
            }
        );
        return true;
    }();
    (void)registered;
}


}
}
}

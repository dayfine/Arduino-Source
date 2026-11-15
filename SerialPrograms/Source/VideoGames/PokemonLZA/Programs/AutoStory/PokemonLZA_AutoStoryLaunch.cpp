/*  PokemonLZA AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Language.h"
#include "Pokemon/AutoStory/Pokemon_StorySwitchPrompts.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "PokemonLZA_AutoStoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::StoryLaunchPolicy;
using Pokemon::StoryLaunchPress;
using Pokemon::StoryLaunchPrompt;


namespace{

//  The name HOME shows above the game's tile, in the console's language.
//  A launch that took any tile opened FireRed once
//  it had been played last and moved to the first tile.
constexpr std::string_view HOME_TILE_NAME = "Pokémon Legends: Z-A";
constexpr Language HOME_LANGUAGE = Language::English;

//  Where the title's "use the backup save?" arrow shows (reset_game_from_home()).
const ImageFloatBox BACKUP_ARROW_BOX{0.2, 0.5, 0.6, 0.3};

//  The game's own prompts on the way, read only after the launch: the
//  backup save's arrow (A) and a dialog box (B).
std::vector<StoryLaunchPrompt> plza_prompts(){
    auto arrow = std::make_shared<SelectionArrowDetector>(COLOR_BLUE, nullptr, SelectionArrowType::RIGHT, BACKUP_ARROW_BOX);
    auto dialog = std::make_shared<BlueDialogDetector>(COLOR_YELLOW);
    return {
        {.name = "backup save arrow",
         .matches = [arrow](const ScreenObservation& look){
            return look.frame && look.frame->frame && arrow->detect(*look.frame->frame);
         },
         .press = {.button = BUTTON_A, .hold = 160ms, .release = 240ms}, .after_launch = true},
        {.name = "dialog",
         .matches = [dialog](const ScreenObservation& look){
            return look.frame && look.frame->frame && dialog->detect(*look.frame->frame);
         },
         .press = {.button = BUTTON_B, .hold = 160ms, .release = 240ms}, .after_launch = true},
    };
}

} // namespace


//  No INTERACT_PROMPT veto: the title's "PRESS (A) BUTTON" reads as one,
//  and the overworld's prompts show with the party bar, which vetoes.
bool plza_title(const ScreenObservation& look){
    if (!look.frame || !look.frame->frame){
        return false;
    }
    for (ScreenEvent event : {
        ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE, ScreenEvent::PLAYER_CAN_MOVE,
        ScreenEvent::BATTLE_STARTED,
    }){
        if (look.events.has(event)){
            return false;
        }
    }
    return true;
}

StoryLaunchPolicy plza_launch_policy(ConsoleHandle& console){
    StoryLaunchPolicy policy;
    policy.game = "plza";
    policy.tile_selected = Pokemon::switch_home_tile_named(HOME_LANGUAGE, std::string(HOME_TILE_NAME));
    policy.title = plza_title;
    policy.title_after_launch = true;
    policy.title_press = {.button = BUTTON_A, .hold = 160ms, .release = 1000ms};
    //  Up, B and X held together: Up first, then B + X, only shows the
    //  title again and its A loads the latest save.
    //  Held together they open the BACKUP card, whose "start from the
    //  backup" the title press then takes.
    policy.backup_save = {
        StoryLaunchPress{.button = BUTTON_UP | BUTTON_B | BUTTON_X, .hold = 1500ms, .release = 1500ms},
    };
    //  The backup save's buttons only on the title's "PRESS (A) BUTTON",
    //  which reads as an A prompt: the boot's logos pass plza_title() too.
    policy.backup_title = [](const ScreenObservation& look){
        return look.events.has(ScreenEvent::INTERACT_PROMPT) && plza_title(look);
    };
    policy.prompts = Pokemon::switch_launch_prompts(console);
    for (StoryLaunchPrompt& prompt : plza_prompts()){
        policy.prompts.push_back(std::move(prompt));
    }
    //  The title takes A through the boot's logos too, a second apart.
    policy.presses_per_screen = 12;
    //  After the title's A, the load's black, then
    //  a tips screen (a blue band, a spinner) that plza_title() passes for
    //  10 s and more. 30 looks of 500 ms or more: longer than the boot's
    //  12 s black, which an A on a logo is followed by too.
    policy.title_load_looks = 30;
    return policy;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  Game Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/AutoStory/PokemonSV_AutoStoryTools.h"
#include "VideoGames/PokemonSV/Programs/AutoStory/PokemonSV_MenuOption.h"
#include "VideoGames/PokemonSV/Programs/AutoStory/PokemonSV_MenuOptionDatabase.h"
#include "PokemonSV_GameOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


void set_game_options_for_programs(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    Language language
){
    stream.log("Setting the game options the program needs.");
    enter_menu_from_overworld(info, stream, context, 4, MenuSide::RIGHT, true);
    MenuOption(stream, context, language).set_options({
        {MenuOptionItemEnum::TEXT_SPEED, {MenuOptionToggleEnum::FAST}},
        {MenuOptionItemEnum::SKIP_MOVE_LEARNING, {MenuOptionToggleEnum::ON}},
        {MenuOptionItemEnum::SEND_TO_BOXES, {MenuOptionToggleEnum::AUTOMATIC, MenuOptionToggleEnum::ON}},
        {MenuOptionItemEnum::GIVE_NICKNAMES, {MenuOptionToggleEnum::OFF}},
        {MenuOptionItemEnum::VERTICAL_CAMERA_CONTROLS, {MenuOptionToggleEnum::REGULAR, MenuOptionToggleEnum::NORMAL}},
        {MenuOptionItemEnum::HORIZONTAL_CAMERA_CONTROLS, {MenuOptionToggleEnum::REGULAR, MenuOptionToggleEnum::NORMAL}},
        {MenuOptionItemEnum::CAMERA_SUPPORT, {MenuOptionToggleEnum::OFF}},
        {MenuOptionItemEnum::CAMERA_INTERPOLATION, {MenuOptionToggleEnum::NORMAL, MenuOptionToggleEnum::AVERAGE}},
        {MenuOptionItemEnum::AUTOSAVE, {MenuOptionToggleEnum::OFF}},
    });
    pbf_mash_button(context, BUTTON_A, 500ms);
    clear_dialog(stream, context, ClearDialogMode::STOP_TIMEOUT, 5, {CallbackEnum::PROMPT_DIALOG});
    press_Bs_to_back_to_overworld(info, stream, context);
}


}
}
}

/*  PokemonLZA AutoStory - Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/EnumStringMap.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::string_view action_name(PlzaAction action){
    static const EnumStringMap<PlzaAction> names{
        {PlzaAction::WAIT_FOR_PLATFORM, "wait_for_platform"},
        {PlzaAction::WALK, "walk"},
        {PlzaAction::CHOOSE_1, "choose_1"},
        {PlzaAction::SCENE, "scene"},
        {PlzaAction::PLUS_SKIP, "plus_skip"},
        {PlzaAction::SET_TEXT_SPEED_FAST, "set_text_speed_fast"},
    };
    return names.get_string(action);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BDSP AutoStory - Badge Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Opens the Trainer Card and checks a badge is on it. The shared verdict is
 *  Pokemon_BadgeCheck.h's.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BadgeCheck_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BadgeCheck_H

#include <cstddef>
#include <string_view>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  When `level` asks for it, opens the Trainer Card from the overworld, saves
//  its screenshot, reads badge `index` (menu with `menu_icons` icons, see
//  open_menu()) and backs out to the overworld. Fails
//  the step with a screenshot when the badge is missing; a card the reader
//  cannot read yet is logged and passes.
void verify_badge(
    VideoStream& stream, ProControllerContext& context,
    Pokemon::StoryCheckLevel level, size_t index, std::string_view badge, int menu_icons
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

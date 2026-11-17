/*  AutoStory Battle Text
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle text box read as letters, for the battle policy's KO risk
 *  (PolicyView::battle_texts): a foe charging Solar Beam or Dive, Sunny
 *  Day's sun.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleText_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleText_H

#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The battle text box's two lines on `screen` as letters, lower case
//  ("foevenusaurtookinsunlight"). English only: none in another language
//  or with no text.
std::optional<std::string> read_battle_text(Language language, const ImageViewRGB32& screen);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

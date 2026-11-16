/*  AutoStory Repel Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The use_repel step: the strongest Repel the bag holds, for the long
 *  walks of Route 23 and Victory Road, where the bag may hold any of the
 *  three. Defined with the other bag uses in
 *  PokemonFRLG_AutoStoryItemUse.cpp.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRepelStep_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRepelStep_H

#include <optional>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryBag.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that uses the strongest Repel the bag holds
//  (use_strongest_repel()). Args as a use_item step's "safari" and
//  "if_any".
inline const std::string USE_REPEL_VARIANT = "use_repel";

//  Uses the strongest Repel the bag holds: a Max Repel, else a Super
//  Repel, else a Repel (a use_repel step). NONE_LEFT if it holds none.
ItemUseEnd use_strongest_repel(Logger& logger, ItemUseActions& actions);

//  The Repel that `step`, done, put on, for renew_repel() to renew when
//  it wears off: a use_item step's Repel, or for a use_repel step the Max
//  Repel, so that its renewals go strongest first whichever it used.
//  Nothing for another step.
std::optional<std::string> repel_put_on_by(const Pokemon::StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

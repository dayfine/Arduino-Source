/*  AutoStory Quiet Overworld
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H
#define PokemonAutomation_PokemonFRLG_AutoStoryQuietOverworld_H

#include <functional>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


bool quiet_overworld(const Pokemon::ScreenSettle& settle, Milliseconds quiet);

Pokemon::ScreenSettle wait_for_quiet_overworld(
    const Pokemon::ScreenLookFunction& look, Milliseconds quiet, Milliseconds timeout
);

std::string clear_box_before_a(
    const StoryStep& step,
    const Pokemon::ScreenLookFunction& look, const std::function<void()>& clear_box
);

FrlgStepAction clearing_box_first(
    FrlgStepAction act, const Pokemon::StoryInterrupts& interrupts, Logger& logger
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

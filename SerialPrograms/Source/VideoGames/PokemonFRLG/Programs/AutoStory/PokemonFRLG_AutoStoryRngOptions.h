/*  AutoStory RNG Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The RNG plan options: what AutoStory's Starter RNG stop searches targets
 *  with (PokemonFRLG_AutoStoryRngPlan.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngOptions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngOptions_H

#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Language.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"
#include "PokemonFRLG_AutoStoryRngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class StarterRngPlanOptions : public GroupOption{
public:
    StarterRngPlanOptions();

    //  The attempts these options ask for, for a FireRed game in "language".
    //  Throws UserSetupError for an option it can't read, or when no target
    //  passes.
    std::vector<RngAttempt> plan(Logger& logger, Language language) const;
    //  The sound setting as Starter RNG's dropdown shows it.
    std::string sound_name() const;

public:
    EnumDropdownOption<SoundSetting> SOUND;
    SimpleIntegerOption<uint16_t> TID;
    StringOption SID;
    SimpleIntegerOption<uint32_t> SID_HELPER_ADVANCES;
    SimpleIntegerOption<uint8_t> SID_HELPER_CANDIDATES;
    BooleanCheckBoxOption SHINY_REQUIRED;
    StringOption NATURES;
    StringOption MIN_IVS;
    SimpleIntegerOption<uint8_t> MAX_ATTEMPTS;

private:
    std::vector<SidCandidate> candidates(Logger& logger) const;
};



}
}
}
#endif

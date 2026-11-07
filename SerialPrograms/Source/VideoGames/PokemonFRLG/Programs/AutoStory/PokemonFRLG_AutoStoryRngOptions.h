/*  AutoStory RNG Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The RNG plan options: what AutoStory's Starter RNG stop searches targets
 *  with (RngManipulation/PokemonFRLG_RngPlan.h).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngOptions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngOptions_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/FloatingPointOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "Common/Cpp/Options/StringOption.h"
#include "CommonFramework/Language.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngCalibration.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  This console's RNG calibration, as Starter RNG's RNG Calibration shows
//  it, for AutoStory's RNG stops to start from: the seed and Continue
//  Screen Frames offsets, the same for every stop, and an in-game advances
//  offset for each stop.
class RngStopCalibrationOptions : public GroupOption{
public:
    RngStopCalibrationOptions();

    //  The calibration in frames for the stop whose in-game advances offset
    //  `ingame` keeps.
    RngCalibrations calibration(const FloatingPointOption& ingame) const;
    //  Keeps `calibration` as calibration() returns it. Called after each
    //  reset, so a stopped run keeps what it learned.
    void save(const RngCalibrations& calibration, FloatingPointOption& ingame);

public:
    SimpleIntegerOption<int64_t> SEED;
    FloatingPointOption CSF;
    //  Starter RNG's in-game advances offset.
    FloatingPointOption STARTER_INGAME;
};


//  User options for Starter RNG plan execution.
class StarterRngPlanOptions : public GroupOption{
public:
    StarterRngPlanOptions();

    //  Throws UserSetupError for a TID, SID, natures or IVs it can't read,
    //  before the run presses anything.
    void check(Logger& logger) const;
    //  The TID typed in, or nothing to read it from the Trainer Card.
    std::optional<uint16_t> tid_override(Logger& logger) const;

    //  The attempts these options ask for, for TID "tid" in a FireRed game
    //  in "language", logged as "RngPlan:" lines. Throws UserSetupError
    //  when no target passes.
    std::vector<RngAttempt> plan(Logger& logger, Language language, uint16_t tid) const;
    //  SID Helper's candidates for "tid", from the two SID Helper options.
    std::vector<SidCandidate> helper_candidates(uint16_t tid) const;
    //  The SIDs plan() plans for: the SID if set, else helper_candidates().
    std::vector<SidCandidate> plan_candidates(Logger& logger, uint16_t tid) const;
    //  The sound setting as Starter RNG's dropdown shows it.
    std::string sound_name() const;

public:
    EnumDropdownOption<SoundSetting> SOUND;
    StringOption TID;
    StringOption SID;
    SimpleIntegerOption<uint32_t> SID_HELPER_ADVANCES;
    SimpleIntegerOption<uint8_t> SID_HELPER_CANDIDATES;
    BooleanCheckBoxOption SHINY_REQUIRED;
    StringOption NATURES;
    StringOption MIN_IVS;
    SimpleIntegerOption<uint8_t> MAX_ATTEMPTS;
    RngStopCalibrationOptions CALIBRATION;

private:
    RngTargetFilters filters(Logger& logger) const;
    std::optional<uint16_t> sid(Logger& logger) const;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

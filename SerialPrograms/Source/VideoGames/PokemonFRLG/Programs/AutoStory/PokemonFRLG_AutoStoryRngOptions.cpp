/*  AutoStory RNG Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.h"
#include "PokemonFRLG_AutoStoryRngOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


RngStopCalibrationOptions::RngStopCalibrationOptions()
    : GroupOption("RNG Calibration (this console)", LockMode::LOCK_WHILE_RUNNING)
    , SEED(
        "<b>Seed Calibration (ms):</b><br>As in Starter RNG's RNG Calibration. Each RNG stop starts from it, "
        "and saves what it learns after every reset.",
        LockMode::LOCK_WHILE_RUNNING, 0
    )
    , CSF(
        "<b>Continue Screen Frames Calibration:</b><br>As in Starter RNG's RNG Calibration, kept as the seed's is.",
        LockMode::LOCK_WHILE_RUNNING, 0.0
    )
    , STARTER_INGAME(
        "<b>Starter RNG's In-Game Advances Calibration:</b><br>As in Starter RNG's RNG Calibration, "
        "for Squirtle only: each RNG stop keeps its own.",
        LockMode::LOCK_WHILE_RUNNING, 0.0
    )
{
    PA_ADD_OPTION(SEED);
    PA_ADD_OPTION(CSF);
    PA_ADD_OPTION(STARTER_INGAME);
}

RngCalibrations RngStopCalibrationOptions::calibration(const FloatingPointOption& ingame) const{
    return RngCalibrations{
        .seed_offset = double(int64_t(SEED)) / FRLG_FRAME_DURATION,
        .csf_offset = CSF,
        .ingame_offset = ingame,
    };
}

void RngStopCalibrationOptions::save(const RngCalibrations& calibration, FloatingPointOption& ingame){
    //  In ms, as RngCalibrationDisplay::set_calibrations() shows it.
    SEED.set(int64_t(std::round(calibration.seed_offset * FRLG_FRAME_DURATION)));
    CSF.set(calibration.csf_offset);
    ingame.set(calibration.ingame_offset);
}


StarterRngPlanOptions::StarterRngPlanOptions()
    : GroupOption("RNG Plan (Stop for Starter RNG)", LockMode::LOCK_WHILE_RUNNING)
    , SOUND(
        "<b>Sound:</b><br>The in-game sound setting, as in Starter RNG. It picks the seeds.",
        {{SoundSetting::Mono, "mono", "Mono"}, {SoundSetting::Stereo, "stereo", "Stereo"}},
        LockMode::LOCK_WHILE_RUNNING, SoundSetting::Mono
    )
    , TID(
        false, "<b>TID:</b><br>Empty: read from the Trainer Card at the stop. Set it only if that read fails.",
        LockMode::LOCK_WHILE_RUNNING, "", ""
    )
    , SID(
        false,
        "<b>SID:</b><br>The SID, once known. Empty: plan for the SIDs SID Helper lists, from the two options below. "
        "Run Starter RNG in AutoStory fills it in when a shiny Squirtle confirms the SID.",
        LockMode::LOCK_WHILE_RUNNING, "", ""
    )
    , SID_HELPER_ADVANCES(
        "<b>SID Helper's Target Advances:</b><br>As set in SID Helper.",
        LockMode::LOCK_WHILE_RUNNING, 3001, 2275
    )
    , SID_HELPER_CANDIDATES(
        "<b>SID Helper's # Candidate SIDs:</b><br>As set in SID Helper. The middle one counts as the likeliest.",
        LockMode::LOCK_WHILE_RUNNING, 5, 1, 11
    )
    , SHINY_REQUIRED(
        "<b>Shiny required:</b><br>Only targets shiny for a SID candidate, tried until one comes out shiny. "
        "Off: the quickest target that passes the natures and IVs.",
        LockMode::LOCK_WHILE_RUNNING, true
    )
    , NATURES(false, "<b>Natures:</b><br>Separated by commas. Empty: any nature.", LockMode::LOCK_WHILE_RUNNING, "", "Modest, Timid")
    , MIN_IVS(
        false, "<b>Minimum IVs:</b><br>HP/Atk/Def/SpA/SpD/Spe.",
        LockMode::LOCK_WHILE_RUNNING, "0/0/0/0/0/0", "0/0/0/0/0/0"
    )
    , MAX_ATTEMPTS("<b>Max attempts:</b><br>The most targets to list.", LockMode::LOCK_WHILE_RUNNING, 5, 1, 20)
{
    PA_ADD_OPTION(SOUND);
    PA_ADD_OPTION(TID);
    PA_ADD_OPTION(SID);
    PA_ADD_OPTION(SID_HELPER_ADVANCES);
    PA_ADD_OPTION(SID_HELPER_CANDIDATES);
    PA_ADD_OPTION(SHINY_REQUIRED);
    PA_ADD_OPTION(NATURES);
    PA_ADD_OPTION(MIN_IVS);
    PA_ADD_OPTION(MAX_ATTEMPTS);
    PA_ADD_OPTION(CALIBRATION);
}

std::string StarterRngPlanOptions::sound_name() const{
    return SOUND == SoundSetting::Stereo ? "Stereo" : "Mono";
}

std::optional<uint16_t> StarterRngPlanOptions::tid_override(Logger& logger) const{
    const std::string text = TID;
    if (text.empty()){
        return std::nullopt;
    }
    const std::optional<uint16_t> tid = parse_trainer_id(text);
    if (!tid){
        throw UserSetupError(logger, "RNG plan: the TID must be a number from 0 to 65535, or empty.");
    }
    return tid;
}

std::optional<uint16_t> StarterRngPlanOptions::sid(Logger& logger) const{
    const std::string text = SID;
    if (text.empty()){
        return std::nullopt;
    }
    const std::optional<uint16_t> sid = parse_trainer_id(text);
    if (!sid){
        throw UserSetupError(logger, "RNG plan: the SID must be a number from 0 to 65535, or empty.");
    }
    return sid;
}

RngTargetFilters StarterRngPlanOptions::filters(Logger& logger) const{
    const std::optional<std::vector<Pokemon::AdvNature>> natures = parse_natures(NATURES);
    if (!natures){
        throw UserSetupError(logger, "RNG plan: natures must be nature names separated by commas, e.g. \"Modest, Timid\".");
    }
    const std::optional<Pokemon::AdvIVs> min_ivs = parse_ivs(MIN_IVS);
    if (!min_ivs){
        throw UserSetupError(logger, "RNG plan: minimum IVs must be six numbers from 0 to 31, e.g. \"0/0/0/31/0/31\".");
    }
    RngTargetFilters filters;
    filters.natures = *natures;
    filters.min_ivs = *min_ivs;
    filters.shiny_required = SHINY_REQUIRED;
    return filters;
}

void StarterRngPlanOptions::check(Logger& logger) const{
    tid_override(logger);
    sid(logger);
    filters(logger);
}

std::vector<SidCandidate> StarterRngPlanOptions::helper_candidates(uint16_t tid) const{
    return sid_helper_candidates(tid, SID_HELPER_ADVANCES, SID_HELPER_CANDIDATES);
}

std::vector<SidCandidate> StarterRngPlanOptions::plan_candidates(Logger& logger, uint16_t tid) const{
    const std::optional<uint16_t> known = sid(logger);
    return known ? std::vector<SidCandidate>{{*known, 1}} : helper_candidates(tid);
}

std::vector<RngAttempt> StarterRngPlanOptions::plan(Logger& logger, Language language, uint16_t tid) const{
    //  SID Helper's candidates are logged even with the SID known, to place
    //  it among them (rng_log_confirmed()).
    const std::vector<SidCandidate> helper = helper_candidates(tid);
    logger.log(rng_log_sid_helper(tid, SID_HELPER_ADVANCES, helper));
    const std::optional<uint16_t> known = sid(logger);
    if (known){
        logger.log(rng_log_confirmed(tid, *known, helper));
    }

    const std::vector<RngAttempt> attempts = plan_starter_rng(StarterRngPlanInputs{
        //  TODO: LeafGreen, once AutoStory plays it.
        .firered = true,
        .language = language,
        .sound = SOUND,
        .tid = tid,
        .candidates = plan_candidates(logger, tid),
        .filters = filters(logger),
        .max_attempts = MAX_ATTEMPTS,
    });
    if (attempts.empty()){
        throw UserSetupError(logger,
            "RNG plan: no Starter RNG target passes the natures, minimum IVs and shiny options for TID " + std::to_string(tid) + "."
        );
    }
    for (size_t c = 0; c < attempts.size(); c++){
        logger.log(rng_log_attempt(c, attempts[c]));
    }
    return attempts;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

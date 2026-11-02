/*  AutoStory RNG Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryRngOptions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StarterRngPlanOptions::StarterRngPlanOptions()
    : GroupOption("RNG Plan (Stop for Starter RNG)", LockMode::LOCK_WHILE_RUNNING)
    , SOUND(
        "<b>Sound:</b><br>The in-game sound setting, as in Starter RNG. It picks the seeds.",
        {{SoundSetting::Mono, "mono", "Mono"}, {SoundSetting::Stereo, "stereo", "Stereo"}},
        LockMode::LOCK_WHILE_RUNNING, SoundSetting::Mono
    )
    , TID("<b>TID:</b><br>The Trainer ID SID Helper read.", LockMode::LOCK_WHILE_RUNNING, 0)
    , SID(
        false,
        "<b>SID:</b><br>The SID, once known. Empty: plan for the SIDs SID Helper lists, from the two options below.",
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
    , NATURES(
        false, "<b>Natures:</b><br>Separated by commas. Empty: any nature.",
        LockMode::LOCK_WHILE_RUNNING, "", "Modest, Timid"
    )
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
}

std::string StarterRngPlanOptions::sound_name() const{
    return SOUND == SoundSetting::Stereo ? "Stereo" : "Mono";
}

std::vector<SidCandidate> StarterRngPlanOptions::candidates(Logger& logger) const{
    const std::string sid_text = SID;
    if (sid_text.empty()){
        return sid_helper_candidates(TID, SID_HELPER_ADVANCES, SID_HELPER_CANDIDATES);
    }
    const std::optional<uint16_t> sid = parse_sid(sid_text);
    if (!sid){
        throw UserSetupError(logger, "RNG plan: the SID must be a number from 0 to 65535, or empty.");
    }
    return {{*sid, 1}};
}

std::vector<RngAttempt> StarterRngPlanOptions::plan(Logger& logger, Language language) const{
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

    const std::vector<RngAttempt> attempts = plan_starter_rng(StarterRngPlanInputs{
        //  AutoStory plays FireRed only.
        .firered = true,
        .language = language,
        .sound = SOUND,
        .tid = TID,
        .candidates = candidates(logger),
        .filters = filters,
        .max_attempts = MAX_ATTEMPTS,
    });
    if (attempts.empty()){
        throw UserSetupError(logger,
            "RNG plan: no Starter RNG target passes the natures, minimum IVs and shiny options for TID " + std::to_string(TID) + "."
        );
    }
    return attempts;
}



}
}
}

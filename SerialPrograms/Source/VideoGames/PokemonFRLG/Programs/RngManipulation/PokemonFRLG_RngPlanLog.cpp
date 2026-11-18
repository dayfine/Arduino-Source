/*  RNG Plan Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanInputs.h"
#include "PokemonFRLG_RngPlanLog.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


namespace{

std::string join_sids(const std::vector<uint16_t>& sids){
    std::string text;
    for (uint16_t sid : sids){
        text += (text.empty() ? "" : ",") + std::to_string(sid);
    }
    return text;
}

} // namespace

std::string rng_log_tid(uint16_t tid, bool from_card){
    return std::format("RngPlan: tid={} source={}", tid, from_card ? "card" : "option");
}

std::string rng_log_sid_helper(uint16_t tid, uint32_t target_advances, const std::vector<SidCandidate>& candidates){
    std::vector<uint16_t> sids;
    for (const SidCandidate& candidate : candidates){
        sids.push_back(candidate.sid);
    }
    return std::format("RngPlan: sid_helper tid={} target_advances={} candidates={}", tid, target_advances, join_sids(sids));
}

std::string rng_log_attempt(size_t index, const RngAttempt& attempt){
    return std::format(
        "RngPlan: attempt={} seed={:04X} advances={} sids={} seed_wait_ms={} ingame_ms={}",
        index + 1, attempt.target.seed.seed, attempt.target.advances, join_sids(attempt.sids),
        starter_rng_seed_wait_ms(attempt.target.seed), starter_rng_ingame_ms(attempt.target.advances)
    );
}

namespace{

std::string_view outcome_name(RngAttemptOutcome outcome){
    switch (outcome){
    case RngAttemptOutcome::SHINY:
        return "shiny";
    case RngAttemptOutcome::SHINY_OFF_TARGET:
        return "shiny_off_target";
    case RngAttemptOutcome::HIT_NOT_SHINY:
        return "hit_not_shiny";
    case RngAttemptOutcome::MISSED:
        return "missed";
    case RngAttemptOutcome::UNUSABLE:
        return "unusable";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RngAttemptOutcome.");
}

std::string_view next_name(RngPlanNext next){
    switch (next){
    case RngPlanNext::RETRY:
        return "retry";
    case RngPlanNext::NEXT_ATTEMPT:
        return "next_attempt";
    case RngPlanNext::CONFIRM:
        return "confirm";
    case RngPlanNext::STOP:
        return "stop";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RngPlanNext.");
}

} // namespace

std::string rng_log_result(const RngResultLog& result){
    const std::optional<AdvPokemonResult>& pokemon = result.pokemon;
    return std::format(
        "RngPlan: result attempt={} try={} outcome={} starter_rng={} nature={} ivs={} pid={} next={}",
        result.index + 1, result.try_number, outcome_name(result.outcome),
        result.detail.empty() ? "-" : result.detail,
        pokemon ? nature_to_string(pokemon->nature) : "-",
        pokemon ? ivs_to_string(pokemon->ivs) : "-",
        pokemon ? std::format("{:08X}", pokemon->pid) : "-",
        next_name(result.next)
    );
}

std::string rng_log_confirmed(uint16_t tid, uint16_t sid, const std::vector<SidCandidate>& sid_helper_candidates){
    size_t k = 0;
    for (size_t c = 0; c < sid_helper_candidates.size(); c++){
        if (sid_helper_candidates[c].sid == sid){
            k = c + 1;
            break;
        }
    }
    return std::format("RngPlan: confirmed tid={} sid={} candidate={} of={}", tid, sid, k, sid_helper_candidates.size());
}


std::vector<std::string> rng_attempt_lines(const std::vector<RngAttempt>& attempts){
    std::vector<std::string> lines;
    for (size_t c = 0; c < attempts.size(); c++){
        const RngTarget& target = attempts[c].target;
        std::string shiny;
        if (attempts[c].sids.empty()){
            shiny = "not shiny for any SID candidate";
        }else{
            shiny = "shiny for SID";
            for (size_t s = 0; s < attempts[c].sids.size(); s++){
                shiny += (s == 0 ? " " : ", ") + std::to_string(attempts[c].sids[s]);
            }
            shiny += std::format(" ({:.0f}% of the candidates left)", target.shiny_probability * 100);
        }
        lines.push_back(std::format(
            "Attempt {}: Target Seed {:04X}, Advances {}: {}, IVs {}, PID {:08X}, {}.",
            c + 1, target.seed.seed, target.advances,
            nature_to_string(target.pokemon.nature), ivs_to_string(target.pokemon.ivs), target.pokemon.pid,
            shiny
        ));
    }
    return lines;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

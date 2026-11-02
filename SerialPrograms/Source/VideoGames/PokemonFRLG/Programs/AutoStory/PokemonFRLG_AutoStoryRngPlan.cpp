/*  AutoStory RNG Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryRngPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;


std::vector<SidCandidate> sid_helper_candidates(uint16_t tid, uint32_t target_advances, uint8_t count){
    //  As SidHelper's get_sid_messages(): from "count / 2" steps before the target.
    const int start = -int(count) / 2;
    const int end = start + int(count);
    const int farthest = std::max(-start, end - 1);

    std::vector<SidCandidate> candidates;
    AdvRngSearcher searcher(tid, uint64_t(int64_t(target_advances) + 2 * start));
    for (int i = start; i < end; i++){
        candidates.push_back({uint16_t(searcher.state.s0 >> 16), double(farthest + 1 - std::abs(i))});
        searcher.advance_state();
        searcher.advance_state();
    }
    return candidates;
}


namespace{

std::string trim(const std::string& text){
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos){
        return "";
    }
    const size_t last = text.find_last_not_of(' ');
    return text.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& text, char separator){
    std::vector<std::string> parts;
    size_t start = 0;
    while (true){
        const size_t next = text.find(separator, start);
        parts.push_back(trim(text.substr(start, next == std::string::npos ? std::string::npos : next - start)));
        if (next == std::string::npos){
            return parts;
        }
        start = next + 1;
    }
}

}


std::optional<uint16_t> parse_sid(const std::string& text){
    const std::string digits = trim(text);
    if (digits.empty() || digits.size() > 5 || !std::all_of(digits.begin(), digits.end(), [](char ch){ return ch >= '0' && ch <= '9'; })){
        return std::nullopt;
    }
    const int value = std::stoi(digits);
    if (value > 0xffff){
        return std::nullopt;
    }
    return uint16_t(value);
}

std::optional<AdvIVs> parse_ivs(const std::string& text){
    const std::vector<std::string> parts = split(text, '/');
    if (parts.size() != 6){
        return std::nullopt;
    }
    uint8_t values[6];
    for (size_t c = 0; c < 6; c++){
        const std::string& part = parts[c];
        if (part.empty() || part.size() > 2 || !std::all_of(part.begin(), part.end(), [](char ch){ return ch >= '0' && ch <= '9'; })){
            return std::nullopt;
        }
        const int value = std::stoi(part);
        if (value > 31){
            return std::nullopt;
        }
        values[c] = uint8_t(value);
    }
    AdvIVs ivs;
    ivs.hp = values[0];
    ivs.attack = values[1];
    ivs.defense = values[2];
    ivs.spatk = values[3];
    ivs.spdef = values[4];
    ivs.speed = values[5];
    return ivs;
}

std::string ivs_to_string(const AdvIVs& ivs){
    return std::format("{}/{}/{}/{}/{}/{}", ivs.hp, ivs.attack, ivs.defense, ivs.spatk, ivs.spdef, ivs.speed);
}

std::optional<std::vector<AdvNature>> parse_natures(const std::string& text){
    std::vector<AdvNature> natures;
    if (trim(text).empty()){
        return natures;
    }
    for (const std::string& name : split(text, ',')){
        const AdvNature nature = string_to_nature(name);
        if (nature == AdvNature::Any){
            return std::nullopt;
        }
        natures.push_back(nature);
    }
    return natures;
}


namespace{

//  "targets" with only the shiny candidates still "left", and their share of
//  the weight left.
std::vector<RngTarget> rescore(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    const std::vector<bool>& left
){
    double total = 0;
    for (size_t c = 0; c < candidates.size(); c++){
        if (left[c]){
            total += candidates[c].weight;
        }
    }
    std::vector<RngTarget> scored;
    scored.reserve(targets.size());
    for (const RngTarget& target : targets){
        RngTarget copy = target;
        copy.shiny_for.clear();
        double weight = 0;
        for (const ShinyCandidate& shiny : target.shiny_for){
            if (left[shiny.candidate]){
                copy.shiny_for.push_back(shiny);
                weight += candidates[shiny.candidate].weight;
            }
        }
        copy.shiny_probability = total > 0 ? weight / total : 0;
        scored.emplace_back(std::move(copy));
    }
    return scored;
}

RngAttempt make_attempt(const RngTarget& target, const std::vector<SidCandidate>& candidates){
    RngAttempt attempt{target, {}};
    for (const ShinyCandidate& shiny : target.shiny_for){
        attempt.sids.push_back(candidates[shiny.candidate].sid);
    }
    return attempt;
}

}


std::vector<RngAttempt> plan_rng_attempts(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_attempts
){
    std::vector<RngAttempt> attempts;
    if (targets.empty() || max_attempts == 0){
        return attempts;
    }
    std::vector<bool> left(candidates.size(), true);

    if (!shiny_required){
        //  With no shiny chance, rank_rng_targets() sorts by time alone.
        std::vector<RngTarget> by_time = targets;
        for (RngTarget& target : by_time){
            target.shiny_probability = 0;
        }
        rank_rng_targets(by_time);
        const uint32_t quickest = by_time.front().state;
        for (const RngTarget& target : rescore(targets, candidates, left)){
            if (target.state == quickest){
                attempts.push_back(make_attempt(target, candidates));
                break;
            }
        }
        return attempts;
    }

    while (attempts.size() < max_attempts){
        std::vector<RngTarget> scored = rescore(targets, candidates, left);
        rank_rng_targets(scored);
        const RngTarget& best = scored.front();
        if (best.shiny_for.empty()){
            break;
        }
        for (const ShinyCandidate& shiny : best.shiny_for){
            left[shiny.candidate] = false;
        }
        attempts.push_back(make_attempt(best, candidates));
        if (std::find(left.begin(), left.end(), true) == left.end()){
            break;
        }
    }
    return attempts;
}


RngAttemptOutcome classify_attempt(const RngTarget& target, const RngAttemptResult& result){
    if (result.shiny){
        return RngAttemptOutcome::SHINY;
    }
    const AdvIVs& want = target.pokemon.ivs;
    const AdvIVs& got = result.ivs;
    const bool same_ivs = want.hp == got.hp && want.attack == got.attack && want.defense == got.defense
        && want.spatk == got.spatk && want.spdef == got.spdef && want.speed == got.speed;
    return result.nature == target.pokemon.nature && same_ivs
        ? RngAttemptOutcome::HIT_NOT_SHINY
        : RngAttemptOutcome::MISSED;
}


RngPlanTracker::RngPlanTracker(
    std::vector<RngAttempt> attempts,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_retries
)
    : m_attempts(std::move(attempts))
    , m_shiny_required(shiny_required)
    , m_max_retries(max_retries)
    , m_step(m_attempts.empty() ? RngPlanStep::OUT_OF_ATTEMPTS : RngPlanStep::RUN_ATTEMPT)
{
    for (const SidCandidate& candidate : candidates){
        m_remaining.push_back(candidate.sid);
    }
}

std::optional<uint16_t> RngPlanTracker::confirmed_sid() const{
    if (m_step == RngPlanStep::GOT_SHINY && m_remaining.size() == 1){
        return m_remaining.front();
    }
    return std::nullopt;
}

bool RngPlanTracker::is_ruled_out(uint16_t sid) const{
    return std::find(m_remaining.begin(), m_remaining.end(), sid) == m_remaining.end();
}

RngPlanStep RngPlanTracker::record(RngAttemptOutcome outcome){
    if (m_step != RngPlanStep::RUN_ATTEMPT){
        return m_step;
    }
    const std::vector<uint16_t>& sids = m_attempts[m_attempt].sids;
    switch (outcome){
    case RngAttemptOutcome::SHINY:{
        //  The SID is one the target is shiny for. None of them: it wasn't
        //  among the candidates.
        std::vector<uint16_t> narrowed;
        for (uint16_t sid : m_remaining){
            if (std::find(sids.begin(), sids.end(), sid) != sids.end()){
                narrowed.push_back(sid);
            }
        }
        m_remaining = std::move(narrowed);
        m_step = RngPlanStep::GOT_SHINY;
        return m_step;
    }
    case RngAttemptOutcome::HIT_NOT_SHINY:
        if (!m_shiny_required){
            m_step = RngPlanStep::GOT_TARGET;
            return m_step;
        }
        m_remaining.erase(
            std::remove_if(m_remaining.begin(), m_remaining.end(), [&](uint16_t sid){
                return std::find(sids.begin(), sids.end(), sid) != sids.end();
            }),
            m_remaining.end()
        );
        m_retries = 0;
        //  The next attempt still shiny for a candidate left.
        for (size_t next = m_attempt + 1; next < m_attempts.size(); next++){
            const std::vector<uint16_t>& next_sids = m_attempts[next].sids;
            if (!std::all_of(next_sids.begin(), next_sids.end(), [&](uint16_t sid){ return is_ruled_out(sid); })){
                m_attempt = next;
                return m_step;
            }
        }
        m_step = RngPlanStep::OUT_OF_ATTEMPTS;
        return m_step;
    case RngAttemptOutcome::MISSED:
        m_retries++;
        if (m_retries > m_max_retries){
            m_step = RngPlanStep::OUT_OF_RETRIES;
        }
        return m_step;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RngAttemptOutcome.");
}


std::vector<RngAttempt> plan_starter_rng(const StarterRngPlanInputs& inputs){
    const SeedsDatabase database(seeds_json_path(inputs.firered, inputs.language));
    //  Only the English lists hold the blackout button's seeds (initial_seed_entries()).
    const bool english_list = inputs.language != Language::Japanese;
    const std::vector<InitialSeedEntry> seeds = initial_seed_entries(database, inputs.sound, "Help", english_list);

    RngTargetFilters filters = inputs.filters;
    filters.min_advances = std::max(filters.min_advances, STARTER_RNG_MIN_ADVANCES);
    const std::vector<RngTarget> targets = search_rng_targets(seeds, inputs.tid, inputs.candidates, filters);
    return plan_rng_attempts(targets, inputs.candidates, filters.shiny_required, inputs.max_attempts);
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



}
}
}

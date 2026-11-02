/*  AutoStory RNG Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which Starter RNG targets AutoStory asks for, in order, when the SID is
 *  only narrowed down to SID Helper's candidates, and what to do after each
 *  one. The loop is the one PokemonFRLG_RngTargetSearch.h describes:
 *   - Shiny: the SID is one of those the target is shiny for. With one left,
 *     it is confirmed and later targets can be searched with it alone.
 *   - Nature and IVs match, but not shiny: the target was hit, so the SIDs it
 *     is shiny for are wrong. Go on to the next attempt.
 *   - Nature or IVs differ: the target was missed. Run it again.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRngPlan_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRngPlan_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_SeedsDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The SIDs SID Helper lists for "tid": "count" of them, 2 advances apart,
//  centered on "target_advances", as its get_sid_messages() lists them. The
//  middle one, where SID Helper aims, weighs the most, and each step away one
//  less, so the outermost weigh 1.
std::vector<SidCandidate> sid_helper_candidates(uint16_t tid, uint32_t target_advances, uint8_t count);

//  A SID typed as a number from 0 to 65535. Nothing for any other text.
std::optional<uint16_t> parse_sid(const std::string& text);

//  "HP/Atk/Def/SpA/SpD/Spe", as Ten Lines and the RNG programs list IVs.
//  Nothing for text that isn't six numbers from 0 to 31.
std::optional<Pokemon::AdvIVs> parse_ivs(const std::string& text);
std::string ivs_to_string(const Pokemon::AdvIVs& ivs);

//  Natures separated by commas ("Modest, Timid"). Empty text is any nature
//  (no entries). Nothing for a name that isn't a nature.
std::optional<std::vector<Pokemon::AdvNature>> parse_natures(const std::string& text);


struct RngAttempt{
    RngTarget target;
    //  The SIDs the target is shiny for, of those the attempts before it
    //  leave. Empty when the target isn't shiny for any of them.
    std::vector<uint16_t> sids;
};

//  The attempts to make, in order, for SID candidates "candidates" (the ones
//  "targets" were searched with). Each is the best target by
//  rank_rng_targets() over the candidates the attempts before it leave, so a
//  target shiny for several candidates comes before one shiny for one, unless
//  it takes much longer. Stops after "max_attempts", or once no target is
//  shiny for a candidate left.
//
//  With "shiny_required" off, a hit ends the plan whether it is shiny or not,
//  so the plan is the one quickest target.
std::vector<RngAttempt> plan_rng_attempts(
    const std::vector<RngTarget>& targets,
    const std::vector<SidCandidate>& candidates,
    bool shiny_required,
    size_t max_attempts
);


//  What came out of one attempt.
enum class RngAttemptOutcome{
    SHINY,
    //  The target's nature and IVs, not shiny.
    HIT_NOT_SHINY,
    //  Another nature or other IVs: not the target.
    MISSED,
};

struct RngAttemptResult{
    bool shiny;
    Pokemon::AdvNature nature;
    Pokemon::AdvIVs ivs;
};
RngAttemptOutcome classify_attempt(const RngTarget& target, const RngAttemptResult& result);


enum class RngPlanStep{
    //  Run current_attempt(): the first, the next, or the same again after a miss.
    RUN_ATTEMPT,
    //  Shiny. remaining_sids() are the SIDs it can be.
    GOT_SHINY,
    //  Shiny not required, and the target was hit.
    GOT_TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  One attempt was missed more often than the retries allow.
    OUT_OF_RETRIES,
};

//  Follows the plan through the attempts' outcomes.
class RngPlanTracker{
public:
    RngPlanTracker(
        std::vector<RngAttempt> attempts,
        const std::vector<SidCandidate>& candidates,
        bool shiny_required,
        size_t max_retries
    );

    RngPlanStep step() const{ return m_step; }
    size_t attempt_index() const{ return m_attempt; }
    const RngAttempt& current_attempt() const{ return m_attempts[m_attempt]; }
    //  The SID candidates not ruled out, in the order given.
    const std::vector<uint16_t>& remaining_sids() const{ return m_remaining; }
    //  The SID, once a shiny leaves one candidate.
    std::optional<uint16_t> confirmed_sid() const;

    //  The next step after the current attempt came out as "outcome". Once
    //  the plan has ended, the step stays as it is.
    RngPlanStep record(RngAttemptOutcome outcome);

private:
    bool is_ruled_out(uint16_t sid) const;

    std::vector<RngAttempt> m_attempts;
    bool m_shiny_required;
    size_t m_max_retries;
    std::vector<uint16_t> m_remaining;
    size_t m_attempt = 0;
    size_t m_retries = 0;
    RngPlanStep m_step;
};


//  What Starter RNG's targets are searched with.
struct StarterRngPlanInputs{
    bool firered;
    Language language;
    SoundSetting sound;
    uint16_t tid;
    std::vector<SidCandidate> candidates;
    RngTargetFilters filters;
    size_t max_attempts;
};

//  Starter RNG's smallest Advances setting.
inline constexpr uint64_t STARTER_RNG_MIN_ADVANCES = 940;

//  The attempts for Starter RNG, from the seed list of the version and
//  language in Help button mode, the one Starter RNG uses.
std::vector<RngAttempt> plan_starter_rng(const StarterRngPlanInputs& inputs);

//  One line per attempt, for the log and the stop message: the Target Seed
//  and Advances to give Starter RNG, the Pokemon, and the SIDs it is shiny for.
std::vector<std::string> rng_attempt_lines(const std::vector<RngAttempt>& attempts);


void add_tests_AutoStoryRngPlan(UnitTestDatabase& database);



}
}
}
#endif

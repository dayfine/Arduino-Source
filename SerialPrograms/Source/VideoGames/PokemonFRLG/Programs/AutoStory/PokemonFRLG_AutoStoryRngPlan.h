/*  AutoStory RNG Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which Starter RNG targets AutoStory asks for, in order, when the SID is
 *  only narrowed down to SID Helper's candidates, and what to do after each
 *  one. The loop is the one PokemonFRLG_RngTargetSearch.h describes:
 *   - Shiny, and the target was hit: the SID is one of those the target is
 *     shiny for. With one left, it is confirmed and later targets can be
 *     searched with it alone.
 *   - Shiny, but not the target (another advance): it says nothing about
 *     the SID. The plan ends with the shiny.
 *   - Nature and IVs match, but not shiny: the target was hit, so the SIDs it
 *     is shiny for are wrong. Go on to the next attempt.
 *   - Nature or IVs differ: the target was missed. Run it again, up to the
 *     retries, then go on to the next attempt.
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

//  A TID or SID typed as a number from 0 to 65535. Nothing for any other text.
std::optional<uint16_t> parse_trainer_id(const std::string& text);

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
    //  Shiny, with the target's nature and IVs.
    SHINY,
    //  Shiny, but not the target, or not known to be.
    SHINY_OFF_TARGET,
    //  The target's nature and IVs, not shiny.
    HIT_NOT_SHINY,
    //  Another nature or other IVs: not the target.
    MISSED,
    //  The attempt couldn't run (Starter RNG refused its timings). It rules
    //  nothing out; the next attempt runs instead.
    UNUSABLE,
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
    //  Shiny, the target hit. remaining_sids() are the SIDs it can be.
    GOT_SHINY,
    //  Shiny, not the target: the SIDs are as they were, none confirmed.
    GOT_SHINY_OFF_TARGET,
    //  Shiny not required, and the target was hit.
    GOT_TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  The last attempt still shiny for a SID left was missed more often
    //  than the retries allow, or couldn't run. (Missing an earlier one moves
    //  on to the next.)
    MISSED_EVERY_ATTEMPT_LEFT,
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
    size_t attempt_count() const{ return m_attempts.size(); }
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
    //  Moves to the next attempt still shiny for a SID left (with shiny
    //  required; else any next one). False if there is none.
    bool move_to_next_attempt();

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

//  Starter RNG's title screen wait, before calibration: the seed list's
//  delay plus this (FIXED_SEED_OFFSET in StarterRng's attempts).
inline constexpr int64_t STARTER_RNG_FIXED_SEED_OFFSET_MS = -845;
//  The shortest title screen wait the plan uses. Starter RNG refuses waits
//  under 29,650 ms, and its docs (pokemonautomation.github.io,
//  docs/Programs/PokemonFRLG/StarterRng.md, "Avoid low seed delays when
//  possible") warn that waits near 30,500 ms can miss the title screen
//  press. 31,500 ms keeps a second clear of that for the seed calibration.
inline constexpr int64_t STARTER_RNG_MIN_SEED_WAIT_MS = 31500;
//  Starter RNG's title screen wait for `seed`, as prepare_timings() makes it
//  before calibration.
int64_t starter_rng_seed_wait_ms(const InitialSeedEntry& seed);
//  Whether the wait is at least STARTER_RNG_MIN_SEED_WAIT_MS. (Console B,
//  2026-11-02, FireRed English: seeds 11A3 and 405C, at 30,473 and
//  30,488 ms in the table, are 29,628 and 29,643 ms waits, which Starter
//  RNG refuses.)
bool starter_rng_can_wait_for(const InitialSeedEntry& seed);

//  Starter RNG's Continue Screen frames, taken off the advances before the
//  in-game wait (CONTINUE_SCREEN_FRAMES in StarterRng's attempts).
inline constexpr uint64_t STARTER_RNG_CONTINUE_SCREEN_FRAMES = 200;
//  Starter RNG's in-game wait for `advances`, before calibration. It
//  refuses one under 7,500 ms, which STARTER_RNG_MIN_ADVANCES just makes.
int64_t starter_rng_ingame_ms(uint64_t advances);
//  Room for the in-game calibration, in advances (2 per frame of wait): the
//  calibration moves the wait, and it went to -720 after one miss on console
//  B (2026-11-02, seed B579 at 967 advances: an 8 s wait cut to 1.7 s). The
//  plan's targets are at least this many advances past Starter RNG's
//  minimum. To tune from the logs' calibrations.
inline constexpr uint64_t STARTER_RNG_INGAME_MARGIN_ADVANCES = 720;
inline constexpr uint64_t STARTER_RNG_PLAN_MIN_ADVANCES = STARTER_RNG_MIN_ADVANCES + STARTER_RNG_INGAME_MARGIN_ADVANCES;

//  The attempts for Starter RNG, from the seed list of the version and
//  language in Help button mode, the one Starter RNG uses.
std::vector<RngAttempt> plan_starter_rng(const StarterRngPlanInputs& inputs);

//  One line per attempt, for the log and the stop message: the Target Seed
//  and Advances to give Starter RNG, the Pokemon, and the SIDs it is shiny for.
std::vector<std::string> rng_attempt_lines(const std::vector<RngAttempt>& attempts);


//  The data to rebuild how likely SID Helper's k-th candidate of N is (the
//  1-2-3-2-1 weights of sid_helper_candidates() are a guess), from the logs
//  alone, and to follow AutoStory's Starter RNG loop. Every line starts with
//  "RngPlan:"; grep for it. In the order a run logs them:
//      RngPlan: tid=<TID> source=<card|option>
//      RngPlan: sid_helper tid=<TID> target_advances=<A> candidates=<SID>,...
//      RngPlan: attempt=<i> seed=<hex> advances=<n> sids=<SID>,...
//          seed_wait_ms=<ms> ingame_ms=<ms>   (Starter RNG's waits, before calibration)
//  then, with Run Starter RNG in AutoStory, for each try of an attempt
//  (rng_log_state() in PokemonFRLG_AutoStoryRngLoop.h):
//      RngPlan: state attempt=<i>/<n> try=<t>/<tries> reset=<r>/<max> seed=<hex>
//          advances=<n> sids=<SID>,... ruled_out=<SID>,... attempt_time=<h:mm:ss>/<cap>
//          total_time=<h:mm:ss>/<cap> calibration=<seed>/<csf>/<ingame frames|->
//      (once at the try's start, then after every reset)
//      RngPlan: result attempt=<i> try=<t> outcome=<shiny|shiny_off_target|hit_not_shiny|missed|unusable>
//          starter_rng=<Starter RNG's outcome> nature=<N|-> ivs=<a/b/c/d/e/f|->
//          pid=<hex|-> next=<retry|next_attempt|confirm|stop>
//      RngPlan: confirmed tid=<TID> sid=<SID> candidate=<k> of=<N>
//  Lists that are empty print "-".
//  "candidates" are in SID Helper's order; "candidate=k" counts from 1 in
//  it, 0 when the SID wasn't among them. Count each TID's "confirmed" once:
//  every later run with the SID set logs it again. While the user runs
//  Starter RNG by hand, its own "Target Hit!" and "Missed target." lines
//  are the attempts' results.
std::string rng_log_tid(uint16_t tid, bool from_card);
std::string rng_log_sid_helper(uint16_t tid, uint32_t target_advances, const std::vector<SidCandidate>& candidates);
std::string rng_log_attempt(size_t index, const RngAttempt& attempt);
//  What the loop does after an attempt's result.
enum class RngPlanNext{
    RETRY,
    NEXT_ATTEMPT,
    //  Shiny and the target: go on with the story.
    CONFIRM,
    STOP,
};
//  The step after `index` came out as the tracker's `step` and its attempt
//  is now `next_index`.
RngPlanNext rng_plan_next(RngPlanStep step, size_t index, size_t next_index);

struct RngResultLog{
    size_t index = 0;
    //  From 1.
    size_t try_number = 1;
    RngAttemptOutcome outcome = RngAttemptOutcome::MISSED;
    //  How the RNG program put it ("out_of_resets"); empty for none.
    std::string detail;
    //  The Pokemon hit, when known.
    std::optional<Pokemon::AdvPokemonResult> pokemon;
    RngPlanNext next = RngPlanNext::STOP;
};
std::string rng_log_result(const RngResultLog& result);
std::string rng_log_confirmed(uint16_t tid, uint16_t sid, const std::vector<SidCandidate>& sid_helper_candidates);


void add_tests_AutoStoryRngPlan(UnitTestDatabase& database);



}
}
}
#endif

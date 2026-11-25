/*  Mail Glitch Battle Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Battle-driven GAME steps for Mail Glitch Activation:
 *  - switch_grind: switch-train a party member (CH'DING / MIMIEN) using a
 *    sweeper (Victory Road 2F with Indigo Plateau Pokemon Center healing, or
 *    Pokemon Tower 5F PurifiedZone) with zero-drift in-place spinning.
 *  - prepare_berry_hp: faint a low-level Berry holder in a wild battle, flee
 *    with the sweeper, and Revive it to exact maxHP / 2 for Turn-0 Berry proc.
 *  - prepare_berry_pp: use up a lead move to trigger the Leppa Berry.
 *  - mail_glitch_double_battle: consume Leppa on Blastoise, switch MIMIEN into
 *    Knock Off, Recycle, then finish the battle with Blastoise.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchBattle_H
#define PokemonAutomation_PokemonFRLG_MailGlitchBattle_H

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySummary.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline constexpr std::string_view MAIL_GLITCH_GRIND_CHECKPOINT = "mg-04b";
inline constexpr std::string_view GRIND_BATCH_USED_FLAG = "mail_glitch_grind_batch_used";

std::string grind_done_flag(std::string_view species);
bool mail_glitch_grind_done(const std::set<std::string>& flags);


//  Attacking move slot on the sweeper with tracked PP for switch-grinding.
struct SweeperMoveEntry{
    size_t slot = 0;
    std::string slug;
    int priority = 0;
    int max_pp = 15;
    int remaining_pp = 15;
};

//  Prioritized attacking move pool for switch_grind (prefers Surf, then Ice/
//  high-BP coverage, then Strength/Bite; falls through as PP runs low or on
//  BattleOutOfPpDetector).
class SweeperMovePool{
public:
    explicit SweeperMovePool(const SummaryRead& summary);

    size_t pick_slot() const;
    void note_used(size_t slot);
    void mark_empty(size_t slot);
    void restore_all();
    bool low_pp() const;
    const std::vector<SweeperMoveEntry>& entries() const{ return m_moves; }

private:
    std::vector<SweeperMoveEntry> m_moves;
};

//  Whether switch_grind should open the trainee's Summary screen after a
//  battle (always on move prompt or within 2 levels, every 5 within 6 levels,
//  otherwise every 12 battles).
bool should_check_grind_summary(
    unsigned current_level,
    unsigned target_level,
    int64_t battles_since_check,
    bool saw_move_prompt
);

Pokemon::Outcome run_switch_grind_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);

Pokemon::Outcome run_clear_purified_zone_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);

Pokemon::Outcome run_prepare_berry_hp_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);

Pokemon::Outcome run_prepare_berry_pp_step(StoryEnv& env, const Pokemon::StoryStep& step);

Pokemon::Outcome run_mail_glitch_double_battle_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Mail Glitch Battle Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Battle-driven GAME steps for Mail Glitch Activation:
 *  - switch_grind: switch-train a party member (CH'DING / MIMIEN) using a
 *    sweeper on Pokemon Tower 5F with PurifiedZone free healing and move learn.
 *  - prepare_berry_hp: faint a low-level Berry holder in a wild battle, flee
 *    with the sweeper, and Revive it to exact maxHP / 2 for Turn-0 Berry proc.
 *  - mail_glitch_double_battle: execute the Turn-0 Berry, Turn-1 Knock Off on
 *    ally slot (switch to MIMIEN), Turn-2 Recycle + sweeper swap, and sweep.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchBattle_H
#define PokemonAutomation_PokemonFRLG_MailGlitchBattle_H

#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


Pokemon::Outcome run_switch_grind_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);

Pokemon::Outcome run_prepare_berry_hp_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);

Pokemon::Outcome run_mail_glitch_double_battle_step(
    StoryEnv& env,
    Pokemon::RunState& state,
    const Pokemon::StoryStep& step
);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  AutoStory Center Heal
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's Pokemon Center on the shared visit (Pokemon_CenterVisit.h):
 *  the center_enter and center_exit GAME steps walk its doors, the HEAL
 *  step is the nurse, each checked against the Center's floor on screen.
 *  FRLG keeps the three steps (each with its declared pose) where BDSP has
 *  one HEAL step for the whole visit.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCenterHeal_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCenterHeal_H

#include <string_view>
#include "Pokemon/AutoStory/Pokemon_CenterVisit.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  FRLG's side of a visit, the console's and the sim's: walk_in() and
//  walk_out() are the room's legs (center_enter_legs(), center_exit_legs()),
//  which wait out each door's fade to its end and then AFTER_WARP
//  (wait_out_warp()), so the settle after them waits no more.
class FrlgCenterVisit : public Pokemon::CenterVisitIO{
public:
    virtual void settle(Pokemon::SettleKind, std::string_view) override{}
};

//  The center_enter step `step` (Pokemon::enter_center()), or its
//  center_exit (Pokemon::leave_center()), on `io`; ends on the step's
//  pose_after.
Pokemon::Outcome run_center_step(const Pokemon::StoryStep& step, Pokemon::CenterVisitIO& io);

//  The nurse's heal, only where `io` reads the Center's floor: on 12.03
//  (console B, 2026-11-13 04:16) a route taken as walked left the player
//  on Route 21's water, the Center entry after it was taken from a still
//  screen, and the heal's A mashing was counted as the nurse's. Anywhere
//  else, or with no frame, fails Pokemon::CENTER_NOT_ENTERED before her.
Pokemon::Outcome heal_at_nurse(std::string_view label, Pokemon::CenterVisitIO& io);

//  On the console: run_center_step(), and heal_at_nurse() with upstream's
//  heal_at_pokecenter() as the nurse and the party's HP and PP then
//  counted full in env.supplies (healed_by_nurse()).
Pokemon::Outcome center_step(StoryEnv& env, const Pokemon::StoryStep& step);
Pokemon::Outcome heal_in_center(StoryEnv& env, std::string_view label);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

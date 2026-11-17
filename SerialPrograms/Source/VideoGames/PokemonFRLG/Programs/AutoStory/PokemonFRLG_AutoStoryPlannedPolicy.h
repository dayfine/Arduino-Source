/*  AutoStory Planned Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's battle policy as PokemonSimulation runs it ("planned",
 *  src/pokesim/games/frlg/league/planned.py, docs/battles/frlg-league-plan.md):
 *  the League plan (PokemonFRLG_AutoStoryLeaguePlanData.h) plus a short
 *  override table, first row that applies. Each choice names its row:
 *    locked          a forced move (two-turn, recharge, Outrage);
 *    sure_ko         a move's lowest roll KOs the foe (at full HP when its
 *                    trainer's Full Restore lands first), it can't miss
 *                    (accuracy with stages and evasion) and we move first
 *                    (speed with stages and paralysis): the cheapest one;
 *                    not while this foe has a setup goal left, we're safe,
 *                    and its trainer has more Pokemon;
 *    sleep_cure      asleep or frozen: a Full Restore at KO risk, else the cure;
 *    foe_heal        the foe's heal block: below its HP, the smallest
 *                    spendable heal restoring at least its amount (before
 *                    sure_ko with heal_when_can_ko);
 *    status_ko       a status at KO risk: a Full Restore;
 *    ko_heal         HP <= D, the foe's worst ordinary hit now: the smallest
 *                    heal that clears D with a mean hit to spare; after two
 *                    medicine turns with the foe's HP unchanged, a Full Restore;
 *    cure            paralysis, poison or a burn with no KO risk: the cure;
 *    kill_order      the trainer can still heal and the planned hit can
 *                    leave the foe at 1-24%: a hit that leaves it at 25% or
 *                    more (one after which the finisher KOs, if any); not
 *                    for a foe with a sequence or kill_order off;
 *    pp_restore      every planned move out of PP: an Elixir not reserved;
 *    plan:setup      the first unmet setup goal whose move has PP, with the
 *                    HP for it (else plan:setup_heal);
 *    plan:move       the foe's sequence move for our attack count, else
 *                    the first planned move with PP above the reserve;
 *    plan:struggle   no move with PP: the caller's best attack.
 *  Pure decisions from the state the caller builds
 *  (PokemonFRLG_AutoStoryPlannedState.h); nothing here reads a screen.
 *  The KO risk is PokemonFRLG_AutoStoryPlannedRisk.h's; the top-ups
 *  between fights are PokemonFRLG_AutoStoryPlannedBetween.h's.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedPolicy_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedPolicy_H

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryPlannedState.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A turn's choice: an item, a move slot, or neither for plan:struggle;
//  and the row that chose it, for the log.
struct PlannedChoice{
    std::string row = {};
    std::optional<std::string> item = std::nullopt;
    std::optional<size_t> move_slot = std::nullopt;
};

//  The override table and the plan for `fight`; `streak` is the medicine
//  turns in a row against the foe at this HP (PlannedStreak).
PlannedChoice choose_planned(const PlannedState& state, const LeagueFightData& fight, int streak);

//  The medicine turns in a row against one foe at one HP, and our attacks
//  on each foe's party slot (PlannedState::attacks_on_foe).
class PlannedStreak{
public:
    int streak(const PlannedState& state) const;
    int attacks(size_t foe_slot) const;
    void chose(const PlannedState& state, const PlannedChoice& choice);

private:
    std::string m_species;
    int m_hp = -1;
    int m_streak = 0;
    std::map<size_t, int> m_attacks;
};

//  Registers the planned policy tests.
void add_tests_AutoStoryPlannedPolicy(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

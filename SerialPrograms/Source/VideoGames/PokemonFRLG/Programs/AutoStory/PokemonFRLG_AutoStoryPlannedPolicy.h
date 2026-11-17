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
 *    status_ko       a status at KO risk: a Full Restore;
 *    ko_heal         HP <= D, the foe's worst ordinary hit now: the smallest
 *                    heal that clears D with a mean hit to spare; after two
 *                    medicine turns with the foe's HP unchanged, a Full Restore;
 *    cure            paralysis, poison or a burn with no KO risk: the cure;
 *    kill_order      the trainer can still heal and the planned hit can
 *                    leave the foe at 1-24%: a hit that leaves it at 25% or
 *                    more (one after which the finisher KOs, if any);
 *    pp_restore      every planned move out of PP: an Elixir not reserved;
 *    plan:setup      the first unmet setup goal whose move has PP, with the
 *                    HP for it (else plan:setup_heal);
 *    plan:move       the first planned move with PP above the reserve;
 *    plan:struggle   no move with PP: the caller's best attack.
 *  Pure decisions from the state the caller builds; nothing here reads a
 *  screen.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedPolicy_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedPolicy_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A move as the policy weighs it.
struct PlannedMove{
    std::string id = {};
    int pp = 0;
    //  The chance it hits, with stages and evasion (hit_chance()).
    double accuracy = 1.0;
    //  Its damage rolls on the other side, lowest first
    //  (gen3_damage_rolls()); empty for no damage.
    std::vector<int> damage = {};
    int priority = 0;
    //  Charges a turn first (Solar Beam, Dive, Sky Attack).
    bool two_turn = false;
};

enum class PlannedStatus{
    NONE,
    POISON,
    PARALYSIS,
    SLEEP,
    BURN,
    FREEZE,
};

//  Our Pokemon out.
struct PlannedOwn{
    int hp = 0;
    int max_hp = 0;
    PlannedStatus status = PlannedStatus::NONE;
    //  battle_speed(): with Thunder's boost, the stage and paralysis.
    int speed = 0;
    int spa_stage = 0;
    int speed_stage = 0;
    int accuracy_stage = 0;
    int attack_stage = 0;
    int defense_stage = 0;
    bool mist = false;
    bool leech_seed = false;
    //  In the move list's order.
    std::vector<PlannedMove> moves = {};
    //  The slot of a move we are locked into.
    std::optional<size_t> locked_slot = std::nullopt;
};

//  The foe out.
struct PlannedFoe{
    std::string species = {};
    //  Its trainer's party slot.
    size_t slot = 0;
    int hp = 0;
    int max_hp = 0;
    int speed = 0;
    bool recharging = false;
    //  Asleep for more than this turn.
    bool sleeping = false;
    bool frozen = false;
    //  The two-turn move it is charging.
    std::optional<std::string> charging = std::nullopt;
    bool sun = false;
    //  Its moves, with their damage on us.
    std::vector<PlannedMove> moves = {};
};

struct PlannedState{
    PlannedOwn own = {};
    PlannedFoe foe = {};
    //  The trainer's Pokemon still standing, the one out included.
    int foe_party_alive = 1;
    //  The trainer has an open item slot (trainer_item_open()).
    bool trainer_item_open = false;
    SlugCounts bag = {};
};

//  pret ShouldUseItem's slot gate for a trainer carrying `carried` items
//  of one kind (Full Restores), `used` of them used, with `alive` Pokemon
//  left: slot i > 0 stays shut while more than 4 - i + 1 are alive.
bool trainer_item_open(int carried, int used, int alive);

//  The foe's HP when our move lands: full when its trainer uses the item
//  first (under 1/4 with a slot open).
int foe_hp_at_our_move(const PlannedState& state);

//  The HP we can lose this turn: residual damage plus the foe's worst
//  ordinary hit as it stands (normal_max), and the mean of its best hit.
//  Residual only while it recharges, sleeps, is frozen or uses an item.
struct PlannedDanger{
    int normal_max = 0;
    double mean = 0;
};
PlannedDanger planned_danger(const PlannedState& state);

bool we_move_first(const PlannedState& state, const PlannedMove& move);

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

//  The medicine turns in a row against one foe at one HP.
class PlannedStreak{
public:
    int streak(const PlannedState& state) const;
    void chose(const PlannedState& state, const PlannedChoice& choice);

private:
    std::string m_species;
    int m_hp = -1;
    int m_streak = 0;
};

//  Our side between two fights.
struct PlannedCarry{
    int hp = 0;
    int max_hp = 0;
    PlannedStatus status = PlannedStatus::NONE;
    //  PP left and full PP, by move.
    SlugCounts pp = {};
    SlugCounts full_pp = {};
    SlugCounts bag = {};
};

//  The top-ups before `next` (its `between` row): a cure, Super Potions
//  (then a Full Restore) to the HP target, a PP item to the PP floor;
//  `next`'s item reserve holds. Updates `carry`; returns the items used,
//  in order.
std::vector<std::string> planned_between(PlannedCarry& carry, const LeagueFightData& next);


//  Registers the planned policy tests.
void add_tests_AutoStoryPlannedPolicy(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

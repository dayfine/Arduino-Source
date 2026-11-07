/*  AutoStory Battle Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's lead against a trainer's party as Gen 3 FireRed plays it,
 *  for the battles fought at BattleRisk::HIGH: the Pokemon on each side,
 *  the trainer AI's choices (its items, its moves, its next Pokemon) and
 *  the lead's (its move, a Potion). PokemonFRLG_AutoStoryWinChance.h plays
 *  whole battles from these.
 *
 *  The trainer's side is pret's: party, levels, IVs, natures, moves, items
 *  and AI flags (trainers.json). The lead is what its summary showed
 *  when the run started (species, level, moves and PP, stats from POKEMON
 *  SKILLS), with the levels since (battle_level()) added at OWN_GENES;
 *  without a summary, its species and battle_level() at OWN_GENES.
 *
 *  Approximations, from pret/pokefirered:
 *  - The AI's move (battle_ai_script_commands.c, data/battle_ai_scripts.s):
 *    every move starts at 100; CHECK_BAD_MOVE takes 10 off a move the
 *    target is immune to, a status the target already has, Leech Seed on
 *    a seeded or Grass target, and a stat change past +-6; TRY_TO_FAINT
 *    adds 4 to a move that faints the target at the mean roll (6 for Quick
 *    Attack) and takes 1 off the damaging moves that aren't its strongest;
 *    CHECK_VIABILITY scores the status moves the early trainers carry
 *    (AI_CV_AttackDown, DefenseDown, AccuracyDown, SpeedDown, Poison,
 *    Toxic for Leech Seed too, Paralyze, Heal) by HP, speed and stages,
 *    with their random parts. The best score wins, ties at random. The
 *    other scripts, and CHECK_VIABILITY on damaging moves, are left out.
 *  - The AI's items (battle_ai_switch_items.c ShouldUseItem) as the game
 *    has them for the HP items: Full Restore below 1/4 HP, the others
 *    below 1/4 or when more HP is missing than the item heals; each item
 *    once, and item i > 0 only once at most itemsNo - i + 1 Pokemon are
 *    left. Other items are not used.
 *  - The AI's next Pokemon (GetMostSuitableMonToSwitchInto): the one the
 *    lead's types hit hardest that has a super effective move against the
 *    lead, else the one whose strongest move does the lead most damage,
 *    else the first left.
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleSim_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleSim_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryDamage.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The lead's IVs and EVs, which the story doesn't read: middling IVs, no
//  EVs, a neutral nature (Hardy).
inline constexpr Genes OWN_GENES{.iv = 15, .ev = 0, .nature = 0};

//  HP a Potion heals.
inline constexpr uint16_t POTION_HP = 20;

//  Simulated battler status condition.
enum class SimStatus{
    NONE,
    SLEEP,
    POISON,     //  and Toxic, as plain poison
    PARALYSIS,
};

//  A move in simulated combat.
struct SimMove{
    std::string move = {};
    uint8_t pp = 0;
};

//  A Pokemon in the sim.
struct SimBattler{
    std::string species = {};
    Combatant body = {};
    uint16_t hp = 0;
    //  In the move list's slot order.
    std::vector<SimMove> moves = {};
    SimStatus status = SimStatus::NONE;
    //  SLEEP: the game's counter (2 to 5): each turn takes one off, and it
    //  moves on the turn it reaches 0.
    uint8_t sleep = 0;
    bool seeded = false;
};

//  The lead with `hp_share` of its HP (1 for full), its moves and PP as
//  the story counts them, at battle_level(): its stats as read
//  (PartyMember::stats) plus what the levels since add with `genes`, else
//  from `genes`. None for a species the data doesn't have.
std::optional<SimBattler> own_battler(const PartyMember& lead, double hp_share = 1.0, const Genes& genes = OWN_GENES);

//  A trainer's Pokemon at full HP and PP.
std::optional<SimBattler> trainer_battler(const TrainerMon& mon);

//  Simulated opposing trainer state.
struct SimTrainer{
    std::vector<SimBattler> party = {};
    //  Each used once; a used one becomes "".
    std::vector<std::string> items = {};
    bool check_bad_move = false;
    bool try_to_faint = false;
    bool check_viability = false;
};
//  None for a trainer the data doesn't have (or a species in its party).
std::optional<SimTrainer> sim_trainer(std::string_view trainer);


//  The HP the AI's `item` heals (255 for all of it), 0 for one that
//  doesn't heal.
uint16_t ai_item_hp(std::string_view item);

//  The item the AI uses on `foe` this turn (its index in `items`), with
//  `foes_left` Pokemon on its side not fainted, none for its move.
std::optional<size_t> ai_item_choice(const SimBattler& foe, const std::vector<std::string>& items, size_t foes_left);

//  The scores the AI gives `foe`'s moves against `target` (see the top);
//  0 for a move with no PP. In slot order. `rng` for CHECK_VIABILITY's
//  random parts.
std::vector<int> ai_move_scores(const SimTrainer& trainer, const SimBattler& foe, const SimBattler& target, std::mt19937& rng);

//  The party slot the AI sends out after `fainted`, of the ones still up.
size_t ai_next_foe(const std::vector<SimBattler>& party, size_t fainted, const SimBattler& lead);


//  BattleRisk::LOW: the slot with PP left whose mean damage against `foe`
//  is highest. None if no move does damage.
std::optional<size_t> best_damage_slot(const SimBattler& own, const SimBattler& foe);

//  BattleRisk::HIGH: a move that faints `foe` at the lowest roll, the
//  weakest of them to keep the strong moves' PP; else the one that faints
//  it in the fewest turns at the mean roll and accuracy, counting the
//  heals the AI would use on it from `items` (ai_item_choice()), ties to
//  the weaker move. None if no move does damage.
std::optional<size_t> planned_attack_slot(
    const SimBattler& own, const SimBattler& foe,
    const std::vector<std::string>& items, size_t foes_left
);

//  The most `foe` can do to `own` with one move, no crit, highest roll.
uint32_t foe_max_hit(const SimBattler& foe, const SimBattler& own);

//  What a Potion now is weighed on.
struct PotionCase{
    uint16_t hp = 0;
    uint16_t max_hp = 0;
    //  The foe's next hit, on average and at most.
    double expected_hit = 0;
    uint32_t max_hit = 0;
    //  The foe's hits the lead takes before its attacks faint the foe, if
    //  it attacks from now on.
    size_t hits_to_ko = 0;
};
//  `own` against `foe`: the foe's strongest move at the mean roll and
//  accuracy, or `seen_hit` (HP the last hit took, as the battle loop read
//  it) if more; the lead's planned move against the foe at its HP now.
PotionCase potion_case(const SimBattler& own, const SimBattler& foe, double seen_hit = 0);

//  At either BattleRisk: a Potion is worth the turn only if, after it and
//  the foe's expected hit, the lead has gained HP, or if it lets the lead
//  last until its attacks faint the foe when attacking now wouldn't.
//  Otherwise it attacks: a Potion the next hit undoes only gives the foe
//  another turn (console B, 2026-11-03, m51: five Potions at 8/60 HP each
//  undone by Misty's Starmie, then no Potions and a faint).
bool potion_pays(const PotionCase& c);

//  BattleRisk::HIGH: a Potion now: `foe`'s next hit could faint `own`,
//  after the Potion it couldn't, and potion_pays().
bool potion_now(const SimBattler& own, const SimBattler& foe, int potions, double seen_hit = 0);

//  The lead's turn at BattleRisk::HIGH.
struct OwnTurn{
    bool potion = false;
    //  Not a Potion: the move's slot, none if no move does damage.
    std::optional<size_t> slot = std::nullopt;
};
//  planned_attack_slot()'s move when it surely faints `foe` first (the
//  lead is faster, the lowest roll faints it, it never misses and the AI
//  won't heal first), else a Potion if potion_now(), else that move.
OwnTurn own_turn(
    const SimBattler& own, int potions, const SimBattler& foe,
    const std::vector<std::string>& items, size_t foes_left
);


//  The damage calculator, the sim's choices and its win chance.
void add_tests_AutoStoryBattleSim(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

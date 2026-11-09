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
 *    Toxic for Leech Seed too, Paralyze, Heal, EvasionUp) by HP, speed and stages,
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
#include <utility>
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

//  A Potion is used only when its HP covers this share of the foe's
//  next-turn damage (next_turn_damage()): at 0.80, a turn that undoes it
//  must be rarer than 1 in 5.
inline constexpr double POTION_COVER_PERCENTILE = 0.80;

//  An Antidote is used, once a battle, on the turn the lead is read
//  poisoned this many turns in a row, if no Potion or Awakening comes
//  first.
inline constexpr int PSN_TURNS_BEFORE_ANTIDOTE = 2;

//  The lead's items in the bag that the sim plays.
struct SimItems{
    int potions = 0;
    int awakenings = 0;
    int antidotes = 0;
    //  The ones only the plan uses (PokemonFRLG_AutoStoryBattleStrategy.h).
    int super_potions = 0;
    int hyper_potions = 0;
    int full_restores = 0;
    int x_attacks = 0;
    int x_specials = 0;
    int x_speeds = 0;
    int x_accuracies = 0;
};


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
    //  The two-turn move (two_turn()) it charged last turn and strikes
    //  with this turn, "" for none.
    std::string charging = {};
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

//  How often the AI picks each of `foe`'s moves against `target` now, in
//  slot order: ai_move_scores() drawn AI_MOVE_SAMPLES times from a fixed
//  seed, ties split evenly. The chances change with the state (a status
//  on the target, Leech Seed, stages, HP). All 0 with no PP left.
inline constexpr size_t AI_MOVE_SAMPLES = 1000;
//  Calculates the probability distribution of enemy AI move selection.
std::vector<double> ai_move_chances(const SimTrainer& trainer, const SimBattler& foe, const SimBattler& target);

//  The chance in percent that `user`'s `move` hits `target`: its accuracy
//  at the user's accuracy and the target's evasion stages
//  (sAccuracyStageRatios); 100 for a move that never misses.
uint32_t hit_percent(const SimBattler& user, const SimBattler& target, const MoveData& move);

//  A move's crit odds: 1 in 16, 1 in 8 for a high-crit move.
uint32_t crit_odds(const MoveData& move);

//  A move that charges a turn and hits the next: Dig, Fly, Dive, Bounce,
//  Solar Beam (no sun in the sim), Razor Wind, Skull Bash, Sky Attack.
bool two_turn(const MoveData& move);

//  Under ground, in the air or under water: charging Dig, Fly, Dive or
//  Bounce. pret AccuracyCalcHelper misses every move at it before the
//  never-miss check (Shock Wave too); moves on the user itself (Double
//  Team) don't check.
bool semi_invulnerable(const SimBattler& battler);

//  The HP `own` loses over the foe's next turn, as the AI picks its move
//  (ai_move_chances()): each damaging move's 16 rolls, crit chance,
//  accuracy with stages and multi-hit counts; a poison or Leech Seed that
//  lands this turn and the poison and Leech Seed already on `own` (1/8 HP
//  each at the end of the turn). An asleep foe doesn't move, a paralysed
//  one 1 turn in 4. A Potion comes before both moves (items act first),
//  so all of it lands after one.
struct NextTurnDamage{
    //  ai_move_chances().
    std::vector<double> move_chances = {};
    //  hp_lost[d]: the chance of losing d HP.
    std::vector<double> hp_lost = {};

    double mean() const;
    //  The least HP lost at least `share` of the time.
    uint32_t percentile(double share) const;
    uint32_t most() const;
};
//  Calculates incoming damage distribution across the foe's next turn.
NextTurnDamage next_turn_damage(const SimTrainer& trainer, const SimBattler& foe, const SimBattler& own);

//  The chance `foe`'s next turn seeds `own`: the AI's chance of Leech Seed
//  times its accuracy. 0 if it has none, or `own` is seeded or Grass.
double seed_chance(const SimTrainer& trainer, const SimBattler& foe, const SimBattler& own);

//  The party slot the AI sends out after `fainted`, of the ones still up.
size_t ai_next_foe(const std::vector<SimBattler>& party, size_t fainted, const SimBattler& lead);


//  BattleRisk::LOW: the slot with PP left whose mean damage a turn against
//  `foe` is highest. None if no move does damage.
std::optional<size_t> best_damage_slot(const SimBattler& own, const SimBattler& foe);

//  BattleRisk::HIGH: a move that faints `foe` at the lowest roll, the
//  weakest of them to keep the strong moves' PP; else the one that faints
//  it in the fewest turns at the mean roll and accuracy, a two-turn move
//  at half its damage a turn (console B, m73: Dig against Lt. Surge's
//  Raichu, the Raichu's Double Team landing while the lead was under
//  ground), counting the
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
    //  The HP the foe's next turn takes (next_turn_damage()): on average,
    //  at POTION_COVER_PERCENTILE and at most.
    double expected_hit = 0;
    uint32_t likely_hit = 0;
    uint32_t max_hit = 0;
    //  The lead's attacks that faint the foe at the mean, and the foe's hits
    //  the lead takes before they do, if it attacks from now on.
    size_t turns_to_ko = 0;
    size_t hits_to_ko = 0;
    //  The foe's moves and ai_move_chances(), for the log.
    std::vector<std::pair<std::string, double>> move_chances = {};
};
//  `own` against `foe`, played by `trainer`'s AI: next_turn_damage(), each
//  figure raised to `seen_hit` (HP the last hit took, as the battle loop
//  read it) if more; the lead's planned move against the foe at its HP now.
PotionCase potion_case(const SimBattler& own, const SimBattler& foe, const SimTrainer& trainer, double seen_hit = 0);

//  At either BattleRisk: a Potion is worth the turn only if its HP is more
//  than the foe's next turn takes at POTION_COVER_PERCENTILE, or if it lets
//  the lead last until its attacks faint the foe (at the mean) when
//  attacking now wouldn't. Otherwise it attacks: a Potion the next turn
//  undoes only gives the foe another turn (console B, 2026-11-03, m51: five
//  Potions at 8/60 HP each undone by Misty's Starmie, then no Potions and
//  a faint). `heal`: a bigger heal's HP, weighed the same way.
bool potion_pays(const PotionCase& c, uint16_t heal = POTION_HP);

//  BattleRisk::HIGH: a Potion now, with `potions` left: the foe's next turn
//  could faint the lead (PotionCase::max_hit) and potion_pays().
bool potion_now(const PotionCase& c, int potions);

//  At either BattleRisk, `own` asleep with an Awakening left: one now if
//  the foe's next turn can hurt it and it outlasts that turn at
//  POTION_COVER_PERCENTILE (`c`). The game counts sleep down only on turns
//  the lead tries to move (pret battle_util.c AtkCanceller_UnableToUseMove),
//  so the turn saves the rest of the 1 to 4 asleep. Below that the lead's
//  move is tried instead, to wake and attack, or a Potion as the Potion
//  rules say.
bool awakening_now(const PotionCase& c, const SimBattler& own, int awakenings);

//  Task #77's top-up before the knockout, with `potions` left: the lead's
//  next attack faints the foe out at the mean, whose turn takes at most
//  half a Potion on average (`c`), a Potion heals at least a quarter of its
//  HP more than that, and a foe in `to_come` hits harder on average
//  (next_turn_damage()). Each such turn costs a small hit and heals more,
//  so the lead meets the stronger foe near full.
bool top_up_now(
    const PotionCase& c, const SimBattler& own, const SimTrainer& trainer,
    const std::vector<SimBattler>& to_come, int potions
);

//  The lead's turn at BattleRisk::HIGH.
struct OwnTurn{
    bool potion = false;
    //  Not a Potion or an Awakening: the move's slot, none if no move does
    //  damage.
    std::optional<size_t> slot = std::nullopt;
    bool awakening = false;
    //  The Potion is top_up_now()'s.
    bool top_up = false;
};
//  A Potion if top_up_now() (`to_come`: the trainer's Pokemon not out and
//  not fainted), else planned_attack_slot()'s move when it surely faints
//  `foe` first (the lead is awake and faster, the lowest roll faints it, it
//  never misses, takes one turn and the AI won't heal first), else a Potion if
//  potion_now(), else an Awakening if awakening_now(), else that move.
//  `trainer`: its AI and the items it has left. `seen_hit`: as
//  potion_case() takes it.
OwnTurn own_turn(
    const SimBattler& own, const SimItems& own_items, const SimBattler& foe,
    const SimTrainer& trainer, size_t foes_left,
    const std::vector<SimBattler>& to_come = std::vector<SimBattler>(), double seen_hit = 0
);


//  The damage calculator, the sim's choices and its win chance.
void add_tests_AutoStoryBattleSim(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

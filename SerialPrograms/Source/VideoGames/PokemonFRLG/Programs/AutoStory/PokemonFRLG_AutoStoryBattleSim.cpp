/*  AutoStory Battle Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>
#include "PokemonFRLG_AutoStoryBattleSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int START_SCORE = 100;
constexpr int BAD_MOVE = 10;
constexpr int CAN_FAINT = 4;
constexpr int QUICK_FAINT = 2;
constexpr int NOT_STRONGEST = 1;
constexpr int MAX_STAGE = 6;
//  More turns than this to faint a foe counts as never.
constexpr size_t TURNS_CAP = 30;
//  The hits a multi-hit move is taken to land when sizing a foe's hit.
constexpr uint32_t MULTI_HITS = 3;

const MoveData& data_of(std::string_view move){
    static const MoveData NONE;
    const MoveData* data = move_data(move);
    return data == nullptr ? NONE : *data;
}

size_t effectiveness(PokemonType type, const Combatant& target){
    size_t tenths = 10;
    for (size_t multiplier : type_multipliers(type, target.first, target.second)){
        tenths = tenths * multiplier / 10;
    }
    return tenths;
}

bool has_type(const Combatant& combatant, PokemonType type){
    return combatant.first == type || combatant.second == type;
}

uint32_t hits_of(const MoveData& move){
    if (move.effect == "multi-hit"){
        return MULTI_HITS;
    }
    return move.effect == "double-hit" || move.effect == "twineedle" ? 2 : 1;
}

uint32_t speed_of(const SimBattler& battler){
    const uint32_t speed = staged_stat(battler.body.stats.speed, battler.body.stages[static_cast<size_t>(Stage::SPEED)]);
    return battler.status == SimStatus::PARALYSIS ? speed / 4 : speed;
}

bool status_effect(std::string_view effect){
    return effect == "sleep" || effect == "poison" || effect == "toxic" || effect == "paralyze";
}

//  A stat-stage effect ("attack-down", "defense-up-2") at its limit.
bool stage_at_limit(std::string_view effect, const SimBattler& user, const SimBattler& target){
    static const std::pair<std::string_view, Stage> STATS[] = {
        {"attack-", Stage::ATTACK}, {"defense-", Stage::DEFENSE}, {"speed-", Stage::SPEED},
        {"special-attack-", Stage::SP_ATTACK}, {"special-defense-", Stage::SP_DEFENSE},
        {"accuracy-", Stage::ACCURACY}, {"evasion-", Stage::EVASION},
    };
    for (const auto& [prefix, stage] : STATS){
        if (!effect.starts_with(prefix) || effect.ends_with("-hit")){
            continue;
        }
        const bool up = effect.substr(prefix.size()).starts_with("up");
        const int now = (up ? user : target).body.stages[static_cast<size_t>(stage)];
        return up ? now >= MAX_STAGE : now <= -MAX_STAGE;
    }
    return false;
}

//  AI_CheckBadMove, as the top of the header says.
bool bad_move(const MoveData& move, const SimBattler& user, const SimBattler& target){
    if (move.power > 0 && effectiveness(move.type, target.body) == 0){
        return true;
    }
    if (status_effect(move.effect) && target.status != SimStatus::NONE){
        return true;
    }
    if ((move.effect == "poison" || move.effect == "toxic")
        && (has_type(target.body, PokemonType::POISON) || has_type(target.body, PokemonType::STEEL))
    ){
        return true;
    }
    if (move.effect == "leech-seed" && (target.seeded || has_type(target.body, PokemonType::GRASS))){
        return true;
    }
    if ((move.effect == "restore-hp" || move.effect == "softboiled") && user.hp >= user.body.stats.hp){
        return true;
    }
    return stage_at_limit(move.effect, user, target);
}

void add_try_to_faint(std::vector<int>& scores, const SimBattler& foe, const SimBattler& target){
    std::vector<double> damage(foe.moves.size(), 0);
    double strongest = 0;
    for (size_t slot = 0; slot < foe.moves.size(); slot++){
        const MoveData& move = data_of(foe.moves[slot].move);
        if (move.power > 0 && foe.moves[slot].pp > 0){
            damage[slot] = average_damage(foe.body, target.body, move) * hits_of(move);
            strongest = move.power > 1 ? std::max(strongest, damage[slot]) : strongest;
        }
    }
    for (size_t slot = 0; slot < foe.moves.size(); slot++){
        const MoveData& move = data_of(foe.moves[slot].move);
        if (scores[slot] <= 0 || move.power == 0){
            continue;
        }
        if (damage[slot] >= target.hp){
            scores[slot] += CAN_FAINT + (move.effect == "quick-attack" ? QUICK_FAINT : 0);
        }else if (move.power > 1 && damage[slot] < strongest){
            scores[slot] -= NOT_STRONGEST;
        }
    }
}

//  CHECK_VIABILITY's terms: HP in percent, a stage as the game's level (6
//  for none), the speed comparison, and if_random_less_than (n / 256).
int hp_percent(const SimBattler& battler){
    return battler.hp * 100 / std::max<int>(battler.body.stats.hp, 1);
}
int level(const SimBattler& battler, Stage stage){
    return battler.body.stages[static_cast<size_t>(stage)] + MAX_STAGE;
}
bool faster(const SimBattler& a, const SimBattler& b){
    return speed_of(a) > speed_of(b);
}
bool random_below(std::mt19937& rng, uint32_t n){
    return rng() % 256 < n;
}

bool physical_type(PokemonType type){
    switch (type){
    case PokemonType::NORMAL:
    case PokemonType::FIGHTING:
    case PokemonType::GROUND:
    case PokemonType::ROCK:
    case PokemonType::BUG:
    case PokemonType::STEEL:
        return true;
    default:
        return false;
    }
}

int cv_attack_down(const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    int score = 0;
    if (level(target, Stage::ATTACK) != MAX_STAGE){
        score -= user.hp * 100 > 90 * user.body.stats.hp ? 1 : 2;
        score -= level(target, Stage::ATTACK) > 3 || random_below(rng, 50) ? 0 : 2;
    }
    score -= hp_percent(target) > 70 ? 0 : 2;
    const bool physical = physical_type(target.body.first) || physical_type(target.body.second);
    return score - (physical || random_below(rng, 50) ? 0 : 2);
}

int cv_defense_down(const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    int score = 0;
    if (hp_percent(user) < 70 || level(target, Stage::DEFENSE) <= 3){
        score -= random_below(rng, 50) ? 0 : 2;
    }
    return score - (hp_percent(target) > 70 ? 0 : 2);
}

int cv_accuracy_down(const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    int score = 0;
    if (hp_percent(user) < 70 || hp_percent(target) <= 70){
        score -= random_below(rng, 100) ? 0 : 1;
    }
    score -= level(user, Stage::ACCURACY) > 4 || random_below(rng, 80) ? 0 : 2;
    score += target.seeded && !random_below(rng, 70) ? 2 : 0;
    if (hp_percent(user) > 70 || level(target, Stage::ACCURACY) == MAX_STAGE){
        return score;
    }
    const bool low = hp_percent(user) < 40 || hp_percent(target) < 40;
    return score - (low || !random_below(rng, 70) ? 2 : 0);
}

int cv_toxic(const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    int score = 0;
    score -= hp_percent(user) > 50 || random_below(rng, 50) ? 0 : 3;
    return score - (hp_percent(target) > 50 || random_below(rng, 50) ? 0 : 3);
}

int cv_heal(const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    if (hp_percent(user) == 100){
        return -3;
    }
    if (!faster(target, user)){
        return -8;
    }
    if (hp_percent(user) >= 70 && !random_below(rng, 30)){
        return -3;
    }
    return random_below(rng, 20) ? 0 : 2;
}

//  CHECK_VIABILITY for the status moves (see the header).
int viability(const MoveData& move, const SimBattler& user, const SimBattler& target, std::mt19937& rng){
    const std::string_view effect = move.effect;
    if (effect == "attack-down"){
        return cv_attack_down(user, target, rng);
    }
    if (effect == "defense-down"){
        return cv_defense_down(user, target, rng);
    }
    if (effect == "accuracy-down"){
        return cv_accuracy_down(user, target, rng);
    }
    if (effect == "speed-down"){
        return faster(target, user) ? (random_below(rng, 70) ? 0 : 2) : -3;
    }
    if (effect == "poison"){
        return hp_percent(user) < 50 || hp_percent(target) <= 50 ? -1 : 0;
    }
    if (effect == "toxic" || effect == "leech-seed"){
        return cv_toxic(user, target, rng);
    }
    if (effect == "paralyze"){
        return faster(target, user) ? (random_below(rng, 20) ? 0 : 3) : (hp_percent(user) > 70 ? 0 : -1);
    }
    if (effect == "restore-hp" || effect == "softboiled"){
        return cv_heal(user, target, rng);
    }
    return 0;
}

bool has_super_effective(const SimBattler& foe, const SimBattler& lead){
    return std::ranges::any_of(foe.moves, [&lead](const SimMove& known){
        const MoveData& move = data_of(known.move);
        return move.power > 0 && effectiveness(move.type, lead.body) > 10;
    });
}

//  GetMostSuitableMonToSwitchInto's first pass: the lead's two types
//  (the same one twice for a single type) against each, the best
//  that has a super effective move.
std::optional<size_t> suitable_by_type(const std::vector<size_t>& left, const std::vector<SimBattler>& party, const SimBattler& lead){
    std::set<size_t> invalid;
    while (invalid.size() < left.size()){
        std::optional<size_t> best;
        size_t best_tenths = 0;
        for (size_t slot : left){
            const size_t tenths = invalid.contains(slot) ? 0
                : effectiveness(lead.body.first, party[slot].body) * effectiveness(lead.body.second, party[slot].body) / 10;
            if (!invalid.contains(slot) && tenths > best_tenths){
                best = slot;
                best_tenths = tenths;
            }
        }
        if (!best){
            return std::nullopt;
        }
        if (has_super_effective(party[*best], lead)){
            return best;
        }
        invalid.insert(*best);
    }
    return std::nullopt;
}

//  Its second pass: the strongest move, compared as the game's u8 does.
std::optional<size_t> suitable_by_damage(const std::vector<size_t>& left, const std::vector<SimBattler>& party, const SimBattler& fainted, const SimBattler& lead){
    std::optional<size_t> best;
    uint8_t best_damage = 0;
    for (size_t slot : left){
        for (const SimMove& known : party[slot].moves){
            const MoveData& move = data_of(known.move);
            const uint32_t damage = move.power > 1 ? damage_before_roll(fainted.body, lead.body, move, false) : 0;
            if (best_damage < damage){
                best_damage = static_cast<uint8_t>(damage);
                best = slot;
            }
        }
    }
    return best;
}

uint32_t lowest_roll(const SimBattler& own, const SimBattler& foe, const MoveData& move){
    return damage_range(own.body, foe.body, move, false).min * hits_of(move);
}

double mean_per_turn(const SimBattler& own, const SimBattler& foe, const MoveData& move){
    const double accuracy = move.accuracy == 0 ? 1.0 : move.accuracy / 100.0;
    return average_damage(own.body, foe.body, move) * hits_of(move) * accuracy;
}

//  Turns to faint `foe` with `per_turn` a turn, the AI healing it as
//  ai_item_choice() says.
size_t turns_to_faint(SimBattler foe, std::vector<std::string> items, size_t foes_left, double per_turn){
    double hp = foe.hp;
    for (size_t turn = 1; turn <= TURNS_CAP; turn++){
        foe.hp = static_cast<uint16_t>(std::max(0.0, std::ceil(hp)));
        if (const std::optional<size_t> item = ai_item_choice(foe, items, foes_left)){
            hp = std::min<double>(foe.body.stats.hp, hp + ai_item_hp(items[*item]));
            items[*item].clear();
        }
        hp -= per_turn;
        if (hp <= 0){
            return turn;
        }
    }
    return TURNS_CAP + 1;
}

//  The slots with PP left that do damage to `foe`.
std::vector<size_t> damaging_slots(const SimBattler& own, const SimBattler& foe){
    std::vector<size_t> slots;
    for (size_t slot = 0; slot < own.moves.size(); slot++){
        const MoveData& move = data_of(own.moves[slot].move);
        if (own.moves[slot].pp > 0 && damage_before_roll(own.body, foe.body, move, false) > 0){
            slots.push_back(slot);
        }
    }
    return slots;
}

}


std::optional<SimBattler> own_battler(const PartyMember& lead, double hp_share, const Genes& genes){
    std::optional<Combatant> body = combatant(lead.species, battle_level(lead), genes);
    if (!body){
        return std::nullopt;
    }
    if (lead.stats && lead.level > 0){
        //  The stats read, and what the levels since add with `genes`.
        const Stats then = combatant(lead.species, lead.level, genes)->stats;
        const Stats now = body->stats;
        const auto grown = [](uint16_t read, uint16_t before, uint16_t after){
            return static_cast<uint16_t>(read + after - before);
        };
        body->stats = Stats{
            .hp = grown(lead.stats->hp, then.hp, now.hp),
            .attack = grown(lead.stats->attack, then.attack, now.attack),
            .defense = grown(lead.stats->defense, then.defense, now.defense),
            .sp_attack = grown(lead.stats->sp_attack, then.sp_attack, now.sp_attack),
            .sp_defense = grown(lead.stats->sp_defense, then.sp_defense, now.sp_defense),
            .speed = grown(lead.stats->speed, then.speed, now.speed),
        };
    }
    SimBattler battler{.species = lead.species, .body = *body, .hp = 0, .moves = {}};
    battler.hp = static_cast<uint16_t>(std::clamp<double>(std::round(body->stats.hp * hp_share), 1, body->stats.hp));
    for (const KnownMove& known : lead.moves){
        battler.moves.push_back(SimMove{.move = known.move, .pp = known.pp});
    }
    return battler;
}

std::optional<SimBattler> trainer_battler(const TrainerMon& mon){
    std::optional<Combatant> body = combatant(mon.species, mon.level, Genes{.iv = trainer_iv(mon.iv), .ev = 0, .nature = mon.nature});
    if (!body){
        return std::nullopt;
    }
    SimBattler battler{.species = mon.species, .body = *body, .hp = body->stats.hp, .moves = {}};
    for (const std::string& move : mon.moves){
        battler.moves.push_back(SimMove{.move = move, .pp = data_of(move).pp});
    }
    return battler;
}

std::optional<SimTrainer> sim_trainer(std::string_view trainer){
    const std::optional<TrainerData> data = trainer_data(trainer);
    if (!data || data->party.empty()){
        return std::nullopt;
    }
    SimTrainer sim{
        .party = {}, .items = data->items,
        .check_bad_move = std::ranges::find(data->ai, "check-bad-move") != data->ai.end(),
        .try_to_faint = std::ranges::find(data->ai, "try-to-faint") != data->ai.end(),
        .check_viability = std::ranges::find(data->ai, "check-viability") != data->ai.end(),
    };
    for (const TrainerMon& mon : data->party){
        std::optional<SimBattler> battler = trainer_battler(mon);
        if (!battler){
            return std::nullopt;
        }
        sim.party.push_back(std::move(*battler));
    }
    return sim;
}

uint16_t ai_item_hp(std::string_view item){
    static const std::pair<std::string_view, uint16_t> HEALS[] = {
        {"potion", 20}, {"super-potion", 50}, {"hyper-potion", 200}, {"max-potion", 255}, {"full-restore", 255},
    };
    for (const auto& [name, hp] : HEALS){
        if (name == item){
            return hp;
        }
    }
    return 0;
}

std::optional<size_t> ai_item_choice(const SimBattler& foe, const std::vector<std::string>& items, size_t foes_left){
    const uint16_t max_hp = foe.body.stats.hp;
    for (size_t i = 0; i < items.size(); i++){
        if (i > 0 && foes_left > items.size() - i + 1){
            continue;
        }
        const uint16_t heal = ai_item_hp(items[i]);
        if (heal == 0 || foe.hp == 0){
            continue;
        }
        const bool low = foe.hp < max_hp / 4;
        if (low || (items[i] != "full-restore" && max_hp - foe.hp > heal)){
            return i;
        }
    }
    return std::nullopt;
}

std::vector<int> ai_move_scores(const SimTrainer& trainer, const SimBattler& foe, const SimBattler& target, std::mt19937& rng){
    std::vector<int> scores;
    for (const SimMove& known : foe.moves){
        const MoveData& move = data_of(known.move);
        const bool bad = trainer.check_bad_move && bad_move(move, foe, target);
        const int viable = trainer.check_viability ? viability(move, foe, target, rng) : 0;
        scores.push_back(known.pp == 0 ? 0 : START_SCORE - (bad ? BAD_MOVE : 0) + viable);
    }
    if (trainer.try_to_faint){
        add_try_to_faint(scores, foe, target);
    }
    return scores;
}

size_t ai_next_foe(const std::vector<SimBattler>& party, size_t fainted, const SimBattler& lead){
    std::vector<size_t> left;
    for (size_t slot = 0; slot < party.size(); slot++){
        if (slot != fainted && party[slot].hp > 0){
            left.push_back(slot);
        }
    }
    if (left.empty()){
        return fainted;
    }
    if (const std::optional<size_t> slot = suitable_by_type(left, party, lead)){
        return *slot;
    }
    if (const std::optional<size_t> slot = suitable_by_damage(left, party, party[fainted], lead)){
        return *slot;
    }
    return left.front();
}

std::optional<size_t> best_damage_slot(const SimBattler& own, const SimBattler& foe){
    std::optional<size_t> best;
    double best_damage = 0;
    for (size_t slot : damaging_slots(own, foe)){
        const double damage = mean_per_turn(own, foe, data_of(own.moves[slot].move));
        if (damage > best_damage){
            best = slot;
            best_damage = damage;
        }
    }
    return best;
}

std::optional<size_t> planned_attack_slot(
    const SimBattler& own, const SimBattler& foe,
    const std::vector<std::string>& items, size_t foes_left
){
    using Rank = std::tuple<size_t, int, uint8_t, int>;
    std::optional<size_t> best;
    Rank best_rank{};
    for (size_t slot : damaging_slots(own, foe)){
        const MoveData& move = data_of(own.moves[slot].move);
        const bool faints = lowest_roll(own, foe, move) >= foe.hp;
        const int sure = move.accuracy == 0 ? 100 : move.accuracy;
        //  Fewest turns, then surest, then weakest, then most PP left.
        const size_t turns = faints ? 1 : turns_to_faint(foe, items, foes_left, mean_per_turn(own, foe, move));
        const Rank rank{turns, -(faints ? sure : 0), move.power, -own.moves[slot].pp};
        if (!best || rank < best_rank){
            best = slot;
            best_rank = rank;
        }
    }
    return best;
}

uint32_t foe_max_hit(const SimBattler& foe, const SimBattler& own){
    uint32_t most = 0;
    for (const SimMove& known : foe.moves){
        const MoveData& move = data_of(known.move);
        if (known.pp > 0){
            most = std::max(most, damage_range(foe.body, own.body, move, false).max * hits_of(move));
        }
    }
    return most;
}

PotionCase potion_case(const SimBattler& own, const SimBattler& foe, double seen_hit){
    double expected = 0;
    for (const SimMove& known : foe.moves){
        if (known.pp > 0){
            expected = std::max(expected, mean_per_turn(foe, own, data_of(known.move)));
        }
    }
    const std::optional<size_t> slot = planned_attack_slot(own, foe, {}, 1);
    const size_t turns = slot
        ? turns_to_faint(foe, {}, 1, mean_per_turn(own, foe, data_of(own.moves[*slot].move)))
        : TURNS_CAP + 1;
    //  A faster foe hits before each of the lead's attacks, a slower one
    //  only between them.
    const size_t hits = speed_of(foe) >= speed_of(own) ? turns : turns - 1;
    return PotionCase{
        .hp = own.hp,
        .max_hp = own.body.stats.hp,
        .expected_hit = std::max(expected, seen_hit),
        .max_hit = std::max(foe_max_hit(foe, own), static_cast<uint32_t>(std::ceil(seen_hit))),
        .hits_to_ko = hits,
    };
}

bool potion_pays(const PotionCase& c){
    const uint16_t heal = static_cast<uint16_t>(std::min<int>(POTION_HP, c.max_hp - c.hp));
    if (heal == 0){
        return false;
    }
    const double healed = c.hp + heal;
    if (heal > c.expected_hit && healed > c.expected_hit){
        return true;
    }
    const bool lasts_if_healed = healed > c.expected_hit * static_cast<double>(c.hits_to_ko + 1);
    const bool lasts_now = c.hp > c.expected_hit * static_cast<double>(c.hits_to_ko);
    return lasts_if_healed && !lasts_now;
}

bool potion_now(const SimBattler& own, const SimBattler& foe, int potions, double seen_hit){
    const uint16_t max_hp = own.body.stats.hp;
    if (potions <= 0 || own.hp >= max_hp){
        return false;
    }
    const PotionCase c = potion_case(own, foe, seen_hit);
    const uint32_t healed = std::min<uint32_t>(max_hp, own.hp + POTION_HP);
    return c.max_hit >= own.hp && healed > c.max_hit && potion_pays(c);
}

OwnTurn own_turn(
    const SimBattler& own, int potions, const SimBattler& foe,
    const std::vector<std::string>& items, size_t foes_left
){
    const std::optional<size_t> slot = planned_attack_slot(own, foe, items, foes_left);
    if (slot){
        const MoveData& move = data_of(own.moves[*slot].move);
        const bool foe_priority = std::ranges::any_of(foe.moves, [](const SimMove& known){
            return known.pp > 0 && data_of(known.move).priority > 0;
        });
        const bool first = speed_of(own) > speed_of(foe) && !foe_priority && move.priority >= 0;
        const bool sure = move.accuracy == 0 || move.accuracy >= 100;
        if (first && sure && lowest_roll(own, foe, move) >= foe.hp && !ai_item_choice(foe, items, foes_left)){
            return OwnTurn{.potion = false, .slot = slot};
        }
    }
    if (potion_now(own, foe, potions)){
        return OwnTurn{.potion = true, .slot = std::nullopt};
    }
    return OwnTurn{.potion = false, .slot = slot};
}



}
}
}

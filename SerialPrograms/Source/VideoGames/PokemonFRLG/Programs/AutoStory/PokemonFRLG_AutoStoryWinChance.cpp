/*  AutoStory Win Chance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <random>
#include <utility>
#include <vector>
#include "PokemonFRLG_AutoStoryWinChance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A battle still going after this many turns counts as lost.
constexpr size_t MAX_TURNS = 100;
constexpr int MAX_STAGE = 6;

const MoveData& data_of(std::string_view move){
    static const MoveData NONE;
    const MoveData* data = move_data(move);
    return data == nullptr ? NONE : *data;
}

int8_t& stage(SimBattler& battler, Stage which){
    return battler.body.stages[static_cast<size_t>(which)];
}

uint16_t eighth(const SimBattler& battler){
    return std::max<uint16_t>(battler.body.stats.hp / 8, 1);
}

void lose_hp(SimBattler& battler, uint32_t damage){
    battler.hp = static_cast<uint16_t>(battler.hp > damage ? battler.hp - damage : 0);
}

void gain_hp(SimBattler& battler, uint32_t heal){
    battler.hp = static_cast<uint16_t>(std::min<uint32_t>(battler.body.stats.hp, battler.hp + heal));
}

//  "attack-down", "speed-up-2", "defense-down-hit": the stage, by how much
//  and on whom. None for another effect.
struct StageChange{
    Stage stat = Stage::ATTACK;
    int amount = 0;
    bool on_user = false;
};
std::optional<StageChange> stage_change(std::string_view effect){
    static const std::pair<std::string_view, Stage> STATS[] = {
        {"attack-", Stage::ATTACK}, {"defense-", Stage::DEFENSE}, {"speed-", Stage::SPEED},
        {"special-attack-", Stage::SP_ATTACK}, {"special-defense-", Stage::SP_DEFENSE},
        {"accuracy-", Stage::ACCURACY}, {"evasion-", Stage::EVASION},
    };
    for (const auto& [prefix, stat] : STATS){
        if (!effect.starts_with(prefix)){
            continue;
        }
        std::string_view rest = effect.substr(prefix.size());
        if (rest.ends_with("-hit")){
            rest.remove_suffix(4);
        }
        const bool up = rest.starts_with("up");
        if (!up && !rest.starts_with("down")){
            return std::nullopt;
        }
        const int amount = rest.ends_with("-2") ? 2 : 1;
        return StageChange{.stat = stat, .amount = up ? amount : -amount, .on_user = up};
    }
    return std::nullopt;
}

void change_stage(SimBattler& user, SimBattler& target, std::string_view effect){
    if (const std::optional<StageChange> change = stage_change(effect)){
        int8_t& now = stage(change->on_user ? user : target, change->stat);
        now = static_cast<int8_t>(std::clamp(now + change->amount, -MAX_STAGE, MAX_STAGE));
    }
}


//  One battle, played once.
class SimBattle{
public:
    SimBattle(const SimBattler& own, const SimItems& items, const SimTrainer& trainer, std::mt19937& rng)
        : m_own(own)
        , m_items(items)
        , m_trainer(trainer)
        , m_rng(rng)
    {}

    bool won(){
        for (size_t turn = 0; turn < MAX_TURNS; turn++){
            play_turn();
            if (m_own.hp == 0){
                return false;
            }
            if (foe().hp == 0 && !send_next()){
                return true;
            }
        }
        return false;
    }

    //  After a lost battle: the foe out and the lead's state.
    void tally_loss(LossTally& tally){
        tally.by_foe[foe().species]++;
        tally.seeded += m_own.seeded ? 1 : 0;
        tally.poisoned += m_own.status == SimStatus::POISON ? 1 : 0;
        tally.asleep += m_own.status == SimStatus::SLEEP ? 1 : 0;
        tally.paralyzed += m_own.status == SimStatus::PARALYSIS ? 1 : 0;
        tally.accuracy_down += stage(m_own, Stage::ACCURACY) < 0 ? 1 : 0;
        tally.no_potions += m_items.potions == 0 ? 1 : 0;
        tally.turn_cap += m_own.hp > 0 ? 1 : 0;
    }

private:
    SimBattler& foe(){
        return m_trainer.party[m_out];
    }

    size_t foes_left() const{
        return static_cast<size_t>(std::ranges::count_if(m_trainer.party, [](const SimBattler& battler){
            return battler.hp > 0;
        }));
    }

    //  The trainer's Pokemon not out and not fainted.
    std::vector<SimBattler> to_come() const{
        std::vector<SimBattler> rest;
        for (size_t slot = 0; slot < m_trainer.party.size(); slot++){
            if (slot != m_out && m_trainer.party[slot].hp > 0){
                rest.push_back(m_trainer.party[slot]);
            }
        }
        return rest;
    }

    bool send_next(){
        if (foes_left() == 0){
            return false;
        }
        m_out = ai_next_foe(m_trainer.party, m_out, m_own);
        return true;
    }

    uint32_t random(uint32_t n){
        return static_cast<uint32_t>(m_rng() % n);
    }

    //  The AI's move: the best score, ties at random. None with no PP left.
    std::optional<size_t> foe_move(){
        const std::vector<int> scores = ai_move_scores(m_trainer, foe(), m_own, m_rng);
        const int best = scores.empty() ? 0 : *std::ranges::max_element(scores);
        std::vector<size_t> ties;
        for (size_t slot = 0; slot < scores.size(); slot++){
            if (best > 0 && scores[slot] == best){
                ties.push_back(slot);
            }
        }
        if (ties.empty()){
            return std::nullopt;
        }
        return ties[random(static_cast<uint32_t>(ties.size()))];
    }

    void play_turn(){
        const size_t left = foes_left();
        m_psn_turns = m_own.status == SimStatus::POISON ? m_psn_turns + 1 : 0;
        //  A charged two-turn move strikes: no choice this turn.
        const bool own_locked = !m_own.charging.empty();
        const bool foe_locked = !foe().charging.empty();
        const OwnTurn own = own_locked ? OwnTurn{} : own_turn(m_own, m_items, foe(), m_trainer, left, to_come());
        const bool antidote = !own_locked && !own.potion && !own.awakening && antidote_now();
        const std::optional<size_t> item = foe_locked ? std::nullopt : ai_item_choice(foe(), m_trainer.items, left);
        const std::optional<size_t> foe_slot = item || foe_locked ? std::nullopt : foe_move();
        if (own.potion){
            gain_hp(m_own, POTION_HP);
            m_items.potions--;
        }
        if (own.awakening){
            m_own.status = SimStatus::NONE;
            m_own.sleep = 0;
            m_items.awakenings--;
        }
        if (antidote){
            m_own.status = SimStatus::NONE;
            m_items.antidotes--;
            m_antidote_used = true;
        }
        if (item){
            gain_hp(foe(), ai_item_hp(m_trainer.items[*item]));
            m_trainer.items[*item].clear();
        }
        const bool item_used = own.potion || own.awakening || antidote;
        const std::string own_move = own_locked ? m_own.charging
            : item_used ? "" : own.slot ? m_own.moves[*own.slot].move : "struggle";
        const std::string foe_used = foe_locked ? foe().charging
            : item ? "" : foe_slot ? foe().moves[*foe_slot].move : "struggle";
        play_moves(own_move, own.slot, foe_used, foe_slot);
        end_of_turn();
    }

    //  As the battle loop: once a battle, PSN_TURNS_BEFORE_ANTIDOTE turns
    //  poisoned in a row.
    bool antidote_now() const{
        return m_items.antidotes > 0 && !m_antidote_used && m_psn_turns >= PSN_TURNS_BEFORE_ANTIDOTE;
    }

    bool own_first(const std::string& own_move, const std::string& foe_used){
        const int own_priority = data_of(own_move).priority;
        const int foe_priority = data_of(foe_used).priority;
        if (own_priority != foe_priority){
            return own_priority > foe_priority;
        }
        const uint32_t own_speed = speed(m_own);
        const uint32_t foe_speed = speed(foe());
        return own_speed != foe_speed ? own_speed > foe_speed : random(2) == 0;
    }

    void play_moves(const std::string& own_move, std::optional<size_t> own_slot, const std::string& foe_used, std::optional<size_t> foe_slot){
        m_flinched = false;
        const bool first = own_move.empty() || foe_used.empty() || own_first(own_move, foe_used);
        if (first){
            use(m_own, foe(), own_move, own_slot, true);
            use(foe(), m_own, foe_used, foe_slot, false);
        }else{
            use(foe(), m_own, foe_used, foe_slot, true);
            use(m_own, foe(), own_move, own_slot, false);
        }
    }

    static uint32_t speed(const SimBattler& battler){
        const uint32_t speed = staged_stat(battler.body.stats.speed, battler.body.stages[static_cast<size_t>(Stage::SPEED)]);
        return battler.status == SimStatus::PARALYSIS ? speed / 4 : speed;
    }

    //  Whether `user` gets to move: sleep, flinch, paralysis.
    bool can_move(SimBattler& user, bool moves_first){
        if (user.status == SimStatus::SLEEP){
            if (--user.sleep > 0){
                return false;
            }
            user.status = SimStatus::NONE;
        }
        if (!moves_first && m_flinched){
            return false;
        }
        return user.status != SimStatus::PARALYSIS || random(4) != 0;
    }

    bool hits(const SimBattler& user, const SimBattler& target, const MoveData& move){
        return move.accuracy == 0 || random(100) + 1 <= hit_percent(user, target, move);
    }

    void use(SimBattler& user, SimBattler& target, const std::string& move_name, std::optional<size_t> slot, bool moves_first){
        if (move_name.empty() || user.hp == 0 || target.hp == 0){
            return;
        }
        if (!can_move(user, moves_first)){
            user.charging.clear();
            return;
        }
        if (slot && user.moves[*slot].pp > 0){
            user.moves[*slot].pp--;
        }
        const MoveData& move = data_of(move_name);
        if (two_turn(move) && user.charging.empty()){
            user.charging = move_name;
            return;
        }
        user.charging.clear();
        if (semi_invulnerable(target) && !on_user(move)){
            return;
        }
        if (!hits(user, target, move)){
            return;
        }
        if (move.power > 0){
            strike(user, target, move, moves_first);
        }else{
            apply_status_move(user, target, move);
        }
    }

    //  A move on its user only: a stat raise, Recover.
    static bool on_user(const MoveData& move){
        if (move.power > 0){
            return false;
        }
        if (move.effect == "restore-hp" || move.effect == "softboiled"){
            return true;
        }
        const std::optional<StageChange> change = stage_change(move.effect);
        return change && change->on_user;
    }

    uint32_t hit_count(const MoveData& move){
        if (move.effect == "double-hit" || move.effect == "twineedle"){
            return 2;
        }
        if (move.effect != "multi-hit"){
            return 1;
        }
        //  Cmd_setmultihitcounter: 2 or 3 at 3/8 each, 4 or 5 at 1/8 each.
        const uint32_t first = random(4);
        return first > 1 ? random(4) + 2 : first + 2;
    }

    uint32_t one_hit(const SimBattler& user, const SimBattler& target, const MoveData& move){
        if (move.effect == "super-fang"){
            return std::max<uint32_t>(target.hp / 2, 1);
        }
        const bool crit = random(crit_odds(move)) == 0;
        const uint32_t before = damage_before_roll(user.body, target.body, move, crit);
        return fixed_damage(move, user.body) ? before : rolled_damage(before, 100 - random(16));
    }

    void strike(SimBattler& user, SimBattler& target, const MoveData& move, bool moves_first){
        const uint32_t count = hit_count(move);
        uint32_t dealt = 0;
        for (uint32_t i = 0; i < count && target.hp > 0; i++){
            const uint32_t damage = std::min<uint32_t>(one_hit(user, target, move), target.hp);
            lose_hp(target, damage);
            dealt += damage;
        }
        if (move.effect == "absorb"){
            gain_hp(user, std::max<uint32_t>(dealt / 2, 1));
        }
        if (dealt > 0 && target.hp > 0 && random(100) < move.effect_chance){
            secondary(user, target, move, moves_first);
        }
    }

    void secondary(SimBattler& user, SimBattler& target, const MoveData& move, bool moves_first){
        if (move.effect == "flinch-hit" || move.effect == "flinch-minimize-hit"){
            m_flinched = moves_first;
        }else if (move.effect == "poison-hit"){
            set_status(target, SimStatus::POISON);
        }else if (move.effect == "paralyze-hit"){
            set_status(target, SimStatus::PARALYSIS);
        }else{
            change_stage(user, target, move.effect);
        }
    }

    void set_status(SimBattler& target, SimStatus status){
        if (target.status != SimStatus::NONE){
            return;
        }
        const bool poison_proof = target.body.first == PokemonType::POISON || target.body.second == PokemonType::POISON
            || target.body.first == PokemonType::STEEL || target.body.second == PokemonType::STEEL;
        if (status == SimStatus::POISON && poison_proof){
            return;
        }
        target.status = status;
        if (status == SimStatus::SLEEP){
            target.sleep = static_cast<uint8_t>(2 + random(4));
        }
    }

    void apply_status_move(SimBattler& user, SimBattler& target, const MoveData& move){
        if (move.effect == "sleep"){
            set_status(target, SimStatus::SLEEP);
        }else if (move.effect == "poison" || move.effect == "toxic"){
            set_status(target, SimStatus::POISON);
        }else if (move.effect == "paralyze"){
            set_status(target, SimStatus::PARALYSIS);
        }else if (move.effect == "leech-seed"){
            const bool grass = target.body.first == PokemonType::GRASS || target.body.second == PokemonType::GRASS;
            target.seeded = target.seeded || !grass;
        }else if (move.effect == "restore-hp" || move.effect == "softboiled"){
            gain_hp(user, user.body.stats.hp / 2);
        }else{
            change_stage(user, target, move.effect);
        }
    }

    void end_of_turn(){
        for (SimBattler* battler : {&m_own, &foe()}){
            if (battler->hp > 0 && battler->status == SimStatus::POISON){
                lose_hp(*battler, eighth(*battler));
            }
        }
        seed(m_own, foe());
        seed(foe(), m_own);
    }

    static void seed(SimBattler& seeded, SimBattler& other){
        if (!seeded.seeded || seeded.hp == 0 || other.hp == 0){
            return;
        }
        const uint16_t drained = std::min(eighth(seeded), seeded.hp);
        lose_hp(seeded, drained);
        gain_hp(other, drained);
    }

    SimBattler m_own;
    SimItems m_items;
    SimTrainer m_trainer;
    std::mt19937& m_rng;
    size_t m_out = 0;
    bool m_flinched = false;
    int m_psn_turns = 0;
    bool m_antidote_used = false;
};

} // namespace


WinEstimate estimate_win_chance(const SimBattler& own, const SimItems& items, const SimTrainer& trainer, size_t battles, uint32_t seed){
    std::mt19937 rng(seed);
    WinEstimate estimate{.battles = battles, .wins = 0, .chance = 0};
    for (size_t i = 0; i < battles; i++){
        SimBattle battle(own, items, trainer, rng);
        if (battle.won()){
            estimate.wins++;
        }else{
            battle.tally_loss(estimate.losses);
        }
    }
    estimate.chance = battles == 0 ? 0 : static_cast<double>(estimate.wins) / battles;
    return estimate;
}

std::string loss_text(const WinEstimate& estimate){
    const LossTally& t = estimate.losses;
    std::string text = std::format("lost {}:", estimate.battles - estimate.wins);
    std::vector<std::pair<std::string, size_t>> foes(t.by_foe.begin(), t.by_foe.end());
    std::ranges::sort(foes, [](const auto& a, const auto& b){ return a.second > b.second; });
    for (size_t i = 0; i < foes.size(); i++){
        text += std::format("{} {} {}", i == 0 ? "" : ",", foes[i].first, foes[i].second);
    }
    const std::pair<std::string_view, size_t> states[] = {
        {"seeded", t.seeded}, {"poisoned", t.poisoned}, {"asleep", t.asleep}, {"paralyzed", t.paralyzed},
        {"accuracy down", t.accuracy_down}, {"no Potions", t.no_potions}, {"turn cap", t.turn_cap},
    };
    const char* separator = ";";
    for (const auto& [name, count] : states){
        if (count > 0){
            text += std::format("{} {} {}", separator, name, count);
            separator = ",";
        }
    }
    return text;
}

std::optional<WinEstimate> story_win_chance(const PartyMember& lead, const SimItems& items, double hp_share, std::string_view trainer){
    const std::optional<SimBattler> own = own_battler(lead, hp_share);
    const std::optional<SimTrainer> sim = sim_trainer(trainer);
    if (!own || !sim){
        return std::nullopt;
    }
    WinEstimate estimate = estimate_win_chance(*own, items, *sim);
    estimate.inputs = std::format("{} Lv{} {}/{} HP:", own->species, own->body.level, own->hp, own->body.stats.hp);
    for (size_t slot = 0; slot < own->moves.size(); slot++){
        estimate.inputs += std::format(
            "{} {} {}/{}", slot == 0 ? "" : ",", own->moves[slot].move, own->moves[slot].pp, max_pp(own->moves[slot].move)
        );
    }
    estimate.inputs += std::format("; {} Potions", items.potions);
    if (items.awakenings > 0){
        estimate.inputs += std::format(", {} Awakenings", items.awakenings);
    }
    if (items.antidotes > 0){
        estimate.inputs += std::format(", {} Antidotes", items.antidotes);
    }
    return estimate;
}

std::optional<std::string> win_chance_refusal(const WinEstimate& estimate, double minimum){
    if (estimate.chance >= minimum){
        return std::nullopt;
    }
    return std::format(
        "win chance {:.0f}% < {:.0f}%{}: grind first",
        std::floor(100 * estimate.chance), std::round(100 * minimum),
        estimate.inputs.empty() ? "" : " with " + estimate.inputs
    );
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

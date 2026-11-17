/*  AutoStory Planned Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include "PokemonFRLG_AutoStoryGen3Damage.h"
#include "PokemonFRLG_AutoStoryMoveData.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::PokemonType;

//  Guard Spec's Mist and Sunny Day's sun last 5 turns (pret).
constexpr int MIST_TURNS = 5;
constexpr int SUN_TURNS = 5;
//  The quantiles a multi-hit move's total is read as (the sim's damage.cjs).
constexpr int HIT_QUANTILES = 32;

//  The League's items as the policy counts them.
constexpr std::string_view LEAGUE_ITEMS[] = {
    "full-restore", "max-potion", "hyper-potion", "super-potion", "full-heal", "parlyz-heal", "antidote",
    "awakening", "burn-heal", "ice-heal", "elixir", "max-elixir", "x-special", "x-speed", "x-accuracy",
    "x-attack", "x-defend", "guard-spec",
};

//  One side of a hit: its stats as the battle uses them (badges applied).
struct Side{
    int level = 1;
    Stats stats = {};
    SpeciesData types = {};
    std::string ability = {};
};

double effectiveness(PokemonType attack, PokemonType defend){
    return static_cast<double>(type_effectiveness(attack, defend)) / 10.0;
}

bool ability_blocks(std::string_view ability, PokemonType type){
    return (ability == "waterabsorb" && type == PokemonType::WATER)
        || (ability == "voltabsorb" && type == PokemonType::ELECTRIC)
        || (ability == "flashfire" && type == PokemonType::FIRE)
        || (ability == "levitate" && type == PokemonType::GROUND);
}

//  A multi-hit move's total, as HIT_QUANTILES equally likely values:
//  2 and 3 hits 3/8 each, 4 and 5 hits 1/8 each (a fixed count: all of it).
std::vector<int> multi_hit_rolls(const std::vector<int>& per_hit, std::pair<int, int> hits){
    const std::vector<std::pair<int, double>> weights = hits.first == hits.second
        ? std::vector<std::pair<int, double>>{{hits.first, 1.0}}
        : std::vector<std::pair<int, double>>{{2, 3.0 / 8}, {3, 3.0 / 8}, {4, 1.0 / 8}, {5, 1.0 / 8}};
    std::map<int, double> total;
    for (const auto& [count, weight] : weights){
        std::map<int, double> sums{{0, 1.0}};
        for (int hit = 0; hit < count; hit++){
            std::map<int, double> next;
            for (const auto& [sum, chance] : sums){
                for (int damage : per_hit){
                    next[sum + damage] += chance / static_cast<double>(per_hit.size());
                }
            }
            sums = std::move(next);
        }
        for (const auto& [sum, chance] : sums){
            total[sum] += chance * weight;
        }
    }
    std::vector<int> rolls;
    for (int i = 0; i < HIT_QUANTILES; i++){
        double cumulative = 0;
        int value = total.rbegin()->first;
        for (const auto& [sum, chance] : total){
            cumulative += chance;
            if (cumulative >= (i + 0.5) / HIT_QUANTILES){
                value = sum;
                break;
            }
        }
        rolls.push_back(value);
    }
    return rolls;
}

//  `move`'s damage rolls from `attacker` on `defender`, with the
//  attacker's offensive stage; empty for none.
std::vector<int> damage_rolls(const LeagueMoveData& move, const Side& attacker, const Side& defender, int stage){
    const double first = effectiveness(move.type, defender.types.first);
    const double second = defender.types.second == defender.types.first ? 1.0 : effectiveness(move.type, defender.types.second);
    if (first * second == 0 || ability_blocks(defender.ability, move.type)){
        return {};
    }
    if (move.fixed_damage){
        return {*move.fixed_damage};
    }
    int attack = move.physical ? attacker.stats.attack : attacker.stats.sp_attack;
    //  Thick Fat halves the attacker's stat for Fire and Ice.
    if (defender.ability == "thickfat" && (move.type == PokemonType::FIRE || move.type == PokemonType::ICE)){
        attack /= 2;
    }
    const std::vector<int> rolls = gen3_damage_rolls(Gen3Hit{
        .level = attacker.level,
        .attack = attack,
        .defense = move.physical ? defender.stats.defense : defender.stats.sp_defense,
        .power = move.power,
        .attack_stage = stage,
        .stab = move.type == attacker.types.first || move.type == attacker.types.second,
        .effectiveness_first = first,
        .effectiveness_second = second,
    });
    return move.hits && !rolls.empty() ? multi_hit_rolls(rolls, *move.hits) : rolls;
}

Stats boosted_stats(const Stats& stats){
    return Stats{
        .hp = stats.hp,
        .attack = static_cast<uint16_t>(badge_boosted(stats.attack)),
        .defense = static_cast<uint16_t>(badge_boosted(stats.defense)),
        .sp_attack = static_cast<uint16_t>(badge_boosted(stats.sp_attack)),
        .sp_defense = static_cast<uint16_t>(badge_boosted(stats.sp_defense)),
        .speed = static_cast<uint16_t>(badge_boosted(stats.speed)),
    };
}

SpeciesData types_of(std::string_view species){
    const SpeciesData* data = species_data(species);
    return data != nullptr ? *data : SpeciesData{};
}

PlannedStatus planned_status(StatusCondition status){
    switch (status){
    case StatusCondition::POISON:
        return PlannedStatus::POISON;
    case StatusCondition::PARALYSIS:
        return PlannedStatus::PARALYSIS;
    case StatusCondition::SLEEP:
        return PlannedStatus::SLEEP;
    case StatusCondition::OTHER:
        return PlannedStatus::BURN;
    case StatusCondition::NONE:
        break;
    }
    return PlannedStatus::NONE;
}

//  The attacker's stages a move reads.
struct Offense{
    int attack = 0;
    int spa = 0;
    int accuracy = 0;
};

PlannedMove planned_move(std::string_view id, int pp, const LeagueFoesData& data, const std::pair<Side, Side>& sides, const Offense& offense){
    PlannedMove planned{.id = std::string(id), .pp = pp};
    const auto found = data.moves.find(id);
    if (found == data.moves.end()){
        return planned;
    }
    const LeagueMoveData& move = found->second;
    planned.accuracy = move.accuracy ? hit_chance(*move.accuracy, offense.accuracy, 0) : 1.0;
    planned.damage = damage_rolls(move, sides.first, sides.second, move.physical ? offense.attack : offense.spa);
    planned.priority = move.priority;
    planned.two_turn = move.two_turn;
    return planned;
}

PlannedOwn planned_own(const PlannedInputs& inputs, const std::pair<Side, Side>& sides, const LeagueFoesData& data){
    const PlannedStages& stages = inputs.stages;
    const PlannedStatus status = planned_status(inputs.status);
    PlannedOwn own{
        .hp = inputs.hp,
        .max_hp = sides.first.stats.hp,
        .status = status,
        .speed = battle_speed(sides.first.stats.speed, stages.speed, status == PlannedStatus::PARALYSIS),
        .spa_stage = stages.spa,
        .speed_stage = stages.speed,
        .accuracy_stage = stages.accuracy,
        .attack_stage = stages.attack,
        .mist = stages.mist_turns > 0,
    };
    const Offense offense{.attack = stages.attack, .spa = stages.spa, .accuracy = stages.accuracy};
    for (const KnownMove& known : inputs.lead.moves){
        own.moves.push_back(planned_move(known.move, known.pp, data, sides, offense));
    }
    return own;
}

PlannedFoe planned_foe(const PlannedFoeTrack& track, const std::pair<Side, Side>& sides, const LeagueFoesData& data){
    const int max_hp = track.foe.stats.hp;
    const int hp = track.fill <= 0 ? 0 : std::max(1, static_cast<int>(std::lround(track.fill * max_hp)));
    PlannedFoe planned{
        .species = track.foe.species,
        .slot = track.slot,
        .hp = hp,
        .max_hp = max_hp,
        .speed = battle_speed(track.foe.stats.speed, 0, false),
        .recharging = track.recharging,
        .charging = track.charging,
        .sun = track.sun,
    };
    for (const std::string& move : track.foe.moves){
        planned.moves.push_back(planned_move(move, max_pp(move), data, {sides.second, sides.first}, Offense{}));
    }
    return planned;
}

bool has(const std::string& letters, std::string_view word){
    return letters.find(word) != std::string::npos;
}

} // namespace


PlannedState planned_state(const PlannedInputs& inputs, const LeagueFoesData& data){
    const Stats raw = inputs.lead.stats.value_or(Stats{});
    const Side us{
        .level = static_cast<int>(battle_level(inputs.lead)),
        .stats = boosted_stats(raw),
        .types = types_of(inputs.lead.species),
    };
    const PlannedFoeTrack& track = inputs.foe;
    const Side foe{.level = track.foe.level, .stats = track.foe.stats, .types = types_of(track.foe.species), .ability = track.foe.ability};
    return PlannedState{
        .own = planned_own(inputs, {us, foe}, data),
        .foe = planned_foe(track, {us, foe}, data),
        .foe_party_alive = track.party_alive,
        .trainer_item_open = trainer_item_open(track.full_restores_carried, track.full_restores_used, track.party_alive),
        .bag = inputs.bag,
    };
}


PlannedBattle::PlannedBattle(const LeagueFightData& fight, const LeagueTrainerData& trainer)
    : m_fight(&fight)
    , m_trainer(&trainer)
{}

void PlannedBattle::read_texts(const std::vector<std::string>& texts){
    //  The texts of the turn since the last choice: what the foe did.
    m_recharging = false;
    m_charging.reset();
    bool restored = false;
    for (const std::string& letters : texts){
        //  pokefirered: "{trainer} used\nFULL RESTORE!"; the player's own
        //  medicine shows no such text.
        restored = restored || has(letters, "usedfullrestore");
        m_recharging = m_recharging || has(letters, "usedhyperbeam");
        if (has(letters, "took") && has(letters, "sunlight")){
            m_charging = "solar-beam";
        }else if (has(letters, "underwater")){
            m_charging = "dive";
        }else if (has(letters, "sunlight") && has(letters, "bright")){
            m_sun_turns = SUN_TURNS;
        }else if (has(letters, "sunlight") && has(letters, "faded")){
            m_sun_turns = 0;
        }
    }
    if (restored){
        m_full_restores_used++;
    }
}

PlannedFoeTrack PlannedBattle::foe_out(const Supplies& supplies, const PolicyView& view){
    const std::vector<LeagueFoeData>& party = m_trainer->party;
    size_t slot = std::min(view.foe_index, party.empty() ? 0 : party.size() - 1);
    for (size_t index = slot; index < party.size() && !supplies.foe_out.empty(); index++){
        if (party[index].species == supplies.foe_out){
            slot = index;
            break;
        }
    }
    const LeagueFoeData foe = party.empty() ? LeagueFoeData{} : party[slot];
    //  Intimidate as it comes out, unless Mist is up.
    if (m_last_slot != slot && foe.ability == "intimidate" && m_stages.mist_turns == 0){
        m_stages.attack = std::max(-6, m_stages.attack - 1);
    }
    m_last_slot = slot;
    const auto carried = m_trainer->items.find("full-restore");
    return PlannedFoeTrack{
        .foe = foe,
        .slot = slot,
        .fill = view.foe_fill,
        .party_alive = static_cast<int>(party.size() - slot),
        .full_restores_carried = carried == m_trainer->items.end() ? 0 : carried->second,
        .full_restores_used = m_full_restores_used,
        .recharging = m_recharging,
        .charging = m_charging,
        .sun = m_sun_turns > 0,
    };
}

PlannedBattle::Turn PlannedBattle::choose(const Supplies& supplies, const PolicyView& view){
    read_texts(view.battle_texts);
    PlannedInputs inputs{
        .lead = supplies.lead,
        .hp = static_cast<int>(std::lround(view.hp)),
        .status = view.status,
        .stages = m_stages,
        .foe = foe_out(supplies, view),
    };
    for (std::string_view item : LEAGUE_ITEMS){
        inputs.bag[std::string(item)] = bag_count(supplies, item);
    }
    const PlannedState state = planned_state(inputs, league_foes_data());
    const PlannedChoice choice = choose_planned(state, *m_fight, m_streak.streak(state));
    m_streak.chose(state, choice);
    const PlannedDanger danger = planned_danger(state);
    std::string what = choice.item ? *choice.item
        : choice.move_slot && *choice.move_slot < state.own.moves.size() ? state.own.moves[*choice.move_slot].id
        : std::string("best attack");
    std::string log = std::format(
        "{} HP {}/{}, {} {}/{} (slot {}, {} left), D {} mean {:.0f}, stages atk {} spa {} spe {} acc {} mist {}, "
        "trainer Full Restores used {}: {} ({})",
        m_fight->name, state.own.hp, state.own.max_hp, state.foe.species, state.foe.hp, state.foe.max_hp,
        state.foe.slot, state.foe_party_alive, danger.normal_max, danger.mean,
        m_stages.attack, m_stages.spa, m_stages.speed, m_stages.accuracy, m_stages.mist_turns,
        m_full_restores_used, choice.row, what
    );
    if (m_stages.mist_turns > 0){
        m_stages.mist_turns--;
    }
    if (m_sun_turns > 0){
        m_sun_turns--;
    }
    return Turn{.choice = choice, .log = std::move(log)};
}

void PlannedBattle::item_used(std::string_view item){
    if (item == "x-special"){
        m_stages.spa = std::min(6, m_stages.spa + 1);
    }else if (item == "x-speed"){
        m_stages.speed = std::min(6, m_stages.speed + 1);
    }else if (item == "x-accuracy"){
        m_stages.accuracy = std::min(6, m_stages.accuracy + 1);
    }else if (item == "x-attack"){
        m_stages.attack = std::min(6, m_stages.attack + 1);
    }else if (item == "guard-spec"){
        m_stages.mist_turns = MIST_TURNS;
    }
}

void PlannedBattle::lead_switched_in(){
    //  Mist is the side's and stays.
    m_stages = PlannedStages{.mist_turns = m_stages.mist_turns};
    m_last_slot.reset();
}

std::optional<PlannedBattle> planned_battle(std::string_view trainer){
    const std::optional<std::string> name = league_fight_name(trainer);
    if (!name){
        return std::nullopt;
    }
    const LeagueFightData* fight = league_fight_data(league_plan_data(), *name);
    const auto found = league_foes_data().trainers.find(trainer);
    if (fight == nullptr || found == league_foes_data().trainers.end()){
        return std::nullopt;
    }
    return PlannedBattle(*fight, found->second);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

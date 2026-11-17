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
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryMoveData.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"
#include "PokemonFRLG_AutoStoryPlannedRisk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::PokemonType;

//  The quantiles a multi-hit move's total is read as (the sim's damage.cjs).
constexpr int HIT_QUANTILES = 32;

//  The League's items as the policy counts them.
constexpr std::string_view LEAGUE_ITEMS[] = {
    "full-restore", "max-potion", "hyper-potion", "super-potion", "full-heal", "parlyz-heal", "antidote",
    "awakening", "burn-heal", "ice-heal", "elixir", "max-elixir", "x-special", "x-speed", "x-accuracy",
    "x-attack", "x-defend", "guard-spec",
};

//  One side of a hit: its stats as the battle uses them (badges applied),
//  its HP, stages and status.
struct Side{
    int level = 1;
    Stats stats = {};
    int hp = 0;
    SpeciesData types = {};
    std::string ability = {};
    StatStages stages = {};
    PlannedStatus status = PlannedStatus::NONE;
};

//  The pinch abilities of the species that have one as their only
//  ability (the starters' lines): our lead's, which the story doesn't read.
std::string pinch_ability(std::string_view species){
    static const std::map<std::string_view, std::string_view> ABILITIES{
        {"bulbasaur", "overgrow"}, {"ivysaur", "overgrow"}, {"venusaur", "overgrow"},
        {"charmander", "blaze"}, {"charmeleon", "blaze"}, {"charizard", "blaze"},
        {"squirtle", "torrent"}, {"wartortle", "torrent"}, {"blastoise", "torrent"},
    };
    const auto it = ABILITIES.find(species);
    return it == ABILITIES.end() ? std::string() : std::string(it->second);
}

//  Overgrow, Blaze, Torrent and Swarm: x1.5 power to their type at a
//  third of the HP or less (@smogon/calc calculateBasePowerADV, pret).
int base_power(const LeagueMoveData& move, const Side& attacker){
    const bool pinch = (attacker.ability == "overgrow" && move.type == PokemonType::GRASS)
        || (attacker.ability == "blaze" && move.type == PokemonType::FIRE)
        || (attacker.ability == "torrent" && move.type == PokemonType::WATER)
        || (attacker.ability == "swarm" && move.type == PokemonType::BUG);
    return pinch && attacker.hp * 3 <= attacker.stats.hp ? move.power * 3 / 2 : move.power;
}

//  The foe's HP as its bar's fill shows it.
int hp_of_fill(double fill, int max_hp){
    return fill <= 0 ? 0 : std::max(1, static_cast<int>(std::lround(fill * max_hp)));
}

//  The weather on a move of `type` (@smogon/calc calculateFinalModsADV).
double weather_modifier(FieldWeather weather, PokemonType type, bool solar_beam){
    const bool sun = weather == FieldWeather::SUN;
    const bool rain = weather == FieldWeather::RAIN;
    if ((sun && type == PokemonType::FIRE) || (rain && type == PokemonType::WATER)){
        return 1.5;
    }
    if ((sun && type == PokemonType::WATER) || (rain && type == PokemonType::FIRE) || (rain && solar_beam)){
        return 0.5;
    }
    return 1.0;
}

//  The attacker's Attack or Sp. Atk before its stage (@smogon/calc
//  calculateAttackADV): halved by the defender's Thick Fat on a Fire or
//  Ice hit, then x1.5 for Guts with a status on a physical one.
int attack_stat(const LeagueMoveData& move, const Side& attacker, const Side& defender){
    int attack = move.physical ? attacker.stats.attack : attacker.stats.sp_attack;
    if (defender.ability == "thickfat" && (move.type == PokemonType::FIRE || move.type == PokemonType::ICE)){
        attack /= 2;
    }
    if (move.physical && attacker.ability == "guts" && attacker.status != PlannedStatus::NONE){
        attack = attack * 3 / 2;
    }
    return attack;
}

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

//  `move` (`id`)'s damage rolls from `attacker` on `defender` in
//  `weather`; empty for none.
std::vector<int> damage_rolls(std::string_view id, const LeagueMoveData& move, const std::pair<Side, Side>& sides, FieldWeather weather){
    const auto& [attacker, defender] = sides;
    const double first = effectiveness(move.type, defender.types.first);
    const double second = defender.types.second == defender.types.first ? 1.0 : effectiveness(move.type, defender.types.second);
    if (first * second == 0 || ability_blocks(defender.ability, move.type)){
        return {};
    }
    if (move.fixed_damage){
        return {*move.fixed_damage};
    }
    const std::vector<int> rolls = gen3_damage_rolls(Gen3Hit{
        .level = attacker.level,
        .attack = attack_stat(move, attacker, defender),
        .defense = move.physical ? defender.stats.defense : defender.stats.sp_defense,
        .power = base_power(move, attacker),
        .attack_stage = move.physical ? attacker.stages.attack : attacker.stages.sp_attack,
        .defense_stage = move.physical ? defender.stages.defense : defender.stages.sp_defense,
        .stab = move.type == attacker.types.first || move.type == attacker.types.second,
        .effectiveness_first = first,
        .effectiveness_second = second,
        .burned_physical = move.physical && attacker.status == PlannedStatus::BURN && attacker.ability != "guts",
        .physical = move.physical,
        .weather = weather_modifier(weather, move.type, id == "solar-beam"),
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

//  `id` as the policy weighs it, from the first of `sides` on the second.
PlannedMove planned_move(std::string_view id, int pp, const LeagueFoesData& data, const std::pair<Side, Side>& sides, FieldWeather weather){
    PlannedMove planned{.id = std::string(id), .pp = pp};
    const auto found = data.moves.find(id);
    if (found == data.moves.end()){
        return planned;
    }
    const LeagueMoveData& move = found->second;
    const auto& [attacker, defender] = sides;
    planned.accuracy = move.accuracy ? hit_chance(*move.accuracy, attacker.stages.accuracy, defender.stages.evasion) : 1.0;
    planned.damage = damage_rolls(id, move, sides, weather);
    planned.priority = move.priority;
    planned.two_turn = move.two_turn;
    return planned;
}

PlannedOwn planned_own(const PlannedInputs& inputs, const std::pair<Side, Side>& sides, const LeagueFoesData& data){
    const BattleTrackState& track = inputs.track;
    const Side& us = sides.first;
    PlannedOwn own{
        .hp = inputs.hp,
        .max_hp = us.stats.hp,
        .status = us.status,
        .speed = battle_speed(us.stats.speed, track.own.speed, us.status == PlannedStatus::PARALYSIS),
        .spa_stage = track.own.sp_attack,
        .speed_stage = track.own.speed,
        .accuracy_stage = track.own.accuracy,
        .attack_stage = track.own.attack,
        .defense_stage = track.own.defense,
        .mist = track.mist_turns > 0,
    };
    for (const KnownMove& known : inputs.lead.moves){
        own.moves.push_back(planned_move(known.move, known.pp, data, sides, track.weather));
    }
    return own;
}

//  The foe's moves at full PP: the program can't see the foe's PP.
PlannedFoe planned_foe(const PlannedInputs& inputs, const std::pair<Side, Side>& sides, const LeagueFoesData& data){
    const BattleTrackState& track = inputs.track;
    const PlannedFoeTrack& foe = inputs.foe;
    const Side& them = sides.second;
    const int max_hp = foe.foe.stats.hp;
    const int hp = hp_of_fill(foe.fill, max_hp);
    PlannedFoe planned{
        .species = foe.foe.species,
        .slot = foe.slot,
        .hp = hp,
        .max_hp = max_hp,
        .speed = battle_speed(them.stats.speed, track.foe.speed, them.status == PlannedStatus::PARALYSIS),
        .recharging = track.foe_recharging,
        .sleeping = them.status == PlannedStatus::SLEEP && track.foe_sleep_turns > 1,
        .frozen = them.status == PlannedStatus::FREEZE,
        .charging = track.foe_charging,
        .sun = track.weather == FieldWeather::SUN,
    };
    for (const std::string& move : foe.foe.moves){
        planned.moves.push_back(planned_move(move, max_pp(move), data, {them, sides.first}, track.weather));
    }
    return planned;
}

//  A turn's line for the log.
std::string turn_log(const LeagueFightData& fight, const PlannedState& state, const BattleTrackState& track, const PlannedChoice& choice){
    const PlannedDanger danger = planned_danger(state);
    const std::string what = choice.item ? *choice.item
        : choice.move_slot && *choice.move_slot < state.own.moves.size() ? state.own.moves[*choice.move_slot].id
        : std::string("best attack");
    const StatStages& own = track.own;
    const StatStages& foe = track.foe;
    return std::format(
        "{} HP {}/{}, {} {}/{} (slot {}, {} left), D {} mean {:.0f}, our stages atk {} def {} spa {} spd {} spe {} acc {} mist {}, "
        "foe stages atk {} def {} spa {} spd {} spe {} eva {}, foe status {} sleep {}, weather {} ({}), "
        "trainer Full Restores used {}: {} ({})",
        fight.name, state.own.hp, state.own.max_hp, state.foe.species, state.foe.hp, state.foe.max_hp,
        state.foe.slot, state.foe_party_alive, danger.normal_max, danger.mean,
        own.attack, own.defense, own.sp_attack, own.sp_defense, own.speed, own.accuracy, track.mist_turns,
        foe.attack, foe.defense, foe.sp_attack, foe.sp_defense, foe.speed, foe.evasion,
        static_cast<int>(track.foe_status), track.foe_sleep_turns, static_cast<int>(track.weather), track.weather_turns,
        track.trainer_full_restores, choice.row, what
    );
}

} // namespace


PlannedState planned_state(const PlannedInputs& inputs, const LeagueFoesData& data){
    const Stats raw = inputs.lead.stats.value_or(Stats{});
    const BattleTrackState& track = inputs.track;
    const Side us{
        .level = static_cast<int>(battle_level(inputs.lead)),
        .stats = boosted_stats(raw),
        .hp = inputs.hp,
        .types = types_of(inputs.lead.species),
        .ability = pinch_ability(inputs.lead.species),
        .stages = track.own,
        .status = planned_status(inputs.status),
    };
    const LeagueFoeData& foe_data = inputs.foe.foe;
    const Side foe{
        .level = foe_data.level, .stats = foe_data.stats, .hp = hp_of_fill(inputs.foe.fill, foe_data.stats.hp),
        .types = types_of(foe_data.species), .ability = foe_data.ability,
        .stages = track.foe, .status = track.foe_status,
    };
    const PlannedFoeTrack& foe_track = inputs.foe;
    return PlannedState{
        .own = planned_own(inputs, {us, foe}, data),
        .foe = planned_foe(inputs, {us, foe}, data),
        .foe_party_alive = foe_track.party_alive,
        .trainer_item_open = trainer_item_open(foe_track.full_restores_carried, track.trainer_full_restores, foe_track.party_alive),
        .bag = inputs.bag,
    };
}


PlannedBattle::PlannedBattle(const LeagueFightData& fight, const LeagueTrainerData& trainer)
    : m_fight(&fight)
    , m_trainer(&trainer)
{}

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
    m_track.foe_out(slot, foe.ability);
    const auto carried = m_trainer->items.find("full-restore");
    return PlannedFoeTrack{
        .foe = foe,
        .slot = slot,
        .fill = view.foe_fill,
        .party_alive = static_cast<int>(party.size() - slot),
        .full_restores_carried = carried == m_trainer->items.end() ? 0 : carried->second,
    };
}

PlannedBattle::Turn PlannedBattle::choose(const Supplies& supplies, const PolicyView& view){
    m_track.read_turn(view.battle_texts);
    PlannedInputs inputs{
        .lead = supplies.lead,
        .hp = static_cast<int>(std::lround(view.hp)),
        .status = view.status,
        .foe = foe_out(supplies, view),
    };
    inputs.track = m_track.state();
    for (std::string_view item : LEAGUE_ITEMS){
        inputs.bag[std::string(item)] = bag_count(supplies, item);
    }
    const PlannedState state = planned_state(inputs, league_foes_data());
    const PlannedChoice choice = choose_planned(state, *m_fight, m_streak.streak(state));
    m_streak.chose(state, choice);
    std::string log = turn_log(*m_fight, state, inputs.track, choice);
    m_track.turn_chosen();
    return Turn{.choice = choice, .log = std::move(log)};
}

void PlannedBattle::item_used(std::string_view item){
    m_track.item_used(item);
}

void PlannedBattle::lead_switched_in(){
    m_track.lead_switched_in();
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

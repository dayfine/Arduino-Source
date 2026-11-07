/*  AutoStory Damage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "PokemonFRLG_AutoStoryDamage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr uint32_t MAX_IV = 31;
constexpr uint32_t MIN_ROLL = 85;
constexpr uint32_t MAX_ROLL = 100;
constexpr uint32_t ROLLS = MAX_ROLL - MIN_ROLL + 1;
constexpr int MAX_STAGE = 6;

//  CALC_STAT with ModifyStatByNature. `index` is the nature table's:
//  Attack 0, Defense 1, Speed 2, Sp. Atk 3, Sp. Def 4.
uint16_t other_stat(uint8_t base, const Genes& genes, uint8_t level, size_t index){
    uint32_t stat = (2 * base + genes.iv + genes.ev / 4) * level / 100 + 5;
    const size_t raised = genes.nature / 5;
    const size_t lowered = genes.nature % 5;
    if (raised != lowered && index == raised){
        stat = stat * 110 / 100;
    }else if (raised != lowered && index == lowered){
        stat = stat * 90 / 100;
    }
    return static_cast<uint16_t>(stat);
}

uint32_t boosted(uint32_t stat, bool badge){
    return badge ? 110 * stat / 100 : stat;
}

//  APPLY_STAT_MOD, and a crit's: the attacker's drops and the defender's
//  rises are left out.
uint32_t attack_side(uint32_t stat, int stage, bool crit){
    return crit && stage <= 0 ? stat : staged_stat(stat, stage);
}
uint32_t defense_side(uint32_t stat, int stage, bool crit){
    return crit && stage >= 0 ? stat : staged_stat(stat, stage);
}

int stage_of(const Combatant& combatant, Stage stage){
    return combatant.stages[static_cast<size_t>(stage)];
}

//  CalculateBaseDamage, for a physical or special move.
uint32_t base_damage(const Combatant& attacker, const Combatant& defender, const MoveData& move, bool crit){
    const bool special = special_type(move.type);
    const uint32_t attack = special
        ? attack_side(boosted(attacker.stats.sp_attack, attacker.volcano_badge), stage_of(attacker, Stage::SP_ATTACK), crit)
        : attack_side(boosted(attacker.stats.attack, attacker.boulder_badge), stage_of(attacker, Stage::ATTACK), crit);
    const uint32_t defense = special
        ? defense_side(boosted(defender.stats.sp_defense, defender.volcano_badge), stage_of(defender, Stage::SP_DEFENSE), crit)
        : defense_side(boosted(defender.stats.defense, defender.soul_badge), stage_of(defender, Stage::DEFENSE), crit);
    uint32_t damage = attack * move.power;
    damage *= 2 * attacker.level / 5 + 2;
    damage /= std::max<uint32_t>(defense, 1);
    damage /= 50;
    //  Only the physical branch has this.
    if (!special && damage == 0){
        damage = 1;
    }
    return damage + 2;
}

} // namespace


uint8_t trainer_iv(uint8_t pret_iv){
    return static_cast<uint8_t>(pret_iv * MAX_IV / 255);
}

Stats gen3_stats(const BaseStats& base, uint8_t level, const Genes& genes){
    return Stats{
        .hp = static_cast<uint16_t>((2 * base.hp + genes.iv + genes.ev / 4) * level / 100 + level + 10),
        .attack = other_stat(base.attack, genes, level, 0),
        .defense = other_stat(base.defense, genes, level, 1),
        .sp_attack = other_stat(base.sp_attack, genes, level, 3),
        .sp_defense = other_stat(base.sp_defense, genes, level, 4),
        .speed = other_stat(base.speed, genes, level, 2),
    };
}

uint32_t staged_stat(uint32_t stat, int stage){
    stage = std::clamp(stage, -MAX_STAGE, MAX_STAGE);
    //  gStatStageRatios: (10 + 5n) / 10 up, 10 / (10 + 5n) down.
    return stage >= 0 ? stat * (10 + 5 * stage) / 10 : stat * 10 / (10 - 5 * stage);
}

std::optional<Combatant> combatant(std::string_view species, uint8_t level, const Genes& genes){
    const SpeciesData* data = species_data(species);
    if (data == nullptr){
        return std::nullopt;
    }
    return Combatant{.first = data->first, .second = data->second, .level = level, .stats = gen3_stats(data->base, level, genes)};
}

std::optional<uint32_t> fixed_damage(const MoveData& move, const Combatant& attacker){
    if (move.effect == "dragon-rage"){
        return 40;
    }
    if (move.effect == "sonicboom"){
        return 20;
    }
    if (move.effect == "level-damage"){
        return attacker.level;
    }
    return std::nullopt;
}

uint32_t damage_before_roll(const Combatant& attacker, const Combatant& defender, const MoveData& move, bool crit){
    if (move.power == 0 || move.type == PokemonType::NONE){
        return 0;
    }
    const std::vector<size_t> multipliers = type_multipliers(move.type, defender.first, defender.second);
    const bool immune = std::ranges::find(multipliers, 0) != multipliers.end();
    if (const std::optional<uint32_t> fixed = fixed_damage(move, attacker)){
        return immune ? 0 : *fixed;
    }
    uint32_t damage = base_damage(attacker, defender, move, crit);
    if (crit){
        damage *= 2;
    }
    if (attacker.first == move.type || attacker.second == move.type){
        damage = damage * 15 / 10;
    }
    for (size_t tenths : multipliers){
        damage = static_cast<uint32_t>(damage * tenths / 10);
        if (damage == 0 && tenths != 0){
            damage = 1;
        }
    }
    return damage;
}

uint32_t rolled_damage(uint32_t before, uint32_t percent){
    if (before == 0){
        return 0;
    }
    return std::max<uint32_t>(before * percent / 100, 1);
}

DamageRange damage_range(const Combatant& attacker, const Combatant& defender, const MoveData& move, bool crit){
    const uint32_t before = damage_before_roll(attacker, defender, move, crit);
    if (fixed_damage(move, attacker)){
        return DamageRange{.min = before, .max = before};
    }
    return DamageRange{.min = rolled_damage(before, MIN_ROLL), .max = rolled_damage(before, MAX_ROLL)};
}

double average_damage(const Combatant& attacker, const Combatant& defender, const MoveData& move){
    const uint32_t before = damage_before_roll(attacker, defender, move, false);
    if (fixed_damage(move, attacker)){
        return before;
    }
    uint32_t total = 0;
    for (uint32_t percent = MIN_ROLL; percent <= MAX_ROLL; percent++){
        total += rolled_damage(before, percent);
    }
    return static_cast<double>(total) / ROLLS;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

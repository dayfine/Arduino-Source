/*  AutoStory Gen 3 Damage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "PokemonFRLG_AutoStoryGen3Damage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


int staged_stat(int stat, int stage){
    stage = std::clamp(stage, -6, 6);
    if (stage >= 0){
        return stat * (2 + stage) / 2;
    }
    return stat * 2 / (2 - stage);
}

int badge_boosted(int stat){
    return stat * 110 / 100;
}

std::vector<int> gen3_damage_rolls(const Gen3Hit& hit){
    if (hit.power <= 0){
        return {};
    }
    //  @smogon/calc calculateADV(): a critical hit keeps only the stages
    //  that help the attacker.
    const bool attack_staged = hit.attack_stage > 0 || (!hit.critical && hit.attack_stage < 0);
    const bool defense_staged = hit.defense_stage < 0 || (!hit.critical && hit.defense_stage > 0);
    const int64_t attack = attack_staged ? staged_stat(hit.attack, hit.attack_stage) : hit.attack;
    const int64_t defense = std::max(1, defense_staged ? staged_stat(hit.defense, hit.defense_stage) : hit.defense);
    const int64_t level_factor = 2 * hit.level / 5 + 2;
    int64_t base = level_factor * attack * hit.power / defense / 50;
    if (hit.burned_physical){
        base /= 2;
    }
    base = static_cast<int64_t>(std::floor(static_cast<double>(base) * hit.weather));
    base = (hit.physical ? std::max<int64_t>(base, 1) : base) + 2;
    if (hit.critical){
        base *= 2;
    }
    if (hit.stab){
        base = base * 3 / 2;
    }
    base = static_cast<int64_t>(std::floor(static_cast<double>(base) * hit.effectiveness_first));
    base = static_cast<int64_t>(std::floor(static_cast<double>(base) * hit.effectiveness_second));
    std::vector<int> rolls;
    for (int64_t percent = 85; percent <= 100; percent++){
        rolls.push_back(static_cast<int>(std::max<int64_t>(base == 0 ? 0 : 1, base * percent / 100)));
    }
    return rolls;
}

int battle_speed(int raw, int stage, bool paralyzed){
    const int speed = staged_stat(raw, stage);
    return paralyzed ? speed / 4 : speed;
}

double hit_chance(int accuracy, int accuracy_stage, int evasion_stage){
    //  pret's sAccuracyStageRatios, from -6 to +6.
    constexpr int RATIOS[13][2] = {
        {33, 100}, {36, 100}, {43, 100}, {50, 100}, {60, 100}, {75, 100}, {1, 1},
        {133, 100}, {166, 100}, {2, 1}, {233, 100}, {133, 50}, {3, 1},
    };
    const int stage = std::clamp(accuracy_stage - evasion_stage, -6, 6) + 6;
    const int percent = accuracy * RATIOS[stage][0] / RATIOS[stage][1];
    return std::min(1.0, percent / 100.0);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A Pokemon's stats as POKEMON SKILLS shows them.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStats_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStats_H

#include <cstdint>
#include <optional>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A Pokemon's stats, in the game's order.
struct Stats{
    uint16_t hp = 0;
    uint16_t attack = 0;
    uint16_t defense = 0;
    uint16_t sp_attack = 0;
    uint16_t sp_defense = 0;
    uint16_t speed = 0;
};

//  A foe's Defense and Sp. Def, and its HP where the plan has it, from its
//  trainer's party data (a battle step's "foe_stats").
struct FoeStats{
    int defense = 0;
    int sp_defense = 0;
    std::optional<int> hp = std::nullopt;
};

//  A stat stage's multiplier: (2 + stage) / 2 up, 2 / (2 - stage) down.
inline double stage_multiplier(int stage){
    return stage >= 0 ? (2.0 + stage) / 2.0 : 2.0 / (2.0 - stage);
}

//  What the damage estimate knows beyond the member itself.
struct AttackContext{
    //  The member's Attack and Sp. Atk stages: X Attacks and X Specials used.
    int attack_stage = 0;
    int sp_attack_stage = 0;
    //  The foe's stats, for a single foe; else even Defense and Sp. Def.
    std::optional<FoeStats> foe = std::nullopt;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

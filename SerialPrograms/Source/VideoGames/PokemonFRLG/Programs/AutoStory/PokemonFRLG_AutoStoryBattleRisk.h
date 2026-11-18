/*  AutoStory Battle Risk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How carefully a story battle is fought, by the trainer its plan names.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleRisk_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleRisk_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  How carefully a battle step is fought.
enum class BattleRisk{
    //  The plan's attacks, else the best type matchup against the foe out.
    LOW,
    //  The battles the simulation's plan is for (the rival, gym leaders,
    //  bosses); fought as LOW until the checkpoint carries a plan.
    HIGH,
};

//  String representation of battle risk level.
std::string_view battle_risk_name(BattleRisk risk);

//  A trainer's risk by its kind, from pret's constant the battle step's
//  plan names ("TRAINER_LEADER_BROCK"): HIGH for the rival, gym leaders,
//  bosses (Giovanni, the Rocket admins), the Elite Four and the Champion;
//  LOW for the rest and for a battle that names no trainer.
BattleRisk default_battle_risk(std::string_view trainer);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

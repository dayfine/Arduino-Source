/*  BDSP AutoStory - Talk Fight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A talk that may end in a trainer's battle, with whether anyone talked on
 *  the overworld after the battle.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TalkFight_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TalkFight_H

#include "PokemonBDSP_AutoStory_Talks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How a talk ended, and what was said after its battle.
struct TalkFight{
    bool fought = false;
    //  Looks with a box on the overworld after the battle (0 with none).
    size_t box_looks_after = 0;
};
//  talk_or_fight() that also says whether anyone talked after the battle.
TalkFight talk_or_fight_who(
    VideoStream& stream, ProControllerContext& context, const std::string& label,
    TalkExpect expect = TalkExpect::TALK_OR_BATTLE, BattleFormat format = BattleFormat::SINGLES
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

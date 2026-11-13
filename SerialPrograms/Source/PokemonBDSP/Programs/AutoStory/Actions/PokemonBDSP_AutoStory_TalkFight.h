/*  BDSP AutoStory - Talk Fight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A talk that may end in a trainer's battle, with who the battle's intro
 *  text said the opponent was.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TalkFight_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TalkFight_H

#include "PokemonBDSP_AutoStory_Talks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How a talk ended, and who the battle's intro text said it was against.
struct TalkFight{
    bool fought = false;
    //  UNREAD when there was no battle, or its intro was not read.
    BattleOpponent opponent = BattleOpponent::UNREAD;
};
//  talk_or_fight() that also says who the opponent was.
TalkFight talk_or_fight_who(
    VideoStream& stream, ProControllerContext& context, const std::string& label,
    TalkExpect expect = TalkExpect::TALK_OR_BATTLE, BattleFormat format = BattleFormat::SINGLES
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

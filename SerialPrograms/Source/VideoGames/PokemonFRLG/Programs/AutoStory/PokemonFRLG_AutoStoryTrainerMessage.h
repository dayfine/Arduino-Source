/*  AutoStory Trainer Message
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A trainer who saw the player and speaks on the overworld: the message
 *  cleared, and the battle after it fought.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrainerMessage_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrainerMessage_H

#include <functional>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  From a message on the overworld: advances it until the overworld is back
//  and, when a battle starts after it (a trainer who saw the player),
//  fights it and recovers after it. A battle lost (a blackout) throws
//  OperationFailedException.
void clear_overworld_message(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, Pokemon::StoryCheckLevel checks, Language language
);

//  On the console, from a trainer's message during pacing: closes it,
//  fights the battle after it and walks the player back onto the patch's
//  tile, planned from both of pacing's tiles. While a trainer walks up the
//  player can't move, so the steps pacing took meanwhile went nowhere and
//  the spot pacing kept can be a tile off. A battle lost throws, as on a
//  route.
using PaceTrainerBattle = std::function<void()>;



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

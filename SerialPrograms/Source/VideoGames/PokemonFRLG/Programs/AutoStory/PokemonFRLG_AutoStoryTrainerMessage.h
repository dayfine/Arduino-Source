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
#include "Common/Cpp/Time.h"
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

//  The longest wait_out_battle_exit() waits for the overworld.
inline constexpr Milliseconds BATTLE_EXIT_TIMEOUT(10000);

//  After a battle run from or won: waits until the overworld has shown
//  nothing (wait_for_quiet_overworld()) for MovementMechanics::
//  AFTER_BATTLE_QUIET, at most BATTLE_EXIT_TIMEOUT, and logs how the screen
//  ended. A tap sent as the battle's black ends is lost (console B,
//  2026-11-09, 00.28 winston_line).
void wait_out_battle_exit(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

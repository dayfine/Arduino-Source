/*  Trainer Card
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Opening the Trainer Card from the overworld and reading the TID on it,
 *  as SID Helper does. Shared with AutoStory, which reads the TID for its
 *  Starter RNG plan.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_TrainerCard_H
#define PokemonAutomation_PokemonFRLG_TrainerCard_H

#include <cstdint>
#include <optional>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  From the overworld, START then the player's entry, until the card shows.
//  The entry is second only before the first Pokemon and the Pokedex.
//  Throws OperationFailedException after 5 failed tries.
void navigate_to_trainer_card(ConsoleHandle& console, ProControllerContext& context);

//  The TID on the Trainer Card on screen. An unreadable TID comes back as
//  TrainerIdReader::read_tid() returns it.
uint16_t read_trainer_card_tid(ConsoleHandle& console, ProControllerContext& context, Language language);
//  Nothing when the TID can't be read.
std::optional<uint16_t> try_read_trainer_card_tid(ConsoleHandle& console, ProControllerContext& context, Language language);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

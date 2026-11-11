/*  AutoStory PC Transfer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The PC step's transfer, a deposit or a withdrawal, and its args in the
 *  story table.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPcTransfer_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPcTransfer_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_PcScreenReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  The storage operation to perform at the PC.
enum class PcAction{
    DEPOSIT,
    WITHDRAW,
};

//  What a DEPOSIT does when the party hasn't got its slot: refuse, or
//  skip it as done (the Magikarp from Gift RNG, when Gift RNG didn't run).
enum class PcMissing{
    REFUSE,
    SKIP,
};

//  What a PC step does. Slots and boxes count from 0.
struct PcTransfer{
    PcAction action = PcAction::DEPOSIT;
    //  DEPOSIT: the party slot to store.
    size_t party_slot = 0;
    //  DEPOSIT: the box to store it in. WITHDRAW: the box to take from,
    //  which must be the box the PC shows (the last one viewed).
    size_t box = 0;
    //  WITHDRAW: the box slot, row by row, six to a row.
    size_t box_slot = 0;
    PcMissing if_missing = PcMissing::REFUSE;
};

//  The GAME action of a PC step, slots and boxes counted from 1 as the game
//  shows them: do WORD (required), "deposit" or "withdraw"; slot INTEGER
//  (required), the party slot (1-6) of a deposit or the box slot (1-30)
//  of a withdrawal; box INTEGER (required, 1-14); if_present BOOLEAN
//  (false): a deposit skipped when the party hasn't got the slot.
inline const std::string PC_ACTION = "pc";
//  `transfer` as its action's args.
Pokemon::StoryActionArgs pc_args(const PcTransfer& transfer);
//  The transfer a PC_ACTION step's args give; nothing if they name a slot
//  or box the game hasn't got.
std::optional<PcTransfer> pc_transfer_of(const Pokemon::StoryActionArgs& args);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

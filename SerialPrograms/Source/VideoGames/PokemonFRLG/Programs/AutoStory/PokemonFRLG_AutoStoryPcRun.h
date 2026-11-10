/*  AutoStory PC Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game steps a PC transfer takes, injected so its decisions run in
 *  tests, how it ends, and its bounds.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPcRun_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPcRun_H

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
#include "PokemonFRLG_AutoStoryPcDialog.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The game steps a PC step takes, injected so its decisions run in tests.
class PcStorageActions{
public:
    virtual ~PcStorageActions() = default;

    //  From the overworld: the party, read from the party menu, which is
    //  closed again.
    virtual std::vector<PcPartyMember> read_party() = 0;
    //  What shows now.
    virtual PcView read_view() = 0;
    //  One press (A, B or the D-pad), and the wait for the screen to answer.
    virtual void press(PcPress press) = 0;
};

//  How a PC transfer sequence concluded.
enum class PcEnd{
    DONE,
    //  A DEPOSIT with PcMissing::SKIP whose slot the party hasn't got: the
    //  PC wasn't touched.
    SKIPPED,
    //  pc_transfer_error() refused it before the PC was touched.
    REFUSED,
    FAILED,
};
//  The result of a PC transfer operation.
struct PcResult{
    PcEnd end = PcEnd::FAILED;
    std::string detail = {};
};

//  Reads after this many presses without the overworld back: a failure.
constexpr size_t PC_MAX_PRESSES = 80;
//  OTHER read this many times in a row: a failure.
constexpr size_t PC_MAX_OTHER_IN_A_ROW = 10;
//  The overworld read this many times after A at the PC: it didn't boot.
constexpr size_t PC_BOOT_READS = 4;



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

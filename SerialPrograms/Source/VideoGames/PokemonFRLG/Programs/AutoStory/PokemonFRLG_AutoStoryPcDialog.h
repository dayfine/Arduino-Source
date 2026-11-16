/*  AutoStory PC Dialog
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The PC's screens as a dialog: what to press on each, for a transfer and
 *  the party it starts with.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPcDialog_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPcDialog_H

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
#include "PokemonFRLG_AutoStoryPcTransfer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A party member as far as a PC step needs to know.
struct PcPartyMember{
    //  Its species' slug, or empty if not read.
    std::string species = {};
    bool fainted = false;
};

//  Why `transfer` can't be done with `party`, or empty if it can: a
//  deposit of a slot the party hasn't got, of its last Pokemon, or of its
//  last one that can fight; a withdrawal into a full party.
std::string pc_transfer_error(const PcTransfer& transfer, const std::vector<PcPartyMember>& party);


//  A controller action taken during a PC dialog sequence.
enum class PcPress{
    A,
    B,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    //  Nothing: look again.
    WAIT,
    //  The transfer is made and the overworld is back.
    DONE,
    FAIL,
};
//  The human-readable name of a PC controller action.
std::string_view pc_press_name(PcPress press);

//  The next action decided by the PC dialog state machine.
struct PcDecision{
    PcPress press = PcPress::WAIT;
    //  FAIL: why.
    std::string reason = {};
};

//  What the PC step presses on each screen it reads, from the overworld
//  facing the PC to the overworld again with the transfer made.
class PcStorageDialog{
public:
    explicit PcStorageDialog(const PcTransfer& transfer);

    PcDecision next(const PcView& view);

    //  Whether the transfer was made: the last press that makes it was sent.
    bool transferred() const{
        return m_transferred;
    }

private:
    PcDecision on_overworld();
    PcDecision on_storage_menu(const PcView& view);
    PcDecision on_party_panel(const PcView& view);
    PcDecision on_box_grid(const PcView& view);
    PcDecision on_mon_menu(const PcView& view);
    PcDecision on_box_chooser(const PcView& view);

    PcTransfer m_transfer;
    bool m_booted = false;
    //  The overworld read since A at the PC, while the PC boots.
    size_t m_booting_reads = 0;
    bool m_transferred = false;
    //  BOX_CHOOSER opens on BOX 1 each time the PC is entered.
    bool m_chooser_seen = false;
    size_t m_others_in_a_row = 0;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

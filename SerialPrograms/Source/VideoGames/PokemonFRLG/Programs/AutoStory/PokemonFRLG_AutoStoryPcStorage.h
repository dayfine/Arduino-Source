/*  AutoStory PC Storage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story step at a Pokemon Center's PC: deposit a party member in a box,
 *  or withdraw one from a box, as pokefirered's Pokemon Storage System runs
 *  it (data/scripts/pc.inc, src/pokemon_storage_system_*.c), confirmed by
 *  hand on console B (2026-11-02, Route 4's Pokemon Center):
 *
 *  - A at the PC: "<player> booted up the PC.", then "Which PC should be
 *    accessed?" with SOMEONE'S PC (BILL'S PC later) on top, always opened
 *    on it. A on it: "Accessed SOMEONE's PC." and "POKEMON Storage System
 *    opened.", then the storage menu: WITHDRAW, DEPOSIT, MOVE POKEMON,
 *    MOVE ITEMS, SEE YA!. It opens on WITHDRAW from the PC, but on the last
 *    option used when it comes back from a box, and UP/DOWN wrap.
 *  - DEPOSIT: the box screen with the party panel, the hand on the party's
 *    first slot. DOWN steps through the six slots, then CANCEL, and wraps.
 *    A on a Pokemon: STORE, SUMMARY, MARK, RELEASE, CANCEL, on STORE.
 *    STORE: "Deposit in which BOX?", on BOX 1 each time the PC is entered;
 *    RIGHT/LEFT change the box, A stores it there.
 *  - WITHDRAW: the box screen on the current box (the game remembers it),
 *    the hand on its top left Pokemon; RIGHT/DOWN move it by one of six
 *    columns and five rows. A on a Pokemon: WITHDRAW, SUMMARY, MARK,
 *    RELEASE, CANCEL, on WITHDRAW; WITHDRAW puts it in the party.
 *  - B on a box screen: "Continue BOX operations?" YES/NO on YES; B leaves
 *    for the storage menu, B again the PC list, B again the overworld.
 *
 *  The step never moves the cursor on a Pokemon's menu, whose RELEASE is
 *  three rows below STORE: it confirms the top row and presses A there.
 *  The game refuses to deposit its last Pokemon and to withdraw into a full
 *  party; the step refuses first, and also refuses to leave the party with
 *  no Pokemon that can fight.
 *
 *  The step's decisions take what shows from a PcStorageActions, so they
 *  run in tests against a fake PC. On the console, each screen is read
 *  (PokemonFRLG_PcScreenReader.h) before the press it gets: a press sent
 *  before a menu is drawn goes to the message behind it (console B,
 *  2026-11-02: DOWN, A before the storage menu showed opened WITHDRAW).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPcStorage_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPcStorage_H

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
const char* pc_press_name(PcPress press);

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

//  From the overworld, facing the PC: the transfer, back to the overworld.
PcResult run_pc_transfer(Logger& logger, PcStorageActions& actions, const PcTransfer& transfer);


//  From the overworld: the party's size, read from the party menu, which is
//  closed again.
size_t read_party_size(ConsoleHandle& console, ProControllerContext& context);

//  The console's PC steps. Party members are read by count only: none is
//  taken as fainted, since the story heals before it uses the PC. Screens
//  are read with read_pc_screen(); the overworld only once a second frame,
//  a moment later, shows it too, since a frame between two menus can look
//  like it.
class ConsolePcStorageActions : public PcStorageActions{
public:
    ConsolePcStorageActions(ConsoleHandle& console, ProControllerContext& context);

    virtual std::vector<PcPartyMember> read_party() override;
    virtual PcView read_view() override;
    virtual void press(PcPress press) override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
};


//  Register auto-story PC storage unit tests.
void add_tests_AutoStoryPcStorage(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

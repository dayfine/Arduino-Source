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
#include "PokemonFRLG_AutoStoryPcRun.h"
#include "PokemonFRLG_AutoStoryPcDialog.h"
#include "PokemonFRLG_AutoStoryPcTransfer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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

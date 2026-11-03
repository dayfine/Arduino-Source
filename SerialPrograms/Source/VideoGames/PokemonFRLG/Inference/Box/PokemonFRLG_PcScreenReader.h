/*  PC Screen Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tells which screen of a Pokemon Center's PC shows, and where its cursor
 *  is, from one frame. Made from captures of FireRed on console B
 *  (2026-11-02, Route 4's Pokemon Center), in GBA pixels:
 *
 *  - The PC list ("Which PC should be accessed?") and the storage menu
 *    (WITHDRAW ... SEE YA!) are windows at the top left, their arrow at x 9
 *    and rows 16 apart from y 12; the list's window ends at x 126, the
 *    menu's at x 150. Both have the white message box below them.
 *  - The box screens have the PKMN DATA panel at the left. DEPOSIT shows
 *    the party in a teal column (the lead at the left, five slots at the
 *    right, CANCEL below), the hand over the slot it points at; WITHDRAW
 *    shows the box's grid. A Pokemon's menu (STORE or WITHDRAW, SUMMARY,
 *    MARK, RELEASE, CANCEL) opens at (154, 34), its arrow at x 160, rows
 *    16 apart from y 44; "Deposit in which BOX?" a yellow box with the
 *    box's name; "Continue BOX operations?" a YES/NO at (188, 85).
 *
 *  A message box with nothing else is TEXT; a frame with none of these, no
 *  START menu and not dark is the OVERWORLD. A screen between two of them
 *  can read as the overworld: the caller confirms it on a later frame.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PcScreenReader_H
#define PokemonAutomation_PokemonFRLG_PcScreenReader_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What shows during a PC step, as far as its decisions go.
enum class PcScreen{
    //  The overworld, with no message.
    OVERWORLD,
    //  A message waiting for a button: the PC booting, "Accessed ...".
    TEXT,
    //  "Which PC should be accessed?": SOMEONE'S PC on top.
    PC_LIST,
    //  WITHDRAW, DEPOSIT, MOVE POKEMON, MOVE ITEMS, SEE YA!.
    STORAGE_MENU,
    //  DEPOSIT's box screen: the hand on the party panel (0-5, 6 CANCEL).
    PARTY_PANEL,
    //  WITHDRAW's box screen: the hand on the box (0-29, row by row).
    BOX_GRID,
    //  A Pokemon's menu: STORE or WITHDRAW, SUMMARY, MARK, RELEASE, CANCEL.
    MON_MENU,
    //  "Deposit in which BOX?".
    BOX_CHOOSER,
    //  "Continue BOX operations?".
    YES_NO,
    //  Anything else: a fade, a frame between two screens.
    OTHER,
};
const char* pc_screen_name(PcScreen screen);

struct PcView{
    PcScreen screen = PcScreen::OTHER;
    //  The cursor's row (a menu, the party panel; YES/NO: 0 YES, 1 NO) or
    //  cell (the box grid), if read.
    std::optional<size_t> cursor = std::nullopt;
    //  BOX_GRID, BOX_CHOOSER: the box shown (from 0), if read.
    std::optional<size_t> box = std::nullopt;
    //  BOX_GRID: whether the cell under the hand holds a Pokemon, if read.
    std::optional<bool> occupied = std::nullopt;
};

//  The PC's screen on `screen`, with its cursor where it can be read.
PcView read_pc_screen(Logger& logger, const ImageViewRGB32& screen);


void add_tests_PcScreenReader(UnitTestDatabase& database);



}
}
}
#endif

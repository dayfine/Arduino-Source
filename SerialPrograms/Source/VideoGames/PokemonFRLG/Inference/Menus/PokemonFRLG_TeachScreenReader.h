/*  Teach Screen Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the screens a TM is taught through, from the bag to the move it
 *  replaces, as the game shows them (console B, 2026-11-03): the bag's
 *  pockets, TM CASE in KEY ITEMS and its OPEN/REGISTER/CANCEL menu, the TM
 *  CASE's list and a TM's USE/GIVE/EXIT menu, the party menu's "Teach which
 *  POKEMON?", its messages and YES/NO, and the KNOWN MOVES page picking the
 *  move to forget (read with PokemonFRLG_SummaryMovesReader.h).
 *
 *  Which screen shows is told from the colors of its background, its boxes
 *  and the dark arrow of its cursor, measured on those captures in the
 *  game's 240x160 pixels: the bag's frame is orange, the TM CASE's mint,
 *  the party menu's dark teal, the TM's scene's pale blue diamonds. Names
 *  and messages are read by OCR, in English only so far: in another
 *  language nothing is read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_TeachScreenReader_H
#define PokemonAutomation_PokemonFRLG_TeachScreenReader_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "PokemonFRLG_TeachTextReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The screen or menu shown while teaching a TM or HM.
enum class TeachScreen{
    OTHER,
    //  A pocket's list, its cursor live.
    BAG,
    //  OPEN/REGISTER/CANCEL over the bag, or USE/GIVE/EXIT over the TM CASE.
    ITEM_MENU,
    //  The TM CASE's list, its cursor live.
    TM_CASE,
    //  "Teach which POKEMON?"
    PARTY_PROMPT,
    //  A message over the party menu.
    PARTY_TEXT,
    //  A message over the party menu with YES/NO.
    PARTY_YES_NO,
    //  The KNOWN MOVES page with its red cursor: the move to forget.
    KNOWN_MOVES,
    //  The TM's scene over the member (pokefirered
    //  pokemon_special_anim.c), on its blue diamonds: with a free slot,
    //  right after A on the member; after a forget, "1, 2 and... Poof!",
    //  "<MON> forgot <OLD>.", "And...". Then "Machine set!", which waits
    //  for A (B cancels the teach), the disc's wobble, "<MON> learned
    //  <MOVE>!" and the TM CASE.
    TM_SCENE,
};
//  The human-readable name of a teach screen.
std::string_view teach_screen_name(TeachScreen screen);

//  Reads the active teach screen from an image.
TeachScreen read_teach_screen(const ImageViewRGB32& screen);


//  Rows a list or menu shows at once: the bag's, the TM CASE's (its info
//  box is below), the item menu's.
constexpr size_t TEACH_LIST_ROWS = 6;

//  The row the cursor is on: the dark arrow in the bag's list, the TM
//  CASE's list, the item menu (0: OPEN or USE) or the party menu's YES/NO
//  (0: YES); the red box on KNOWN MOVES. Nothing if no row or more than one
//  has it.
std::optional<size_t> read_teach_cursor(TeachScreen screen, const ImageViewRGB32& image);

//  The open pocket's name: "items", "key-items" or "poke-balls"; empty if
//  not read.
std::string read_pocket_name(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The rows of KEY ITEMS (`screen` BAG) or the TM CASE from the top, up to
//  the first not read; KEY ITEMS' end with its "cancel".
std::vector<std::string> read_teach_rows(Logger& logger, Language language, const ImageViewRGB32& image, TeachScreen screen);

//  Register teach screen reader unit tests.
void add_tests_TeachScreenReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

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
 *  the party menu's dark teal. Names and messages are read by OCR, in
 *  English only so far: in another language nothing is read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_TeachScreenReader_H
#define PokemonAutomation_PokemonFRLG_TeachScreenReader_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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
};
const char* teach_screen_name(TeachScreen screen);

TeachScreen read_teach_screen(const ImageViewRGB32& screen);


//  Rows a list or menu shows at once.
constexpr size_t TEACH_LIST_ROWS = 6;
constexpr size_t ITEM_MENU_ROWS = 3;

//  The row the dark arrow is on: in the bag's list, the TM CASE's list,
//  the item menu (0: OPEN or USE) or the party menu's YES/NO (0: YES).
//  Nothing if no row or more than one has it.
std::optional<size_t> read_teach_cursor(TeachScreen screen, const ImageViewRGB32& image);

//  The open pocket's name: "items", "key-items" or "poke-balls"; empty if
//  not read.
std::string read_pocket_name(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The key item in the bag's row `row`: "tm-case", "teachy-tv", ... or
//  "cancel"; empty if not read.
std::string read_key_item(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row);

//  The move of the TM or HM in the TM CASE's row `row`, as a slug such as
//  "water-pulse"; empty for CLOSE or a move not read.
std::string read_tm_case_move(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row);

//  The messages of the party menu while a TM is taught.
enum class TeachText{
    UNKNOWN,
    TEACH_WHICH,        //  "Teach which POKEMON?"
    WANTS_TO_LEARN,     //  "<MON> wants to learn the move <MOVE>."
    KNOWS_FOUR,         //  "However, <MON> already knows four moves."
    DELETE_QUESTION,    //  "Should a move be deleted and replaced with <MOVE>?"
    WHICH_MOVE,         //  "Which move should be forgotten?"
    STOP_QUESTION,      //  "Stop trying to teach <MOVE>?"
    DID_NOT_LEARN,      //  "<MON> did not learn the move <MOVE>."
    NOT_COMPATIBLE,     //  "<MON> and <MOVE> are not compatible."
};
const char* teach_text_name(TeachText text);

//  The message over the party menu, from the prompt's or the message box's
//  first two lines.
TeachText read_teach_text(Logger& logger, Language language, const ImageViewRGB32& screen);


void add_tests_TeachScreenReader(UnitTestDatabase& database);



}
}
}
#endif

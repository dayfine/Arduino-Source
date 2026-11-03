/*  Summary Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the KNOWN MOVES page of a Pokemon's summary: each move's name and
 *  PP, and which row the red cursor box is on when the page picks a move
 *  (A on the page, or the move to forget for a new one). pokefirered's
 *  pokemon_summary_screen.c draws the moves in the right pane from (160,
 *  19), 28 pixels apart: the name at x 3, the PP's line at y 16 with the
 *  current PP right-aligned in two digits at x 46, "/" at 58 and the full
 *  PP at 64. The fifth row is CANCEL, or the move offered.
 *
 *  It knows the names of the moves of the lines the FRLG AutoStory party
 *  comes from, and of the TMs' and HMs' moves, in English only so far: in
 *  another language a name is never read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_SummaryMovesReader_H
#define PokemonAutomation_PokemonFRLG_SummaryMovesReader_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The rows of the page: four moves, then CANCEL or the move offered.
constexpr size_t SUMMARY_MOVE_ROWS = 5;

//  Where row `row` (from 0) shows its move's name, its current PP and its
//  full PP, as fractions of the game screen.
ImageFloatBox summary_move_name_box(size_t row);
ImageFloatBox summary_move_pp_box(size_t row);
ImageFloatBox summary_move_max_pp_box(size_t row);

//  The move named in row `row`: a slug such as "water-gun", in a game in
//  `language`; empty for an empty slot, a move the reader has no name
//  for, or a language it has no names for.
std::string read_summary_move_name(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row);

//  The move named in `name`, a box around one move's name in dark text on
//  a light background, such as a TM CASE row's; empty as above.
std::string read_move_name(Logger& logger, Language language, const ImageViewRGB32& name);

struct SummaryMove{
    std::string move;
    //  -1 if not read. The game draws a move's PP in color once it is down
    //  to half, which the digit templates may not read.
    int pp = -1;
    int max_pp = -1;
};

//  The moves from the top, up to the first row whose name can't be read.
std::vector<SummaryMove> read_summary_moves(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The row (0 to 4) the red cursor box is on, if one shows.
std::optional<size_t> read_summary_move_cursor(const ImageViewRGB32& screen);


void add_tests_SummaryMovesReader(UnitTestDatabase& database);



}
}
}
#endif

/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryLanguageList.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

enum class ListBox{
    HIGHLIGHTED,
    PLAIN,
    OTHER,
};

//  A strip at the left end of entry `index`'s box, left of its text and
//  right of the cursor's arrow. The boxes are 576 x 84 at 1920 x 1080, the
//  columns' left edges at x 347 and 996, the rows' tops 108 apart from
//  y 342; the strip is 65 x 54 from 38 right of the edge and 15 down.
ImageFloatBox language_box_strip(size_t index){
    const size_t column = index / LANGUAGE_LIST_ROWS;
    const size_t row = index % LANGUAGE_LIST_ROWS;
    return ImageFloatBox(column == 0 ? 0.2005 : 0.5365, 0.331 + 0.1 * (double)row, 0.034, 0.05);
}

//  Measured on console B (2026-11-13): the highlighted box is 255 white,
//  flat; the others slate blue, about (54, 60-76, 82-107), each channel's
//  spread under 3.
ListBox classify_list_box(const ImageStats& stats){
    if (stats.count == 0){
        return ListBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return ListBox::HIGHLIGHTED;
    }
    const bool slate =
        average.r >= 35 && average.r <= 90 &&
        average.g >= 45 && average.g <= 110 &&
        average.b >= 65 && average.b <= 140 &&
        average.b >= average.r + 15;
    if (slate && spread <= 20){
        return ListBox::PLAIN;
    }
    return ListBox::OTHER;
}

//  The dark blue background between the columns (about (9, 24, 56)).
bool column_gap_dark(const ImageViewRGB32& frame){
    const ImageStats gap = image_stats(extract_box_reference(frame, ImageFloatBox(0.4896, 0.331, 0.0208, 0.447)));
    return gap.count > 0 && gap.average.sum() / 3 < 70;
}

constexpr size_t LOOK_COLUMNS = 4;

//  The look picker's tiles are 240 x 240 at 1920 x 1080, their left edges
//  at x 373, 685, 997 and 1309, their tops at y 324 and 609.
double look_tile_left(size_t look){
    return (373.0 + 312.0 * (double)((look - 1) % LOOK_COLUMNS)) / 1920;
}
double look_tile_top(size_t look){
    return (look <= LOOK_COLUMNS ? 324.0 : 609.0) / 1080;
}

//  Corner `corner` of look `look`'s tile (0 top left, 1 top right, 2 bottom
//  left, 3 bottom right): 12 x 12, 10 in from both edges. The highlighted
//  look's white card reaches about 12 past the tile on every side, so the
//  patches stay on it while it is enlarged.
ImageFloatBox look_corner_patch(size_t look, size_t corner){
    const double dx = (corner % 2 == 0 ? 10.0 : 218.0) / 1920;
    const double dy = (corner < 2 ? 10.0 : 218.0) / 1080;
    return ImageFloatBox(look_tile_left(look) + dx, look_tile_top(look) + dy, 12.0 / 1920, 12.0 / 1080);
}

//  The face in the middle of look `look`'s tile: 60 x 60 from 90 right of
//  its edge and 100 down.
ImageFloatBox look_face_patch(size_t look){
    return ImageFloatBox(look_tile_left(look) + 90.0 / 1920, look_tile_top(look) + 100.0 / 1080, 60.0 / 1920, 60.0 / 1080);
}

double brightness(const ImageStats& stats){
    return stats.average.sum() / 3;
}

enum class LookCorner{
    WHITE,
    DARK,
    OTHER,
};

//  Measured on console B's look pickers (2026-11-13, 67 frames in English,
//  German and Japanese): the highlighted card's corners (240-248, 245-250,
//  246-251), spread under 17; the others slate or, past a tile's rounded
//  corner, the background: (21-34, 29-57, 56-91), spread under 32.
LookCorner classify_look_corner(const ImageStats& stats){
    if (stats.count == 0){
        return LookCorner::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 40){
        return LookCorner::WHITE;
    }
    if (brightness(stats) < 90 && average.r <= 50 && average.b >= average.r + 20 && spread <= 50){
        return LookCorner::DARK;
    }
    return LookCorner::OTHER;
}

//  How many of look `look`'s 4 corners are white; nullopt if the tile is
//  not a look picker tile: a corner neither white nor dark, or no face in
//  its middle (on the same frames brightness 108-196, spread over 117).
std::optional<size_t> look_white_corners(const ImageViewRGB32& frame, size_t look){
    const ImageStats face = image_stats(extract_box_reference(frame, look_face_patch(look)));
    if (face.count == 0 || brightness(face) < 80 || face.stddev.sum() < 60){
        return std::nullopt;
    }
    size_t white = 0;
    for (size_t corner = 0; corner < 4; corner++){
        switch (classify_look_corner(image_stats(extract_box_reference(frame, look_corner_patch(look, corner))))){
        case LookCorner::WHITE:
            white++;
            break;
        case LookCorner::DARK:
            break;
        case LookCorner::OTHER:
            return std::nullopt;
        }
    }
    return white;
}

//  Whether `frame` is the look picker: all 8 tiles have a face and corners
//  that are white or dark. If so, `highlighted` is the one look with 4
//  white corners while the other 7 have none; with any other mix (the card
//  moving between looks) no look is highlighted.
bool read_look_picker(const ImageViewRGB32& frame, std::optional<size_t>& highlighted){
    highlighted.reset();
    if (!frame || frame.width() < 160 || frame.height() < 90){
        return false;
    }
    std::optional<size_t> card;
    size_t lit_tiles = 0;
    for (size_t look = 1; look <= LOOK_COUNT; look++){
        const std::optional<size_t> white = look_white_corners(frame, look);
        if (!white){
            return false;
        }
        if (*white > 0){
            lit_tiles++;
        }
        if (*white == 4){
            card = look;
        }
    }
    if (lit_tiles == 1 && card){
        highlighted = card;
    }
    return true;
}

//  The right ends of the "Confirm Choices" rows, right of their text: the
//  boxes span x 144 to 742 at 1920 x 1080, the two choices from y 578 and
//  662 (68 high), "Ready!" from y 790 (112 high). Each strip is 100 wide
//  from x 620.
ImageFloatBox confirm_row_strip(size_t row){
    switch (row){
    case 0:
        return ImageFloatBox(620.0 / 1920, 592.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    case 1:
        return ImageFloatBox(620.0 / 1920, 676.0 / 1080, 100.0 / 1920, 40.0 / 1080);
    default:
        return ImageFloatBox(620.0 / 1920, 820.0 / 1080, 100.0 / 1920, 60.0 / 1080);
    }
}

//  A "Confirm Choices" row's strip: the highlighted row white, flat, as the
//  language list's; the others a darker slate than its boxes. The
//  highlight's glow pulses over the row next to it: on console B's frames
//  in ten languages (2026-11-13) that row's spread went up to 20.
ListBox classify_confirm_row(const ImageStats& stats){
    if (stats.count == 0){
        return ListBox::OTHER;
    }
    const FloatPixel& average = stats.average;
    const double spread = stats.stddev.sum();
    if (average.r >= 200 && average.g >= 200 && average.b >= 200 && spread <= 30){
        return ListBox::HIGHLIGHTED;
    }
    const bool slate =
        average.r >= 20 && average.r <= 55 &&
        average.g >= 40 && average.g <= 80 &&
        average.b >= 65 && average.b <= 115 &&
        average.b >= average.r + 25;
    if (slate && spread <= 30){
        return ListBox::PLAIN;
    }
    return ListBox::OTHER;
}

//  The dark blue, dotted background round the rows: between the choices and
//  "Ready!", and right of all three. The highlight's blue glow reaches 40
//  pixels out and fills the gap under "Change looks" when that row is
//  highlighted: (12-13, 56-59, 102-106), spread up to 51, against the plain
//  background's (10-11, 29-37, 63-75), spread up to 31 (console B, ten
//  languages, 2026-11-13). Both are dark and blue with little red; the
//  intro's other screens are not.
bool confirm_background_dark(const ImageViewRGB32& frame){
    const ImageFloatBox boxes[] = {
        ImageFloatBox(620.0 / 1920, 742.0 / 1080, 100.0 / 1920, 40.0 / 1080),
        ImageFloatBox(770.0 / 1920, 580.0 / 1080, 30.0 / 1920, 320.0 / 1080),
    };
    for (const ImageFloatBox& box : boxes){
        const ImageStats stats = image_stats(extract_box_reference(frame, box));
        if (stats.count == 0 || brightness(stats) >= 90 || stats.stddev.sum() > 80){
            return false;
        }
        if (stats.average.r > 30 || stats.average.b < stats.average.r + 40){
            return false;
        }
    }
    return true;
}

} // namespace


std::optional<std::vector<IntroKey>> confirm_choices_moves(size_t cursor){
    if (cursor > CONFIRM_CHOICES_READY_ROW){
        return std::nullopt;
    }
    std::vector<IntroKey> moves(CONFIRM_CHOICES_READY_ROW - cursor, IntroKey::DOWN);
    moves.push_back(IntroKey::A);
    return moves;
}


std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 160 || frame.height() < 90){
        return std::nullopt;
    }
    if (!column_gap_dark(frame)){
        return std::nullopt;
    }
    std::optional<size_t> highlighted;
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        const ImageStats stats = image_stats(extract_box_reference(frame, language_box_strip(index)));
        switch (classify_list_box(stats)){
        case ListBox::HIGHLIGHTED:
            if (highlighted){
                return std::nullopt;
            }
            highlighted = index;
            break;
        case ListBox::PLAIN:
            break;
        case ListBox::OTHER:
            return std::nullopt;
        }
    }
    return highlighted;
}

bool is_language_list(const ImageViewRGB32& frame){
    return read_language_list_cursor(frame).has_value();
}

std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame){
    std::optional<size_t> highlighted;
    if (!read_look_picker(frame, highlighted)){
        return std::nullopt;
    }
    return highlighted;
}

bool is_look_picker(const ImageViewRGB32& frame){
    std::optional<size_t> highlighted;
    return read_look_picker(frame, highlighted);
}

//  Measured on console B (2026-11-13), "Confirm Choices" as it opens: the
//  highlighted row 255 white, flat; the others slate, (35-36, 52-61,
//  82-95), spread under 10; the background (10, 29-31, 64-66), spread under
//  25. The language list's boxes and the look picker's tiles fall on the
//  background's strips, so neither reads as this screen.
std::optional<size_t> read_confirm_choices_cursor(const ImageViewRGB32& frame){
    if (!frame || frame.width() < 160 || frame.height() < 90){
        return std::nullopt;
    }
    if (!confirm_background_dark(frame)){
        return std::nullopt;
    }
    std::optional<size_t> highlighted;
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        const ImageStats stats = image_stats(extract_box_reference(frame, confirm_row_strip(row)));
        switch (classify_confirm_row(stats)){
        case ListBox::HIGHLIGHTED:
            if (highlighted){
                return std::nullopt;
            }
            highlighted = row;
            break;
        case ListBox::PLAIN:
            break;
        case ListBox::OTHER:
            return std::nullopt;
        }
    }
    return highlighted;
}

bool is_confirm_choices(const ImageViewRGB32& frame){
    return read_confirm_choices_cursor(frame).has_value();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

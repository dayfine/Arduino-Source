/*  PokemonLZA AutoStory - New game intro screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <string_view>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t LANGUAGE_LIST_ROWS = 5;
constexpr size_t LANGUAGE_LIST_SIZE = 2 * LANGUAGE_LIST_ROWS;
constexpr size_t LOOK_COLUMNS = 4;
constexpr size_t LOOK_COUNT = 2 * LOOK_COLUMNS;

bool usable(const ImageViewRGB32& frame){
    return frame && frame.width() >= 160 && frame.height() >= 90;
}

double brightness(const ImageStats& stats){
    return stats.average.sum() / 3;
}

ImageStats box_stats(const ImageViewRGB32& frame, const ImageFloatBox& box){
    return image_stats(extract_box_reference(frame, box));
}

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

//  The highlighted box is 255 white, flat; the others slate blue, about
//  (54, 60-76, 82-107), each channel's spread under 3.
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
    const ImageStats gap = box_stats(frame, ImageFloatBox(0.4896, 0.331, 0.0208, 0.447));
    return gap.count > 0 && brightness(gap) < 70;
}

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

enum class LookCorner{
    WHITE,
    DARK,
    OTHER,
};

//  Measured on the console's look pickers (67 frames in English, German
//  and Japanese): the highlighted card's corners (240-248, 245-250,
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
    const ImageStats face = box_stats(frame, look_face_patch(look));
    if (face.count == 0 || brightness(face) < 80 || face.stddev.sum() < 60){
        return std::nullopt;
    }
    size_t white = 0;
    for (size_t corner = 0; corner < 4; corner++){
        switch (classify_look_corner(box_stats(frame, look_corner_patch(look, corner)))){
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

//  The look picker as read from `frame`: whether it shows, and the look
//  highlighted, the one with 4 white corners while the other 7 have none.
struct LookPickerReading{
    bool shows = false;
    std::optional<size_t> highlighted;
};

LookPickerReading read_look_picker(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return {};
    }
    std::optional<size_t> card;
    size_t lit_tiles = 0;
    for (size_t look = 1; look <= LOOK_COUNT; look++){
        const std::optional<size_t> white = look_white_corners(frame, look);
        if (!white){
            return {};
        }
        if (*white > 0){
            lit_tiles++;
        }
        if (*white == 4){
            card = look;
        }
    }
    LookPickerReading reading{.shows = true};
    if (lit_tiles == 1 && card){
        reading.highlighted = card;
    }
    return reading;
}

//  The number at `text[pos]`, moving `pos` past it; nullopt when no digit
//  is there.
std::optional<size_t> read_digits(std::string_view text, size_t& pos){
    const size_t start = pos;
    size_t value = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9' && pos - start < 4){
        value = value * 10 + (size_t)(text[pos] - '0');
        pos++;
    }
    if (pos == start){
        return std::nullopt;
    }
    return value;
}

void skip_spaces(std::string_view text, size_t& pos){
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r')){
        pos++;
    }
}

//  The limit N in a counter's text "0/N"; nullopt for any other text.
std::optional<size_t> parse_counter(std::string_view text){
    size_t pos = 0;
    skip_spaces(text, pos);
    if (!read_digits(text, pos)){
        return std::nullopt;
    }
    skip_spaces(text, pos);
    if (pos >= text.size() || text[pos] != '/'){
        return std::nullopt;
    }
    pos++;
    skip_spaces(text, pos);
    const std::optional<size_t> limit = read_digits(text, pos);
    skip_spaces(text, pos);
    if (!limit || *limit == 0 || pos != text.size()){
        return std::nullopt;
    }
    return limit;
}

//  `box` of `frame` as black text on white for OCR: the pixels with every
//  channel at least `min_channel` are the text.
std::string read_light_text(const ImageViewRGB32& frame, const ImageFloatBox& box, Language language, uint8_t min_channel){
    const uint32_t mins = 0xff000000 | ((uint32_t)min_channel << 16) | ((uint32_t)min_channel << 8) | min_channel;
    const ImageRGB32 filtered = to_blackwhite_rgb32_range(
        extract_box_reference(frame, box), /*in_range_black=*/true, mins, /*maxs=*/0xffffffff
    );
    return OCR::ocr_read(language, filtered);
}

} // namespace


std::optional<size_t> read_language_list_cursor(const ImageViewRGB32& frame){
    if (!usable(frame) || !column_gap_dark(frame)){
        return std::nullopt;
    }
    std::optional<size_t> highlighted;
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        switch (classify_list_box(box_stats(frame, language_box_strip(index)))){
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

std::optional<size_t> read_look_picker_cursor(const ImageViewRGB32& frame){
    return read_look_picker(frame).highlighted;
}

bool is_look_picker(const ImageViewRGB32& frame){
    return read_look_picker(frame).shows;
}

//  The question area (x 1056-1824, y 54-184 at 1920 x 1080) is dark and
//  flat: 27, spread 3 on all ten languages' keyboards; the intro's other
//  screens are 46 or brighter there, or busy. The keys' bottom row
//  (y 929-950) averages 205-211; elsewhere 160 at most.
bool is_name_keyboard(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return false;
    }
    const ImageStats question = box_stats(frame, ImageFloatBox(0.55, 0.05, 0.40, 0.12));
    if (question.count == 0 || brightness(question) > 40 || question.stddev.sum() > 15){
        return false;
    }
    const ImageStats keys = box_stats(frame, ImageFloatBox(0.03, 0.86, 0.80, 0.02));
    return keys.count > 0 && brightness(keys) >= 185;
}

//  The counter is grey (about 128) on the dark question area: with a
//  12-character field ("0/12") at x 1645-1725, y 325-355; with a 6-character
//  one ("0/6") at x 1275-1335, y 355-385 (Japanese) or 340-370 (Korean,
//  Chinese, whose keyboards are taller). The field's underline runs just
//  above each, so each layout gets its own box.
std::optional<size_t> read_name_keyboard_limit(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return std::nullopt;
    }
    const ImageFloatBox boxes[] = {
        ImageFloatBox(1625.0 / 1920, 312.0 / 1080, 120.0 / 1920, 53.0 / 1080),
        ImageFloatBox(1258.0 / 1920, 345.0 / 1080, 115.0 / 1920, 50.0 / 1080),
        ImageFloatBox(1258.0 / 1920, 328.0 / 1080, 115.0 / 1920, 50.0 / 1080),
    };
    for (const ImageFloatBox& box : boxes){
        const std::optional<size_t> limit = parse_counter(read_light_text(frame, box, Language::English, /*min_channel=*/90));
        if (limit){
            return limit;
        }
    }
    return std::nullopt;
}

//  The values right of the "Language" and "Name" labels: white, from x 345;
//  the language's row y 400-450, the name's 468-522.
std::string read_confirm_choices_name(const ImageViewRGB32& frame){
    if (!usable(frame)){
        return "";
    }
    const std::string text = read_light_text(
        frame, ImageFloatBox(340.0 / 1920, 468.0 / 1080, 760.0 / 1920, 54.0 / 1080), Language::English, /*min_channel=*/150
    );
    std::string name;
    for (const char ch : text){
        if (ch != ' ' && ch != '\n' && ch != '\r'){
            name += ch;
        }
    }
    return name;
}

std::string read_confirm_choices_language(const ImageViewRGB32& frame, Language language){
    if (!usable(frame)){
        return "";
    }
    return read_light_text(
        frame, ImageFloatBox(340.0 / 1920, 400.0 / 1080, 760.0 / 1920, 50.0 / 1080), language, /*min_channel=*/150
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

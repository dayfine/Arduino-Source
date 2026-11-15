/*  Home Tile Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <utility>
#include <vector>
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_StringNormalization.h"
#include "Tests/ImageUnitTest.h"
#include "NintendoSwitch_HomeTileReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


namespace{

//  The selected tile's frame lines, about 0.008 of the screen tall, just
//  outside the tile art: strips across the screen that the art of the
//  other tiles stays out of.
const ImageFloatBox FRAME_TOP_STRIP{0.0, 0.252, 1.0, 0.014};
const ImageFloatBox FRAME_BOTTOM_STRIP{0.0, 0.627, 1.0, 0.014};
constexpr double FRAME_TOP = 0.256;
constexpr double FRAME_BOTTOM = 0.637;
//  A column crosses a frame line when this many of its pixels in the strip
//  are the frame's cyan.
constexpr size_t MIN_LINE_PIXELS = 3;
//  A tile frame is 0.214 of the screen wide.
constexpr double MIN_FRAME_WIDTH = 0.18;
constexpr double MAX_FRAME_WIDTH = 0.25;

//  The band above the row where HOME writes the selected game's name. The
//  name starts 0.005 to 0.01 left of the frame, right of a game card icon
//  that ends 0.017 or more left of it.
constexpr double NAME_TOP = 0.19;
constexpr double NAME_HEIGHT = 0.06;
constexpr double NAME_LEAD = 0.015;
constexpr double NAME_RIGHT = 0.98;
//  The name's cyan (ARGB), dark text on HOME's light background once
//  filtered: red low, green and blue high.
constexpr uint32_t NAME_MINS = 0xff000096;
constexpr uint32_t NAME_MAXS = 0xff96ffff;

//  The frame's cyan, which pulses between about (0, 187, 229) and
//  (52, 249, 229), with lighter anti-aliased edges.
bool is_frame_cyan(uint32_t pixel){
    const int red = (pixel >> 16) & 0xff;
    const int green = (pixel >> 8) & 0xff;
    const int blue = pixel & 0xff;
    return green >= 160 && blue >= 160 && green - red >= 60 && blue - red >= 60;
}

//  Per column of `strip`, whether it crosses a frame line.
std::vector<bool> frame_line_columns(const ImageViewRGB32& strip){
    std::vector<bool> columns(strip.width(), false);
    for (size_t x = 0; x < strip.width(); x++){
        size_t cyan = 0;
        for (size_t y = 0; y < strip.height(); y++){
            cyan += is_frame_cyan(strip.pixel(x, y)) ? 1 : 0;
        }
        columns[x] = cyan >= MIN_LINE_PIXELS;
    }
    return columns;
}

//  The longest run of columns that cross both frame lines, as [begin, end).
std::pair<size_t, size_t> longest_framed_run(const std::vector<bool>& top, const std::vector<bool>& bottom){
    std::pair<size_t, size_t> best{0, 0};
    size_t begin = 0;
    for (size_t x = 0; x <= top.size(); x++){
        const bool framed = x < top.size() && x < bottom.size() && top[x] && bottom[x];
        if (framed){
            continue;
        }
        if (x - begin > best.second - best.first){
            best = {begin, x};
        }
        begin = x + 1;
    }
    return best;
}

} // namespace


std::optional<ImageFloatBox> find_selected_home_tile(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return std::nullopt;
    }
    const std::pair<size_t, size_t> run = longest_framed_run(
        frame_line_columns(extract_box_reference(screen, FRAME_TOP_STRIP)),
        frame_line_columns(extract_box_reference(screen, FRAME_BOTTOM_STRIP))
    );
    const double width = (double)screen.width();
    const double frame_width = (double)(run.second - run.first) / width;
    if (frame_width < MIN_FRAME_WIDTH || frame_width > MAX_FRAME_WIDTH){
        return std::nullopt;
    }
    return ImageFloatBox((double)run.first / width, FRAME_TOP, frame_width, FRAME_BOTTOM - FRAME_TOP);
}

std::optional<std::string> read_selected_home_tile_name(Language language, const ImageViewRGB32& screen){
    const std::optional<ImageFloatBox> frame = find_selected_home_tile(screen);
    if (!frame){
        return std::nullopt;
    }
    const double left = std::max(0.0, frame->x - NAME_LEAD);
    const ImageViewRGB32 band = extract_box_reference(screen, ImageFloatBox(left, NAME_TOP, NAME_RIGHT - left, NAME_HEIGHT));
    const ImageRGB32 text = to_blackwhite_rgb32_range(band, true, NAME_MINS, NAME_MAXS);
    return OCR::ocr_read(language, text);
}

bool selected_home_tile_is(Language language, const ImageViewRGB32& screen, std::string_view name){
    const std::optional<std::string> read = read_selected_home_tile_name(language, screen);
    if (!read){
        return false;
    }
    const std::u32string wanted = OCR::normalize_utf32(std::string(name));
    return !wanted.empty() && OCR::normalize_utf32(*read).find(wanted) != std::u32string::npos;
}



namespace{

//  The names HOME shows for the two games on console B's frames.
constexpr std::string_view FIRERED = "Pokémon FireRed Version";
constexpr std::string_view LEGENDS_ZA = "Pokémon Legends: Z-A";

//  Where the first tile's frame starts, and the step to the next tile, as
//  parts of the screen's width, before the row scrolls.
constexpr double FIRST_TILE_LEFT = 0.076;
constexpr double TILE_STEP = 0.211;
constexpr double TILE_TOLERANCE = 0.02;

struct HomeTileCase{
    struct Input{
        std::string_view image;
    };
    struct Expectation{
        //  The selected tile's place in the row, from 0; nothing: no tile
        //  is selected.
        std::optional<size_t> tile = std::nullopt;
        //  The name the selected tile shows: FIRERED or LEGENDS_ZA.
        std::string_view game = {};
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

//  Console B, 2026-11-14: HOME after the order changed (the game played
//  last moves to the first tile), and the user select over HOME.
const std::vector<HomeTileCase> HOME_TILE_CASES = {
    {.name = "FireRed on the first tile 1",
     .input = {.image = "NintendoSwitch/HomeTileReader/FireRedFirstTile_1.png"},
     .expectation = {.tile = 0, .game = FIRERED}},
    {.name = "FireRed on the first tile 2",
     .input = {.image = "NintendoSwitch/HomeTileReader/FireRedFirstTile_2.png"},
     .expectation = {.tile = 0, .game = FIRERED}},
    {.name = "FireRed on the first tile 3",
     .input = {.image = "NintendoSwitch/HomeTileReader/FireRedFirstTile_3.png"},
     .expectation = {.tile = 0, .game = FIRERED}},
    {.name = "Legends Z-A on the first tile",
     .input = {.image = "NintendoSwitch/HomeTileReader/LegendsZAFirstTile.png"},
     .expectation = {.tile = 0, .game = LEGENDS_ZA}},
    {.name = "Legends Z-A on the second tile",
     .input = {.image = "NintendoSwitch/HomeTileReader/LegendsZASecondTile.png"},
     .expectation = {.tile = 1, .game = LEGENDS_ZA}},
    {.name = "user select over HOME",
     .input = {.image = "NintendoSwitch/HomeTileReader/UserSelectOverHome.png"},
     .expectation = {}},
};

//  What went wrong with the frame on `screen`; empty when it is where `test` expects.
std::string check_frame(const HomeTileCase& test, const ImageViewRGB32& screen){
    const std::optional<ImageFloatBox> frame = find_selected_home_tile(screen);
    if (!test.expectation.tile){
        return frame ? std::format("Expected no selected tile, found a frame at x={:.3f}.", frame->x) : "";
    }
    if (!frame){
        return std::format("Expected a frame on tile {}, found none.", *test.expectation.tile);
    }
    const double expected_left = FIRST_TILE_LEFT + TILE_STEP * (double)*test.expectation.tile;
    if (std::abs(frame->x - expected_left) > TILE_TOLERANCE){
        return std::format("Expected the frame on tile {} (x={:.3f}), found it at x={:.3f}.", *test.expectation.tile, expected_left, frame->x);
    }
    return "";
}

//  What went wrong with the names on `screen`: it must be the game `test`
//  expects and not the other one.
std::string check_names(const HomeTileCase& test, const ImageViewRGB32& screen){
    const std::string read = read_selected_home_tile_name(Language::English, screen).value_or("<no tile>");
    for (std::string_view game : {FIRERED, LEGENDS_ZA}){
        const bool expected = game == test.expectation.game;
        if (selected_home_tile_is(Language::English, screen, game) != expected){
            return std::format("Expected the tile {} \"{}\", read \"{}\".", expected ? "to be" : "not to be", game, read);
        }
    }
    return "";
}

class Test_HomeTileReader : public ImageUnitTest{
public:
    explicit Test_HomeTileReader(const HomeTileCase& test)
        : ImageUnitTest(std::format("NintendoSwitch::HomeTileReader - {}", test.name), {std::string(test.input.image)})
        , m_case(test)
    {
        //  Plus the PaddleOCR (ONNX) recognition session and its working set.
        m_memory += PADDLE_OCR_SINGLE_LINE_MEMORY_ESTIMATE;
    }

    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        std::string error = check_frame(m_case, images[0]);
        if (error.empty()){
            error = check_names(m_case, images[0]);
        }
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    HomeTileCase m_case;
};

} // namespace


void add_tests_HomeTileReader(UnitTestDatabase& database){
    for (const HomeTileCase& test : HOME_TILE_CASES){
        database.add<Test_HomeTileReader>(test);
    }
}



} // namespace NintendoSwitch
} // namespace PokemonAutomation

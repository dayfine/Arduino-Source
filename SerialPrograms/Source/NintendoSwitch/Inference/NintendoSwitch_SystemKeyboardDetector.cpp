/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "Tests/ImageUnitTest.h"
#include "NintendoSwitch_SystemKeyboardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{

//  Centres of the five rows of keys, and of the first column and the spacing
//  between columns, as fractions of the screen.
constexpr double KEY_ROW_Y[] = {0.500, 0.593, 0.681, 0.770, 0.858};
constexpr double KEY_COL_X0 = 0.077;
constexpr double KEY_COL_STEP = 0.075;

//  In the right column, Return spans rows 1-2 and OK rows 3-4. These are the
//  gaps between those keys.
constexpr double RIGHT_COLUMN_RETURN_TOP = 0.548;
constexpr double RIGHT_COLUMN_OK_TOP = 0.725;

//  The cursor border and everything else the reader looks at, from the top of the
//  digit row to the bottom of the bottom row.
const ImageFloatBox KEYS_AREA(0.0, 0.43, 1.0, 0.49);

//  The labels of the bottom row's page keys (globe to the accent pages), inside
//  the border a cursor on one of them draws (y 875-979 at 1080). In the dark
//  theme the current page's label and underline are cyan (about 0, 230, 210,
//  y 909-955 for ABC), like the cursor, so the cursor read skips this band.
const ImageFloatBox PAGE_LABELS(0.0, 0.829, 0.49, 0.065);

//  The bar below the keys, which tells the console's theme.
const ImageFloatBox BOTTOM_BAR(0.10, 0.935, 0.35, 0.030);

enum class KeyboardTheme{
    LIGHT,
    DARK,   //  The console's "Basic Black" theme.
};

//  The bar is white (245) in the light theme and flat grey (76) in the dark
//  one; nothing if it is neither. Measured on Legends: Z-A.
std::optional<KeyboardTheme> keyboard_theme(const ImageViewRGB32& screen){
    const ImageStats bar = image_stats(extract_box_reference(screen, BOTTOM_BAR));
    if (is_white(bar)){
        return KeyboardTheme::LIGHT;
    }
    if (is_grey(bar, /*min_rgb_sum=*/180, /*max_rgb_sum=*/270)){
        return KeyboardTheme::DARK;
    }
    return std::nullopt;
}

bool in_box(double x, double y, const ImageFloatBox& box){
    return x >= box.x && x < box.x + box.width && y >= box.y && y < box.y + box.height;
}

//  The cursor border pulses between bright cyan (about 50, 240, 220) and a darker
//  teal (about 0, 185, 210), measured on Legends: Z-A; in the dark theme it is
//  about (115, 245, 250). The blue of the OK button and of the light theme's ABC
//  underline (40, 90, 250) is not green enough to match.
bool is_cursor_pixel(uint32_t pixel){
    const int red = (pixel >> 16) & 0xff;
    const int green = (pixel >> 8) & 0xff;
    const int blue = pixel & 0xff;
    return red < 130 && green > 160 && blue > 170 && green - red > 60;
}

uint8_t nearest_row(double y){
    uint8_t row = 0;
    for (uint8_t r = 1; r < 5; r++){
        if (std::abs(KEY_ROW_Y[r] - y) < std::abs(KEY_ROW_Y[row] - y)){
            row = r;
        }
    }
    return row;
}

//  The key whose cursor border has its centre at (x, y), or nothing if that is
//  off the keyboard.
std::optional<SystemKeyboardKey> key_at(double x, double y){
    const long col = std::lround((x - KEY_COL_X0) / KEY_COL_STEP);
    if (col < 0 || col > 11){
        return std::nullopt;
    }
    if (col < 11){
        return SystemKeyboardKey{nearest_row(y), (uint8_t)col};
    }
    if (y < RIGHT_COLUMN_RETURN_TOP){
        return SystemKeyboardKey{0, 11};
    }
    if (y < RIGHT_COLUMN_OK_TOP){
        return SystemKeyboardKey{1, 11};
    }
    return SystemKeyboardKey{3, 11};
}

//  The key whose cell holds (x, y), for a point anywhere in KEYS_AREA: the
//  columns left of the first and right of the right column belong to them.
SystemKeyboardKey key_cell_at(double x, double y){
    const double col = std::clamp((x - KEY_COL_X0) / KEY_COL_STEP, 0.0, 11.0);
    return *key_at(KEY_COL_X0 + col * KEY_COL_STEP, y);
}

//  The cyan pixels in one key's cell, against all pixels of the cell.
struct KeyCellPixels{
    double area = 0;
    double count = 0;
    double sum_x = 0;
    double sum_y = 0;
};

//  The cursor's border covers about a fifth of its key's cell. A key whose cell
//  is more cyan than this is solid cyan, not the cursor: the dark theme's OK
//  button once the field has text (0, 233, 211; 60% of its cell).
constexpr double MAX_BORDER_SHARE = 0.35;

//  The cyan pixels of each key's cell in `keys` (KEYS_AREA), by row and column.
using KeyCells = std::array<std::array<KeyCellPixels, 12>, 5>;
KeyCells cyan_pixels_by_key(const ImageViewRGB32& keys){
    KeyCells cells{};
    for (size_t y = 0; y < keys.height(); y++){
        const double screen_y = KEYS_AREA.y + ((double)y + 0.5) / keys.height() * KEYS_AREA.height;
        for (size_t x = 0; x < keys.width(); x++){
            const double screen_x = KEYS_AREA.x + ((double)x + 0.5) / keys.width() * KEYS_AREA.width;
            if (in_box(screen_x, screen_y, PAGE_LABELS)){
                continue;
            }
            const SystemKeyboardKey key = key_cell_at(screen_x, screen_y);
            KeyCellPixels& cell = cells[key.row][key.col];
            cell.area++;
            if (is_cursor_pixel(keys.pixel(x, y))){
                cell.count++;
                cell.sum_x += (double)x;
                cell.sum_y += (double)y;
            }
        }
    }
    return cells;
}

}  // namespace



SystemKeyboardDetector::SystemKeyboardDetector(Color color)
    : m_color(color)
    , m_key_panel_upper(0.15, 0.532, 0.65, 0.036)
    , m_key_panel_lower(0.15, 0.715, 0.65, 0.032)
    , m_bottom_bar(BOTTOM_BAR)
    , m_ok_button(0.875, 0.760, 0.045, 0.020)
{}
void SystemKeyboardDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_key_panel_upper);
    items.add(m_color, m_key_panel_lower);
    items.add(m_color, m_bottom_bar);
    items.add(m_color, m_ok_button);
}
bool SystemKeyboardDetector::detect(const ImageViewRGB32& screen){
    const std::optional<KeyboardTheme> theme = keyboard_theme(screen);
    if (!theme){
        return false;
    }
    //  Each strip crosses the keys and the gaps between them, which gives a
    //  stddev sum of about 11 (white keys, measured on BDSP and Legends: Z-A)
    //  or 12 (dark grey keys, about 84, on Legends: Z-A).
    const double max_panel_stddev = 20;
    const ImageStats upper = image_stats(extract_box_reference(screen, m_key_panel_upper));
    const ImageStats lower = image_stats(extract_box_reference(screen, m_key_panel_lower));
    const ImageStats ok = image_stats(extract_box_reference(screen, m_ok_button));
    if (*theme == KeyboardTheme::DARK){
        const bool panel =
            is_grey(upper, /*min_rgb_sum=*/210, /*max_rgb_sum=*/300, max_panel_stddev) ||
            is_grey(lower, /*min_rgb_sum=*/210, /*max_rgb_sum=*/300, max_panel_stddev);
        //  OK is flat grey (100, 100, 100) while the field is empty and solid
        //  cyan (0, 233, 211) with text.
        return panel && ok.stddev.sum() < 10;
    }
    const bool panel =
        is_white(upper, /*min_rgb_sum=*/500, max_panel_stddev) ||
        is_white(lower, /*min_rgb_sum=*/500, max_panel_stddev);
    if (!panel){
        return false;
    }
    //  OK is blue (40, 93, 255), or flat grey (221, 221, 221) while the field is
    //  empty (disabled). Measured on BDSP and Legends: Z-A.
    return is_solid(ok, {0.103, 0.240, 0.657}) || is_grey(ok, 600, 690);
}


std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen){
    //  The cursor is a cyan border around one key. Neighbouring keys overlap that border,
    //  so take the centroid of all cyan pixels outside solid cyan keys and map it to
    //  the nearest key.
    const ImageViewRGB32 keys = extract_box_reference(screen, KEYS_AREA);
    const KeyCells cells = cyan_pixels_by_key(keys);
    double count = 0, sum_x = 0, sum_y = 0;
    for (const auto& row : cells){
        for (const KeyCellPixels& cell : row){
            if (cell.count > MAX_BORDER_SHARE * cell.area){
                continue;
            }
            count += cell.count;
            sum_x += cell.sum_x;
            sum_y += cell.sum_y;
        }
    }
    //  The border is about 0.18% of the screen; a few stray pixels are not a cursor.
    if (count < 0.0005 * screen.width() * screen.height()){
        return std::nullopt;
    }
    return key_at(
        KEYS_AREA.x + sum_x / count / keys.width() * KEYS_AREA.width,
        KEYS_AREA.y + sum_y / count / keys.height() * KEYS_AREA.height
    );
}

bool system_keyboard_field_has_text(const ImageViewRGB32& screen){
    //  Left part of the backspace key, clear of its icon and of the cursor border.
    //  Light theme: light grey (221) with an empty field, dark grey (52) with
    //  text. Dark theme: grey (100) with an empty field, white (254) with text.
    const ImageStats stats = image_stats(extract_box_reference(screen, ImageFloatBox(0.878, 0.470, 0.008, 0.060)));
    if (keyboard_theme(screen) == KeyboardTheme::DARK){
        return !is_grey(stats, /*min_rgb_sum=*/255, /*max_rgb_sum=*/345);
    }
    return stats.average.sum() < 400;
}

size_t system_keyboard_text_pixels(const ImageViewRGB32& screen){
    //  The field, above its underline, which is white in some games.
    const ImageViewRGB32 field = extract_box_reference(screen, ImageFloatBox(0.03, 0.17, 0.87, 0.105));
    size_t count = 0;
    for (size_t y = 0; y < field.height(); y++){
        for (size_t x = 0; x < field.width(); x++){
            const uint32_t pixel = field.pixel(x, y);
            if (((pixel >> 16) & 0xff) > 190 && ((pixel >> 8) & 0xff) > 190 && (pixel & 0xff) > 190){
                count++;
            }
        }
    }
    return count;
}



namespace{

//  One screenshot and what each function should read from it.
class Test_SystemKeyboardDetector : public ImageUnitTest{
public:
    //  `cursor`: the key under the cursor, or nothing for a screen without the keyboard.
    Test_SystemKeyboardDetector(const std::string& image, std::optional<SystemKeyboardKey> cursor, bool has_text)
        : ImageUnitTest(std::format("NintendoSwitch::SystemKeyboardDetector - {}", image), {image})
        , m_cursor(cursor)
        , m_has_text(has_text)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const ImageRGB32& image = images[0];
        const bool keyboard = SystemKeyboardDetector().detect(image);
        if (keyboard != m_cursor.has_value()){
            return std::format("detect() returned {}.", keyboard);
        }
        if (!keyboard){
            return true;
        }
        const std::optional<SystemKeyboardKey> cursor = read_system_keyboard_cursor(image);
        if (cursor != m_cursor){
            return cursor
                ? std::format("Read the cursor at ({}, {}).", cursor->row, cursor->col)
                : std::format("Read no cursor.");
        }
        const bool has_text = system_keyboard_field_has_text(image);
        if (has_text != m_has_text){
            return std::format("system_keyboard_field_has_text() returned {}.", has_text);
        }
        //  An empty field has no white text pixels; one typed character has many.
        const size_t text_pixels = system_keyboard_text_pixels(image);
        if ((text_pixels >= 20) != m_has_text){
            return std::format("system_keyboard_text_pixels() returned {}.", text_pixels);
        }
        return true;
    }

private:
    std::optional<SystemKeyboardKey> m_cursor;
    bool m_has_text;
};

std::string test_image_path(std::string_view image){
    return std::format("NintendoSwitch/SystemKeyboardDetector/{}", image);
}

//  The cursor on keys other than "1".
void add_cursor_tests(UnitTestDatabase& database){
    //  Legends: Z-A, English and Spanish prompts. The
    //  border pulses between bright cyan and a darker teal; T, / and ! are dark.
    const std::pair<std::string_view, SystemKeyboardKey> cursors[] = {
        {"PLZA-Cursor-T.png",           {1, 4}},
        {"PLZA-Cursor-Slash.png",       {1, 10}},
        {"PLZA-Cursor-Exclamation.png", {3, 10}},
        {"PLZA-Cursor-0.png",           {0, 9}},
        {"PLZA-Cursor-Minus.png",       {0, 10}},
        {"PLZA-Cursor-Backspace.png",   {0, 11}},
        {"PLZA-Cursor-Return.png",      {1, 11}},
        {"PLZA-Cursor-OK.png",          {3, 11}},
        {"PLZA-Cursor-Globe.png",       {4, 0}},
        {"PLZA-Cursor-NTilde.png",      {4, 5}},
        {"PLZA-Cursor-Space.png",       {4, 8}},
    };
    for (const auto& [image, cursor] : cursors){
        database.add<Test_SystemKeyboardDetector>(test_image_path(image), cursor, false);
    }
}

//  Tests whose screen is not captured yet.
void add_planned_tests(UnitTestDatabase& database){
    const SystemKeyboardKey key_1{0, 0};
    //  Screens still to be captured: AZERTY needs a console whose system language
    //  uses it (the layout follows the system, not the game). Each test is added
    //  once its image is in the test repo.
    struct Planned{
        std::string_view image;
        std::optional<SystemKeyboardKey> cursor;
        bool has_text;
    };
    const Planned planned[] = {
        {"AZERTY-Empty.png",    key_1,          false},
        {"PLZA-Korean.png",     std::nullopt,   false},
    };
    for (const Planned& test : planned){
        const std::string image = test_image_path(test.image);
        if (Filesystem::exists(std::format("{}{}", UNIT_TEST_RESOURCE_PATH(), image))){
            database.add<Test_SystemKeyboardDetector>(image, test.cursor, test.has_text);
        }
    }
}

}  // namespace


void add_tests_SystemKeyboardDetector(UnitTestDatabase& database){
    const SystemKeyboardKey key_1{0, 0};
    //  The OK button is blue with text in the field and grey without.
    database.add<Test_SystemKeyboardDetector>(test_image_path("BDSP-PlayerName-Empty.png"), key_1, false);
    database.add<Test_SystemKeyboardDetector>(test_image_path("BDSP-FriendName-Typed.png"), key_1, true);
    for (std::string_view language : {"English", "French", "German", "Italian", "Japanese", "Spanish"}){
        database.add<Test_SystemKeyboardDetector>(test_image_path(std::format("PLZA-{}-Empty.png", language)), key_1, false);
    }
    //  The console's dark theme (Basic Black), console B 2026-11-18: the cyan
    //  ABC label must not pull the cursor read off "1".
    database.add<Test_SystemKeyboardDetector>(test_image_path("PLZA-English-Empty-Dark.png"), key_1, false);
    //  "K" typed: OK turns solid cyan, which is not the cursor (on "8").
    database.add<Test_SystemKeyboardDetector>(test_image_path("PLZA-English-Typed-Dark.png"), SystemKeyboardKey{0, 7}, true);
    for (std::string_view image : {"BDSP-Dialog.png", "HomeMenu.png", "HomeMenu-OnlineLicensePopup.png", "UserSelect.png"}){
        database.add<Test_SystemKeyboardDetector>(test_image_path(image), std::nullopt, false);
    }
    add_cursor_tests(database);
    add_planned_tests(database);
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

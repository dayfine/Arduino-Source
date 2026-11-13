/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

//  The cursor border pulses between bright cyan (about 50, 240, 220) and a darker
//  teal (about 0, 185, 210), measured on Legends: Z-A. The blue of the OK button
//  and of the ABC underline (40, 90, 250) is not green enough to match.
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

}  // namespace



SystemKeyboardDetector::SystemKeyboardDetector(Color color)
    : m_color(color)
    , m_key_panel_upper(0.15, 0.532, 0.65, 0.036)
    , m_key_panel_lower(0.15, 0.715, 0.65, 0.032)
    , m_bottom_bar(0.10, 0.935, 0.35, 0.030)
    , m_ok_button(0.875, 0.760, 0.045, 0.020)
{}
void SystemKeyboardDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_key_panel_upper);
    items.add(m_color, m_key_panel_lower);
    items.add(m_color, m_bottom_bar);
    items.add(m_color, m_ok_button);
}
bool SystemKeyboardDetector::detect(const ImageViewRGB32& screen){
    //  Each strip crosses white keys and the slightly brighter gaps between them,
    //  which gives a stddev sum of about 11 (measured on BDSP and Legends: Z-A).
    const double max_panel_stddev = 20;
    const bool panel =
        is_white(extract_box_reference(screen, m_key_panel_upper), 500, max_panel_stddev) ||
        is_white(extract_box_reference(screen, m_key_panel_lower), 500, max_panel_stddev);
    if (!panel || !is_white(extract_box_reference(screen, m_bottom_bar))){
        return false;
    }
    //  OK is blue (40, 93, 255), or flat grey (221, 221, 221) while the field is
    //  empty (disabled). Measured on BDSP and Legends: Z-A.
    const ImageStats ok = image_stats(extract_box_reference(screen, m_ok_button));
    return is_solid(ok, {0.103, 0.240, 0.657}) || is_grey(ok, 600, 690);
}


std::optional<SystemKeyboardKey> read_system_keyboard_cursor(const ImageViewRGB32& screen){
    //  The cursor is a cyan border around one key. Neighbouring keys overlap that border,
    //  so take the centroid of all cyan pixels and map it to the nearest key.
    const ImageViewRGB32 keys = extract_box_reference(screen, KEYS_AREA);
    double count = 0, sum_x = 0, sum_y = 0;
    for (size_t y = 0; y < keys.height(); y++){
        for (size_t x = 0; x < keys.width(); x++){
            if (is_cursor_pixel(keys.pixel(x, y))){
                count++;
                sum_x += (double)x;
                sum_y += (double)y;
            }
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
    //  Light grey (221) with an empty field, dark grey (52) with text.
    const ImageStats stats = image_stats(extract_box_reference(screen, ImageFloatBox(0.878, 0.470, 0.008, 0.060)));
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

}  // namespace


void add_tests_SystemKeyboardDetector(UnitTestDatabase& database){
    const auto path = [](std::string_view image){
        return std::format("NintendoSwitch/SystemKeyboardDetector/{}", image);
    };
    const SystemKeyboardKey key_1{0, 0};
    //  The OK button is blue with text in the field and grey without.
    database.add<Test_SystemKeyboardDetector>(path("BDSP-PlayerName-Empty.png"), key_1, false);
    database.add<Test_SystemKeyboardDetector>(path("BDSP-FriendName-Typed.png"), key_1, true);
    for (std::string_view language : {"English", "French", "German", "Italian", "Japanese", "Spanish"}){
        database.add<Test_SystemKeyboardDetector>(path(std::format("PLZA-{}-Empty.png", language)), key_1, false);
    }
    for (std::string_view image : {"BDSP-Dialog.png", "HomeMenu.png", "HomeMenu-OnlineLicensePopup.png", "UserSelect.png"}){
        database.add<Test_SystemKeyboardDetector>(path(image), std::nullopt, false);
    }

    //  The cursor on other keys (Legends: Z-A, English and Spanish prompts). The
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
        database.add<Test_SystemKeyboardDetector>(path(image), cursor, false);
    }

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
        const std::string image = path(test.image);
        if (Filesystem::exists(std::format("{}{}", UNIT_TEST_RESOURCE_PATH(), image))){
            database.add<Test_SystemKeyboardDetector>(image, test.cursor, test.has_text);
        }
    }
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

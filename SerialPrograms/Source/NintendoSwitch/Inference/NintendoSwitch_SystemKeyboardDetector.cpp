/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch_SystemKeyboardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

//  Centres of the four rows of character keys, and of the first column and the
//  spacing between columns, as fractions of the screen.
static constexpr double KEY_ROW_Y[] = {0.500, 0.593, 0.681, 0.770};
static constexpr double KEY_COL_X0 = 0.077;
static constexpr double KEY_COL_STEP = 0.075;


SystemKeyboardDetector::SystemKeyboardDetector(Color color)
    : m_color(color)
    , m_key_panel(0.15, 0.555, 0.65, 0.020)
    , m_bottom_bar(0.10, 0.935, 0.35, 0.030)
    , m_ok_button(0.875, 0.760, 0.045, 0.020)
{}
void SystemKeyboardDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_key_panel);
    items.add(m_color, m_bottom_bar);
    items.add(m_color, m_ok_button);
}
bool SystemKeyboardDetector::detect(const ImageViewRGB32& screen){
    if (!is_white(extract_box_reference(screen, m_key_panel)) ||
        !is_white(extract_box_reference(screen, m_bottom_bar))
    ){
        return false;
    }
    //  OK is blue, or flat grey while the field is empty (disabled).
    const ImageStats ok = image_stats(extract_box_reference(screen, m_ok_button));
    return is_solid(ok, {0.172, 0.276, 0.552}) || is_grey(ok, 600, 690);
}


bool find_system_keyboard_cursor(const ImageViewRGB32& screen, int& row, int& col){
    //  The cursor is a cyan border around one key. Neighbouring keys overlap that border,
    //  so take the centroid of all cyan pixels and map it to the nearest key.
    const ImageFloatBox area(0.02, 0.44, 0.84, 0.38);
    ImageViewRGB32 keys = extract_box_reference(screen, area);
    double count = 0, sum_x = 0, sum_y = 0;
    for (size_t y = 0; y < keys.height(); y++){
        for (size_t x = 0; x < keys.width(); x++){
            const uint32_t pixel = keys.pixel(x, y);
            const int red = (pixel >> 16) & 0xff;
            const int green = (pixel >> 8) & 0xff;
            const int blue = pixel & 0xff;
            if (red < 150 && green > 200 && blue > 190){
                count++;
                sum_x += (double)x;
                sum_y += (double)y;
            }
        }
    }
    //  The border is about 0.15% of the screen; a few stray pixels are not a cursor.
    if (count < 0.0005 * screen.width() * screen.height()){
        return false;
    }
    const double cx = area.x + sum_x / count / keys.width() * area.width;
    const double cy = area.y + sum_y / count / keys.height() * area.height;
    col = (int)std::lround((cx - KEY_COL_X0) / KEY_COL_STEP);
    row = 0;
    for (int r = 1; r < 4; r++){
        if (std::abs(KEY_ROW_Y[r] - cy) < std::abs(KEY_ROW_Y[row] - cy)){
            row = r;
        }
    }
    return 0 <= col && col < 11;
}

bool system_keyboard_field_has_text(const ImageViewRGB32& screen){
    //  Left part of the field, after the caret's position on an empty field.
    const ImageStats stats = image_stats(extract_box_reference(screen, ImageFloatBox(0.11, 0.235, 0.19, 0.030)));
    return stats.stddev.sum() > 30;
}


//  One screenshot and what each function should read from it.
class Test_SystemKeyboardDetector : public UnitTest{
public:
    //  cursor_row/col: -1 for a screen without the keyboard.
    Test_SystemKeyboardDetector(const std::string& image, bool keyboard, int cursor_row, int cursor_col, bool has_text)
        : UnitTest("NintendoSwitch::SystemKeyboardDetector - " + image)
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_keyboard(keyboard)
        , m_cursor_row(cursor_row)
        , m_cursor_col(cursor_col)
        , m_has_text(has_text)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope& scope) const override{
        ImageRGB32 image(m_image);
        if (SystemKeyboardDetector().detect(image) != m_keyboard){
            return false;
        }
        if (!m_keyboard){
            return true;
        }
        int row = -1;
        int col = -1;
        return find_system_keyboard_cursor(image, row, col)
            && row == m_cursor_row && col == m_cursor_col
            && system_keyboard_field_has_text(image) == m_has_text;
    }

private:
    std::string m_image;
    bool m_keyboard;
    int m_cursor_row;
    int m_cursor_col;
    bool m_has_text;
};


void add_tests_SystemKeyboardDetector(UnitTestDatabase& database){
    const std::string folder = "NintendoSwitch/SystemKeyboardDetector/";
    //  The OK button is blue with text in the field and grey without.
    database.add<Test_SystemKeyboardDetector>(folder + "BDSP-PlayerName-Empty.png", true, 0, 0, false);
    database.add<Test_SystemKeyboardDetector>(folder + "BDSP-FriendName-Typed.png", true, 0, 0, true);
    database.add<Test_SystemKeyboardDetector>(folder + "BDSP-Dialog.png", false, -1, -1, false);
    database.add<Test_SystemKeyboardDetector>(folder + "HomeMenu.png", false, -1, -1, false);
    database.add<Test_SystemKeyboardDetector>(folder + "HomeMenu-OnlineLicensePopup.png", false, -1, -1, false);
    database.add<Test_SystemKeyboardDetector>(folder + "UserSelect.png", false, -1, -1, false);
}



}
}

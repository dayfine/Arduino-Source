/*  System Keyboard Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <cmath>
#include <cstring>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch_SystemKeyboardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

using namespace std::chrono_literals;


//  Key grid of the QWERTY layout and where each key sits: rows "1".."z", columns "1".."-".
static const char* const KEY_ROWS[] = {"1234567890-", "qwertyuiop/", "asdfghjkl:'", "zxcvbnm,.?!"};
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


void type_on_system_keyboard(VideoStream& stream, ProControllerContext& context, const std::string& text){
    for (char ch : text){
        const char key = (char)std::tolower((unsigned char)ch);
        int target_row = -1;
        int target_col = -1;
        for (int r = 0; r < 4 && target_row < 0; r++){
            const char* p = std::strchr(KEY_ROWS[r], key);
            if (p != nullptr && key != '\0'){
                target_row = r;
                target_col = (int)(p - KEY_ROWS[r]);
            }
        }
        if (target_row < 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("System keyboard: no key for '") + ch + "'.",
                stream
            );
        }

        //  A button pressed while the keyboard slides in is ignored, so read the cursor
        //  again after each move until it is on the target.
        bool arrived = false;
        for (size_t attempt = 0; attempt < 10 && !arrived; attempt++){
            context.wait_for_all_requests();
            int row = -1;
            int col = -1;
            VideoSnapshot screen = stream.video().snapshot();
            if (!screen || !find_system_keyboard_cursor(screen, row, col)){
                pbf_wait(context, 300ms);
                continue;
            }
            if (row == target_row && col == target_col){
                arrived = true;
                break;
            }
            for (; row < target_row; row++){
                pbf_press_dpad(context, DPAD_DOWN, 100ms, 200ms);
            }
            for (; row > target_row; row--){
                pbf_press_dpad(context, DPAD_UP, 100ms, 200ms);
            }
            for (; col < target_col; col++){
                pbf_press_dpad(context, DPAD_RIGHT, 100ms, 200ms);
            }
            for (; col > target_col; col--){
                pbf_press_dpad(context, DPAD_LEFT, 100ms, 200ms);
            }
            pbf_wait(context, 300ms);
        }
        if (!arrived){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::string("System keyboard: the cursor did not reach '") + key + "'.",
                stream
            );
        }
        if (std::isupper((unsigned char)ch)){
            pbf_press_button(context, BUTTON_LCLICK, 100ms, 250ms);
        }
        pbf_press_button(context, BUTTON_A, 80ms, 300ms);
    }
    context.wait_for_all_requests();
}



}
}

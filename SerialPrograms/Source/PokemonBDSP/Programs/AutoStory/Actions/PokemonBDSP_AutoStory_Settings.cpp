/*  BDSP AutoStory - Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <algorithm>
#include <cmath>
#include <string>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP_AutoStory_Bag.h"
#include "PokemonBDSP_AutoStory_Settings.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  How red a pixel must be to count as highlighted text or the row outline.
bool is_red(uint32_t pixel, int margin){
    const int r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
    return r > g + margin && r > b + margin;
}

//  The red pixels in `box` of the screen.
size_t red_pixels(const ImageViewRGB32& screen, const ImageFloatBox& box){
    const ImageViewRGB32 image = extract_box_reference(screen, box);
    size_t red = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            red += is_red(image.pixel(x, y), 30);
        }
    }
    return red;
}

//  The share of near-white pixels on the settings panel, sampled every 4th pixel.
double white_share(const ImageViewRGB32& screen){
    const ImageViewRGB32 panel = extract_box_reference(screen, ImageFloatBox{0.04, 0.065, 0.93, 0.70});
    size_t white = 0, samples = 0;
    for (size_t y = 0; y < panel.height(); y += 4){
        for (size_t x = 0; x < panel.width(); x += 4){
            const uint32_t p = panel.pixel(x, y);
            white += ((p >> 16) & 255) > 225 && ((p >> 8) & 255) > 225 && (p & 255) > 225;
            samples++;
        }
    }
    return samples == 0 ? 0 : (double)white / samples;
}

//  The share of blue pixels in the heading bar, which the icon menu and the Bag lack.
double heading_blue_share(const ImageViewRGB32& screen){
    const ImageViewRGB32 heading = extract_box_reference(screen, ImageFloatBox{0.01, 0.005, 0.20, 0.025});
    size_t blue = 0, total = 0;
    for (size_t y = 0; y < heading.height(); y++){
        for (size_t x = 0; x < heading.width(); x++){
            const uint32_t p = heading.pixel(x, y);
            const int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            blue += b > r + 20 && g > r + 10;
            total++;
        }
    }
    return total == 0 ? 0 : (double)blue / total;
}

//  Where each row's value columns start and end, as fractions of the screen
//  width: the same in every language (values are left-aligned in them).
struct ValueColumn{
    double left;
    double right;
};
const ValueColumn TWO_COLUMNS[] = {{0.335, 0.62}, {0.62, 0.88}};
const ValueColumn THREE_COLUMNS[] = {{0.335, 0.52}, {0.52, 0.70}, {0.70, 0.88}};

//  The first row's outline centre when Options opens, and how far it may be off.
constexpr double FIRST_ROW_Y = 0.157;
constexpr double ROW_Y_TOLERANCE = 0.02;
//  How far apart rows are on screen.
constexpr double ROW_PITCH = 0.086;

//  The rows the story sets, top to bottom, in the game's fixed order (the
//  same in every language). `columns` is the number of value columns, 0 for
//  the Change Window selector, which is left as the new game has it (Type 1);
//  `target` is the column to select, counted from the left. The two volume
//  sliders below Show Nicknames are left as they are.
struct OptionRow{
    const char* name;
    size_t columns;
    size_t target;
};
const OptionRow OPTION_ROWS[] = {
    {"Text Speed", 3, 2},       //  Slow, Normal, Fast
    {"Change Window", 0, 0},    //  < Type 1 >
    {"Battle Effects", 2, 1},   //  On, Off
    {"Battle Style", 2, 1},     //  Switch, Set
    {"Send to Boxes", 2, 1},    //  Manual, Automatic
    {"Give Nicknames", 2, 1},   //  Give, Don't give
    {"Gyroscope", 2, 0},        //  Use, Don't use
    {"Autosave", 2, 1},         //  On, Off
    {"Show Nicknames", 2, 0},   //  Show, Don't show
};
constexpr size_t OPTION_ROW_COUNT = sizeof(OPTION_ROWS) / sizeof(OPTION_ROWS[0]);
constexpr size_t AUTOSAVE_ROW = 7;

} // namespace


bool options_screen_shown(const ImageViewRGB32& screen){
    return screen && white_share(screen) > 0.70 && heading_blue_share(screen) > 0.25;
}

std::optional<double> options_selected_row_y(const ImageViewRGB32& screen){
    if (!screen){
        return std::nullopt;
    }
    //  The two long red horizontal lines of the outline around the selected row.
    int first = -1, last = -1;
    for (size_t y = (size_t)(screen.height() * 0.08); y < screen.height() * 0.75; y++){
        size_t red = 0, total = 0;
        for (size_t x = (size_t)(screen.width() * 0.08); x < screen.width() * 0.90; x += 2){
            const uint32_t p = screen.pixel(x, y);
            red += ((p >> 16) & 255) > 150 && is_red(p, 40);
            total++;
        }
        if (total && red > total * 0.70){
            if (first < 0){
                first = (int)y;
            }
            last = (int)y;
        }
    }
    if (first < 0 || last - first < screen.height() * 0.04 || last - first > screen.height() * 0.12){
        return std::nullopt;
    }
    return (first + last) / (2.0 * screen.height());
}

std::optional<size_t> options_highlighted_column(const ImageViewRGB32& screen, double y, size_t columns){
    const ValueColumn* layout = columns == 2 ? TWO_COLUMNS : columns == 3 ? THREE_COLUMNS : nullptr;
    if (!screen || layout == nullptr){
        return std::nullopt;
    }
    //  The band stays inside the outline, whose lines are red too.
    const double band = 0.022;
    std::optional<size_t> highlighted;
    for (size_t c = 0; c < columns; c++){
        const ImageFloatBox box{layout[c].left, y - band, layout[c].right - layout[c].left, 2 * band};
        //  A short word such as "Off" has a few hundred red pixels at 1080p.
        if (red_pixels(screen, box) * 10000 < screen.width() * screen.height() * 0.5){
            continue;
        }
        if (highlighted){
            return std::nullopt;
        }
        highlighted = c;
    }
    return highlighted;
}


namespace{

// Options fades in after A on its icon: poll instead of judging one frame.
bool wait_for_options_screen(VideoStream& stream, ProControllerContext& context, Milliseconds timeout){
    context.wait_for_all_requests();
    const WallClock deadline = current_time() + timeout;
    while (current_time() < deadline){
        auto frame = stream.video().snapshot();
        if (frame && options_screen_shown(frame)){
            return true;
        }
        context.wait_for(100ms);
    }
    return false;
}

//  From the overworld (or the main menu): the main menu, settled.
void open_main_menu(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    MenuDetector menu(COLOR_RED, /*skip_new_banner*/ true);
    VideoOverlaySet detector_overlays(stream.overlay());
    menu.make_overlays(detector_overlays);
    detector_overlays.add(COLOR_CYAN, {0.04, 0.065, 0.93, 0.70}, "");
    detector_overlays.add(COLOR_BLUE, {0.01, 0.005, 0.20, 0.025}, "");
    detector_overlays.add(COLOR_RED, {0.08, 0.08, 0.82, 0.67}, "");
    auto initial = stream.video().snapshot();
    if (!initial || !menu.detect(initial)){
        pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
        context.wait_for_all_requests();
    }
    MenuWatcher menu_open(COLOR_RED, /*skip_new_banner*/ true);
    if (wait_until(stream, context, 5s, {{menu_open}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Settings checkpoint: Main menu not detected.", stream);
    }
    stream.log("[Tutorial settings] Main menu detected; settling for 500ms before Options navigation.");
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
}

//  From the main menu: Options. Only two icons are available at this
//  initial checkpoint; verify the main menu before trying the other.
void open_options(VideoStream& stream, ProControllerContext& context){
    bool options_open = false;
    for (size_t attempt = 0; attempt < 2; attempt++){
        pbf_press_dpad(context, attempt == 0 ? DPAD_RIGHT : DPAD_LEFT, 80ms, 350ms);
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        if (wait_for_options_screen(stream, context, 5s)){
            options_open = true;
            break;
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
        MenuWatcher back_to_menu(COLOR_RED, /*skip_new_banner*/ true);
        if (wait_until(stream, context, 5s, {{back_to_menu}}) != 0){
            break;
        }
    }
    if (!options_open){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Settings checkpoint: Options screen not detected.", stream);
    }
    stream.log("[Tutorial settings] Options screen detected.");
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
}

//  The selected row's centre on a fresh frame of Options, retried while the
//  screen fades in; fails with a screenshot after about a second.
double read_selected_row(VideoStream& stream, ProControllerContext& context, size_t row){
    for (size_t attempt = 0; attempt < 6; attempt++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const std::optional<double> y = frame && options_screen_shown(frame) ? options_selected_row_y(frame) : std::nullopt;
        if (y){
            return *y;
        }
        pbf_wait(context, 200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Settings checkpoint: No selected row on Options for " + std::string(OPTION_ROWS[row].name), stream);
}

//  Options opens on its first row (Text Speed), at the top of the list:
//  that row's centre.
double check_first_row(VideoStream& stream, ProControllerContext& context){
    const double y = read_selected_row(stream, context, 0);
    stream.log("[Tutorial settings] First row at y=" + std::to_string(y));
    if (y < FIRST_ROW_Y - ROW_Y_TOLERANCE || y > FIRST_ROW_Y + ROW_Y_TOLERANCE){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Settings checkpoint: Options did not open on its first row.", stream);
    }
    return y;
}

//  The mean grey-level difference of two frames over `box`, sampled every 2nd pixel.
double box_difference(const ImageViewRGB32& a, const ImageViewRGB32& b, const ImageFloatBox& box){
    const ImageViewRGB32 x = extract_box_reference(a, box);
    const ImageViewRGB32 z = extract_box_reference(b, box);
    const size_t width = std::min(x.width(), z.width()), height = std::min(x.height(), z.height());
    double sum = 0;
    size_t count = 0;
    for (size_t py = 0; py < height; py += 2){
        for (size_t px = 0; px < width; px += 2){
            const uint32_t p = x.pixel(px, py), q = z.pixel(px, py);
            const int gp = ((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255);
            const int gq = ((q >> 16) & 255) + ((q >> 8) & 255) + (q & 255);
            sum += std::abs(gp - gq) / 3.0;
            count++;
        }
    }
    return count == 0 ? 0 : sum / count;
}

//  From row `row` (outline at `y`): one press down, confirmed by the outline
//  moving a row down, or, at the bottom of the view, by the list scrolling
//  under it (the label under the outline changes). Never presses twice, so
//  a row is never skipped. Returns the new row's centre.
double move_down(VideoStream& stream, ProControllerContext& context, size_t row, double y){
    context.wait_for_all_requests();
    const auto before = stream.video().snapshot();
    pbf_press_dpad(context, DPAD_DOWN, 80ms, 350ms);
    context.wait_for_all_requests();
    const ImageFloatBox label{0.08, y - 0.025, 0.24, 0.05};
    for (size_t check = 0; check < 15; check++){
        const auto after = stream.video().snapshot();
        const std::optional<double> now = after && options_screen_shown(after) ? options_selected_row_y(after) : std::nullopt;
        if (now && *now > y + ROW_PITCH / 2 && *now < y + ROW_PITCH * 1.5){
            return *now;
        }
        if (now && std::abs(*now - y) < ROW_PITCH / 4 && before && box_difference(before, after, label) > 8){
            return *now;
        }
        context.wait_for(100ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Settings checkpoint: Cursor did not move down from " + std::string(OPTION_ROWS[row].name), stream);
}

//  The highlighted column of `row` (outline at `y`) on a fresh frame, or none.
std::optional<size_t> read_column(VideoStream& stream, ProControllerContext& context, size_t row, double y){
    context.wait_for_all_requests();
    const auto frame = stream.video().snapshot();
    const std::optional<size_t> column = frame ? options_highlighted_column(frame, y, OPTION_ROWS[row].columns) : std::nullopt;
    stream.log("[Tutorial settings] Row " + std::to_string(row) + " (" + OPTION_ROWS[row].name + ") y=" +
        std::to_string(y) + " column=" + (column ? std::to_string(*column) : "none") +
        " target=" + std::to_string(OPTION_ROWS[row].target));
    return column;
}

//  Sets the selected row `row` (outline at `y`) to its target column, a
//  press at a time toward it, reading the highlight after each press.
void set_option(VideoStream& stream, ProControllerContext& context, size_t row, double y){
    const OptionRow& option = OPTION_ROWS[row];
    if (option.columns == 0){
        stream.log("[Tutorial settings] Row " + std::to_string(row) + " (" + option.name + ") left as it is.");
        return;
    }
    OverlayBoxScope row_overlay(stream.overlay(), COLOR_RED, {0.07, y - 0.04, 0.84, 0.08}, "");
    OverlayBoxScope value_overlay(stream.overlay(), COLOR_GREEN, {0.335, y - 0.022, 0.545, 0.044}, "");
    size_t unread = 0;
    for (size_t attempt = 0; attempt < 12; attempt++){
        const std::optional<size_t> column = read_column(stream, context, row, y);
        if (!column){
            //  The highlight can still be fading in after a press.
            if (++unread > 4){
                break;
            }
            pbf_wait(context, 200ms);
            continue;
        }
        if (*column == option.target){
            stream.log("[Tutorial settings] Verified " + std::string(option.name));
            return;
        }
        pbf_press_dpad(context, *column < option.target ? DPAD_RIGHT : DPAD_LEFT, 80ms, 350ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Settings checkpoint: Could not apply " + std::string(option.name), stream);
}

//  Confirms the settings and leaves Options (A A A A B B), back to the overworld.
void save_options(VideoStream& stream, ProControllerContext& context, const std::string& failure){
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    context.wait_for_all_requests();
    OverworldWatcher overworld;
    if (wait_until(stream, context, 10s, {{overworld}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, failure, stream);
    }
}

//  From the overworld: the saved Options screen again.
void reopen_options(VideoStream& stream, ProControllerContext& context){
    pbf_press_button(context, BUTTON_X, 80ms, 1000ms);
    context.wait_for_all_requests();
    MenuWatcher verify_menu(COLOR_RED, /*skip_new_banner*/ true);
    if (wait_until(stream, context, 5s, {{verify_menu}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Autosave verification: Main menu not detected.", stream);
    }
    bool reopened = false;
    for (size_t attempt = 0; attempt < 2; attempt++){
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        if (wait_for_options_screen(stream, context, 5s)){
            reopened = true;
            break;
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
        MenuWatcher return_menu(COLOR_RED, /*skip_new_banner*/ true);
        if (wait_until(stream, context, 5s, {{return_menu}}) != 0){
            break;
        }
        pbf_press_dpad(context, DPAD_RIGHT, 80ms, 350ms);
    }
    if (!reopened){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Autosave verification: Could not reopen saved Options.", stream);
    }
}

//  In the saved Options: down to Autosave, a confirmed row at a time, and
//  Off highlighted on three frames 200 ms apart.
void check_saved_autosave(VideoStream& stream, ProControllerContext& context){
    double y = check_first_row(stream, context);
    for (size_t row = 0; row < AUTOSAVE_ROW; row++){
        y = move_down(stream, context, row, y);
    }
    for (size_t confirmation = 0; confirmation < 3; confirmation++){
        if (read_column(stream, context, AUTOSAVE_ROW, y) != OPTION_ROWS[AUTOSAVE_ROW].target){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Saved Autosave setting is not verified Off. Stopping before movement.", stream);
        }
        pbf_wait(context, 200ms);
    }
    stream.log("[Tutorial settings] Autosave = Off: confirmed after reopening saved Options (3 frames).");
}

void verify_tutorial_settings_once(VideoStream& stream, ProControllerContext& context){
    open_main_menu(stream, context);
    open_options(stream, context);
    double y = check_first_row(stream, context);
    for (size_t row = 0; row < OPTION_ROW_COUNT; row++){
        set_option(stream, context, row, y);
        if (row + 1 < OPTION_ROW_COUNT){
            y = move_down(stream, context, row, y);
        }
    }
    save_options(stream, context, "Settings checkpoint: Did not return to the overworld after saving options.");
    // Reopen the saved Options screen and independently verify Autosave.
    // No route movement is permitted until this persisted-value check passes.
    reopen_options(stream, context);
    check_saved_autosave(stream, context);
    save_options(stream, context, "Autosave verification: Could not return to the overworld.");
    stream.log("[Tutorial settings] Completed: Text Speed Fast, Battle Effects Off, Battle Style Set, "
        "Send to Boxes Automatic, Give Nicknames Don't give, Gyroscope Use, Autosave Off, Show Nicknames Show.");
}

//  After a failed attempt: back out of Options, save prompts, the Bag or
//  the menu without moving the player or resetting/loading the game.
void back_to_overworld(VideoStream& stream, ProControllerContext& context){
    stream.log("[Tutorial settings] Returning to overworld before retry.");
    context.wait_for_all_requests();
    const auto failed_frame = stream.video().snapshot();
    if (failed_frame && options_screen_shown(failed_frame)){
        // Finish the known Options save/exit sequence before observing
        // state; never cancel it on an Options frame misclassified as
        // overworld by the generic detector.
        for (size_t press = 0; press < 4; press++){
            pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
        }
    }
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
    OverworldDetector overworld;
    MenuDetector menu(COLOR_RED, /*skip_new_banner*/ true);
    BlackScreenDetector black;
    bool recovered = false;
    size_t confirmations = 0;
    for (size_t check = 0; check < 15 && !recovered; check++){
        const auto frame = stream.video().snapshot();
        const bool clear = frame && !options_screen_shown(frame) &&
            !menu.detect(frame) && !black.detect(frame) && bag_pocket_position(frame) == 0 && overworld.detect(frame);
        if (clear && ++confirmations >= 3){
            recovered = true;
        }else if (clear){
            pbf_wait(context, 200ms);
        }else{
            confirmations = 0;
            pbf_mash_button(context, BUTTON_B, 500ms);
        }
        context.wait_for_all_requests();
    }
    stream.log(recovered ? "[Tutorial settings] Recovery confirmed outside Options/menu; retrying."
        : "[Tutorial settings] Recovery not confirmed; next attempt will recheck the screen.");
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
}

} // namespace

void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context){
    // Settings-only recovery: initial attempt plus three retries.
    // Never permit route movement unless the complete check succeeds.
    constexpr size_t MAX_RETRIES = 3;
    for (size_t attempt = 0; attempt <= MAX_RETRIES; attempt++){
        stream.log("[Tutorial settings] Attempt " + std::to_string(attempt + 1) + "/4.");
        try{
            verify_tutorial_settings_once(stream, context);
            return;
        }catch (OperationFailedException& error){
            stream.log("[Tutorial settings] Attempt failed: " + error.message(), COLOR_ORANGE);
            if (attempt == MAX_RETRIES){
                stream.log("[Tutorial settings] All three retries exhausted; stopping before movement.", COLOR_RED);
                throw;
            }
            back_to_overworld(stream, context);
        }
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

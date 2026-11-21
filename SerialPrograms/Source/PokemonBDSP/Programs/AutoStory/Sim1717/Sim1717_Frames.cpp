/*  BDSP AutoStory PR #1717 Synthetic Video Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <memory>
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TownBannerDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "Tests/TestUtils.h"
#include "Sim1717_Frames.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

constexpr size_t FRAME_W = 960;
constexpr size_t FRAME_H = 540;

uint32_t rgb(uint8_t r, uint8_t g, uint8_t b){
    return 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

void fill_solid(ImageRGB32& img, uint32_t color){
    for (size_t y = 0; y < img.height(); y++){
        for (size_t x = 0; x < img.width(); x++){
            img.pixel(x, y) = color;
        }
    }
}

void fill_box(ImageRGB32& img, const ImageFloatBox& box, uint32_t color){
    const size_t x0 = std::min(img.width(), (size_t)std::floor(box.x * img.width()));
    const size_t y0 = std::min(img.height(), (size_t)std::floor(box.y * img.height()));
    const size_t x1 = std::min(img.width(), (size_t)std::ceil((box.x + box.width) * img.width()));
    const size_t y1 = std::min(img.height(), (size_t)std::ceil((box.y + box.height) * img.height()));
    for (size_t y = y0; y < y1; y++){
        for (size_t x = x0; x < x1; x++){
            img.pixel(x, y) = color;
        }
    }
}

void fill_checker_box(ImageRGB32& img, const ImageFloatBox& box, uint32_t c0, uint32_t c1){
    const size_t x0 = std::min(img.width(), (size_t)std::floor(box.x * img.width()));
    const size_t y0 = std::min(img.height(), (size_t)std::floor(box.y * img.height()));
    const size_t x1 = std::min(img.width(), (size_t)std::ceil((box.x + box.width) * img.width()));
    const size_t y1 = std::min(img.height(), (size_t)std::ceil((box.y + box.height) * img.height()));
    for (size_t y = y0; y < y1; y++){
        for (size_t x = x0; x < x1; x++){
            img.pixel(x, y) = ((x + y) & 1) ? c1 : c0;
        }
    }
}

//  Draws textured mid-green/gray overworld scenery + red Poketch button.
ImageRGB32 make_overworld_base(bool cave){
    ImageRGB32 img(FRAME_W, FRAME_H);
    for (size_t y = 0; y < FRAME_H; y++){
        for (size_t x = 0; x < FRAME_W; x++){
            const int v = ((x / 4 + y / 4) & 1) ? 25 : -25;
            if (cave){
                //  Warm brown cave floor (r > g + 12, g > b + 12).
                img.pixel(x, y) = rgb(
                    (uint8_t)std::clamp(140 + v, 0, 255),
                    (uint8_t)std::clamp(100 + v, 0, 255),
                    (uint8_t)std::clamp(65 + v, 0, 255)
                );
            }else{
                img.pixel(x, y) = rgb(
                    (uint8_t)std::clamp(90 + v, 0, 255),
                    (uint8_t)std::clamp(135 + v, 0, 255),
                    (uint8_t)std::clamp(85 + v, 0, 255)
                );
            }
        }
    }
    if (cave){
        //  Black map void region checked by Segment_02 cave_visible().
        fill_box(img, {0.10, 0.80, 0.20, 0.16}, rgb(5, 5, 5));
    }
    //  Poketch red button for PoketchButtonDetector ({0.968, 0.170, 0.022, 0.120}).
    fill_box(img, {0.965, 0.165, 0.028, 0.130}, rgb(200, 28, 43));
    return img;
}

void draw_short_dialog_overlay(ImageRGB32& img){
    //  White dialog interior covering left_white, right_white, bottom_white.
    fill_box(img, {0.210, 0.830, 0.600, 0.145}, rgb(248, 248, 248));
    //  High-contrast edges on m_left (0.18..0.20) and m_right (0.822..0.842) -> stddev.sum() > 30.
    fill_checker_box(img, {0.175, 0.830, 0.030, 0.130}, rgb(40, 40, 40), rgb(240, 240, 240));
    fill_checker_box(img, {0.818, 0.830, 0.030, 0.130}, rgb(40, 40, 40), rgb(240, 240, 240));
}

void blit_resource_into(ImageRGB32& img, const std::string& rel_path, const ImageFloatBox& box, bool white_bg){
    if (white_bg){
        fill_box(img, box, rgb(252, 252, 252));
    }
    const ImageRGB32 src(RESOURCE_PATH() + rel_path);
    if (!src){
        return;
    }
    const size_t x0 = (size_t)std::lround(box.x * img.width());
    const size_t y0 = (size_t)std::lround(box.y * img.height());
    const size_t w = std::max<size_t>(1, (size_t)std::lround(box.width * img.width()));
    const size_t h = std::max<size_t>(1, (size_t)std::lround(box.height * img.height()));
    const ImageRGB32 scaled = src.scale_to(w, h);
    for (size_t y = 0; y < h && y0 + y < img.height(); y++){
        for (size_t x = 0; x < w && x0 + x < img.width(); x++){
            img.pixel(x0 + x, y0 + y) = scaled.pixel(x, y);
        }
    }
}

ImageRGB32 make_short_dialog_prompt(bool yes_selected){
    ImageRGB32 img = make_overworld_base(false);
    draw_short_dialog_overlay(img);
    //  White prompt box covering {0.60, 0.55, 0.30, 0.25}.
    fill_box(img, {0.60, 0.55, 0.30, 0.25}, rgb(250, 250, 250));
    //  SelectionArrow.png inside Yes {0.707, 0.650, 0.040, 0.065} or No {0.707, 0.713, 0.040, 0.065}.
    //  Leave a small white margin so Waterfill finds a clean connected component.
    const ImageFloatBox target = yes_selected
        ? ImageFloatBox{0.715, 0.658, 0.022, 0.048}
        : ImageFloatBox{0.715, 0.721, 0.022, 0.048};
    blit_resource_into(img, "PokemonBDSP/SelectionArrow.png", target, false);
    return img;
}

ImageRGB32 make_battle_dialog(){
    ImageRGB32 img = make_overworld_base(false);
    //  m_bottom (0.50, 0.91, 0.40, 0.05) and m_left_white (0.07, 0.835, 0.008, 0.12) white.
    fill_box(img, {0.065, 0.830, 0.880, 0.145}, rgb(248, 248, 248));
    //  High contrast on m_left (0.04, 0.835, 0.02, 0.12) and m_right (0.965, 0.835, 0.02, 0.12).
    fill_checker_box(img, {0.035, 0.830, 0.028, 0.130}, rgb(20, 20, 20), rgb(245, 245, 245));
    fill_checker_box(img, {0.960, 0.830, 0.028, 0.130}, rgb(20, 20, 20), rgb(245, 245, 245));
    return img;
}

//  Draws a color button with average ratio close to target and stddev.sum() ~ 60.
void fill_battle_button(ImageRGB32& img, const ImageFloatBox& box, double r_rat, double g_rat, double b_rat){
    const double sum = 360.0;
    const int br = (int)std::lround(r_rat * sum);
    const int bg = (int)std::lround(g_rat * sum);
    const int bb = (int)std::lround(b_rat * sum);
    const uint32_t c0 = rgb(
        (uint8_t)std::clamp(br - 20, 0, 255),
        (uint8_t)std::clamp(bg - 20, 0, 255),
        (uint8_t)std::clamp(bb - 20, 0, 255)
    );
    const uint32_t c1 = rgb(
        (uint8_t)std::clamp(br + 20, 0, 255),
        (uint8_t)std::clamp(bg + 20, 0, 255),
        (uint8_t)std::clamp(bb + 20, 0, 255)
    );
    fill_checker_box(img, box, c0, c1);
}

ImageRGB32 make_battle_menu(int selected_row){
    ImageRGB32 img(FRAME_W, FRAME_H);
    fill_solid(img, rgb(60, 80, 100));
    //  Status & opponent panels (white).
    fill_box(img, {0.135, 0.905, 0.115, 0.035}, rgb(250, 250, 250));
    fill_box(img, {0.400, 0.920, 0.115, 0.025}, rgb(250, 250, 250));
    fill_box(img, {0.703, 0.065, 0.015, 0.038}, rgb(250, 250, 250));
    fill_box(img, {0.977, 0.065, 0.015, 0.038}, rgb(250, 250, 250));
    //  Poke Ball shortcut white boxes (for BattleType::STANDARD).
    fill_box(img, {0.885, 0.470, 0.030, 0.040}, rgb(250, 250, 250));
    fill_box(img, {0.955, 0.470, 0.040, 0.040}, rgb(250, 250, 250));
    //  Four action buttons matching BattleMenuDetector exact ratios.
    fill_battle_button(img, {0.817, 0.585 + 0 * 0.1075, 0.150, 0.070}, 0.541961, 0.231317, 0.226722);
    fill_battle_button(img, {0.817, 0.585 + 1 * 0.1075, 0.150, 0.070}, 0.255944, 0.582771, 0.161285);
    fill_battle_button(img, {0.817, 0.585 + 2 * 0.1075, 0.150, 0.070}, 0.485857, 0.414946, 0.099197);
    fill_battle_button(img, {0.817, 0.585 + 3 * 0.1075, 0.150, 0.070}, 0.250977, 0.269868, 0.479155);

    //  White corner brackets for selected_row (0..3).
    if (selected_row >= 0 && selected_row < 4){
        const double offset = selected_row * 0.1075;
        fill_box(img, {0.793, 0.562 + offset, 0.018, 0.023}, rgb(252, 252, 252));
        fill_box(img, {0.976, 0.562 + offset, 0.018, 0.023}, rgb(252, 252, 252));
        fill_box(img, {0.793, 0.652 + offset, 0.018, 0.023}, rgb(252, 252, 252));
        fill_box(img, {0.976, 0.652 + offset, 0.018, 0.023}, rgb(252, 252, 252));
    }
    return img;
}

ImageRGB32 make_main_menu(){
    ImageRGB32 img(FRAME_W, FRAME_H);
    fill_solid(img, rgb(120, 120, 120));
    //  White panel borders (top, left, right).
    fill_box(img, {0.145, 0.095, 0.715, 0.025}, rgb(250, 250, 250));
    fill_box(img, {0.145, 0.120, 0.030, 0.470}, rgb(250, 250, 250));
    fill_box(img, {0.830, 0.120, 0.025, 0.470}, rgb(250, 250, 250));
    //  Blue-cyan footer: b >= r + 35, g >= r + 20, stddev <= 60.
    fill_box(img, {0.045, 0.955, 0.615, 0.040}, rgb(30, 110, 180));
    //  Dark/light neutral contrast buttons (confirm, save, close, save_shortcut).
    for (const ImageFloatBox& box : {
        ImageFloatBox{0.736, 0.960, 0.022, 0.038},
        ImageFloatBox{0.835, 0.960, 0.024, 0.038},
        ImageFloatBox{0.915, 0.960, 0.024, 0.038},
        ImageFloatBox{0.936, 0.826, 0.029, 0.053},
    }){
        fill_checker_box(img, box, rgb(40, 40, 40), rgb(240, 240, 240));
    }
    //  Save heading: rgb > 180 and stddev.sum() > 30.
    fill_checker_box(img, {0.918, 0.752, 0.075, 0.046}, rgb(190, 190, 190), rgb(245, 245, 245));
    //  Save background: g,b >= r + 40 and stddev.sum() < 90.
    fill_box(img, {0.970, 0.815, 0.026, 0.075}, rgb(40, 150, 180));
    return img;
}

ImageRGB32 make_town_banner(){
    ImageRGB32 img = make_overworld_base(false);
    //  TownBannerDetector: white padding boxes + dark bottom border at {0.032, 0.113, 0.390, 0.003}.
    fill_box(img, {0.022, 0.020, 0.420, 0.090}, rgb(245, 245, 245));
    fill_box(img, {0.022, 0.110, 0.420, 0.010}, rgb(35, 35, 35));
    return img;
}

ImageRGB32 make_briefcase(){
    ImageRGB32 img = make_overworld_base(false);
    //  StarterBriefcase.png is a full-screen reference; ImageMatchDetector crops {0.5, 0.1, 0.5, 0.7} from it.
    blit_resource_into(img, "PokemonBDSP/StarterBriefcase.png", {0.0, 0.0, 1.0, 1.0}, false);
    return img;
}

ImageRGB32 build_frame(SimScreenKind kind){
    switch (kind){
    case SimScreenKind::OVERWORLD:
        return make_overworld_base(false);
    case SimScreenKind::OVERWORLD_CAVE:
        return make_overworld_base(true);
    case SimScreenKind::OVERWORLD_WITH_BANNER:
        return make_town_banner();
    case SimScreenKind::BLACK_SCREEN: {
        ImageRGB32 img(FRAME_W, FRAME_H);
        fill_solid(img, rgb(0, 0, 0));
        return img;
    }
    case SimScreenKind::WHITE_SCREEN: {
        ImageRGB32 img(FRAME_W, FRAME_H);
        fill_solid(img, rgb(255, 255, 255));
        return img;
    }
    case SimScreenKind::SHORT_DIALOG: {
        ImageRGB32 img = make_overworld_base(false);
        draw_short_dialog_overlay(img);
        return img;
    }
    case SimScreenKind::SHORT_DIALOG_PROMPT_YES:
        return make_short_dialog_prompt(true);
    case SimScreenKind::SHORT_DIALOG_PROMPT_NO:
        return make_short_dialog_prompt(false);
    case SimScreenKind::BRIEFCASE:
        return make_briefcase();
    case SimScreenKind::BATTLE_DIALOG:
        return make_battle_dialog();
    case SimScreenKind::BATTLE_MENU_ROW0:
        return make_battle_menu(0);
    case SimScreenKind::BATTLE_MENU_ROW1:
        return make_battle_menu(1);
    case SimScreenKind::BATTLE_MENU_ROW2:
        return make_battle_menu(2);
    case SimScreenKind::BATTLE_MENU_ROW3:
        return make_battle_menu(3);
    case SimScreenKind::MAIN_MENU:
        return make_main_menu();
    }
    return make_overworld_base(false);
}

} // namespace

std::shared_ptr<const ImageRGB32> sim1717_frame(SimScreenKind kind){
    static std::array<std::shared_ptr<const ImageRGB32>, 16> cache;
    const size_t idx = static_cast<size_t>(kind);
    if (!cache[idx]){
        cache[idx] = std::make_shared<const ImageRGB32>(build_frame(kind));
    }
    return cache[idx];
}

std::string sim1717_verify_synthetic_frames(){
    BlackScreenDetector black;
    WhiteScreenDetector white;
    ShortDialogDetector sdialog;
    BattleDialogDetector bdialog;
    BattleMenuDetector bmenu_std(BattleType::STANDARD);
    BattleMenuDetector bmenu_trn(BattleType::TRAINER);
    BattleMenuDetector bmenu_str(BattleType::STARTER);
    MenuDetector menu;
    OverworldDetector overworld;
    TownBannerDetector banner;
    PoketchButtonDetector poketch;
    DummyVideoOverlay overlay;
    SelectionArrowFinder arrow_yes(overlay, {0.707, 0.650, 0.040, 0.065}, COLOR_CYAN);
    SelectionArrowFinder arrow_no(overlay, {0.707, 0.713, 0.040, 0.065}, COLOR_CYAN);
    SelectionArrowFinder arrow_any(overlay, {0.60, 0.55, 0.30, 0.25}, COLOR_CYAN);

    auto check = [&](bool cond, const std::string& msg) -> std::string{
        return cond ? "" : msg;
    };

    const auto& f_ow = *sim1717_frame(SimScreenKind::OVERWORLD);
    if (auto e = check(overworld.detect(f_ow) && !black.detect(f_ow) && !white.detect(f_ow) &&
        !sdialog.detect(f_ow) && !bmenu_std.detect(f_ow) && !menu.detect(f_ow) &&
        !banner.detect(f_ow) && poketch.detect(f_ow), "OVERWORLD frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_blk = *sim1717_frame(SimScreenKind::BLACK_SCREEN);
    if (auto e = check(black.detect(f_blk) && !white.detect(f_blk) && !sdialog.detect(f_blk),
        "BLACK_SCREEN frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_wht = *sim1717_frame(SimScreenKind::WHITE_SCREEN);
    if (auto e = check(white.detect(f_wht) && !black.detect(f_wht) && !banner.detect(f_wht),
        "WHITE_SCREEN frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_sd = *sim1717_frame(SimScreenKind::SHORT_DIALOG);
    if (auto e = check(sdialog.detect(f_sd) && !overworld.detect(f_sd) && !arrow_any.detect(f_sd),
        "SHORT_DIALOG frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_yes = *sim1717_frame(SimScreenKind::SHORT_DIALOG_PROMPT_YES);
    if (auto e = check(sdialog.detect(f_yes) && arrow_yes.detect(f_yes) && !arrow_no.detect(f_yes) && arrow_any.detect(f_yes),
        "SHORT_DIALOG_PROMPT_YES frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_no = *sim1717_frame(SimScreenKind::SHORT_DIALOG_PROMPT_NO);
    if (auto e = check(sdialog.detect(f_no) && arrow_no.detect(f_no) && !arrow_yes.detect(f_no) && arrow_any.detect(f_no),
        "SHORT_DIALOG_PROMPT_NO frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_ban = *sim1717_frame(SimScreenKind::OVERWORLD_WITH_BANNER);
    if (auto e = check(banner.detect(f_ban) && overworld.detect(f_ban) && !white.detect(f_ban),
        "OVERWORLD_WITH_BANNER frame failed detector check"); !e.empty()){
        return e;
    }

    const auto& f_bd = *sim1717_frame(SimScreenKind::BATTLE_DIALOG);
    if (auto e = check(bdialog.detect(f_bd), "BATTLE_DIALOG frame failed detector check"); !e.empty()){
        return e;
    }

    for (int row = 0; row < 4; row++){
        const auto kind = static_cast<SimScreenKind>(static_cast<int>(SimScreenKind::BATTLE_MENU_ROW0) + row);
        const auto& f_bm = *sim1717_frame(kind);
        if (auto e = check(
            bmenu_std.detect(f_bm) && bmenu_trn.detect(f_bm) && bmenu_str.detect(f_bm) &&
            !overworld.detect(f_bm) && bmenu_std.selected_option(f_bm) == static_cast<BattleMenuOption>(row),
            std::format("BATTLE_MENU_ROW{} failed detector/cursor check (got {})",
                row, (int)bmenu_std.selected_option(f_bm))); !e.empty()){
            return e;
        }
    }

    const auto& f_mm = *sim1717_frame(SimScreenKind::MAIN_MENU);
    if (auto e = check(menu.detect(f_mm) && !overworld.detect(f_mm),
        "MAIN_MENU frame failed detector check"); !e.empty()){
        return e;
    }

    return "";
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

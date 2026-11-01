/*  BDSP AutoStory - Battle Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <algorithm>
#include <cmath>
#include <vector>
#include <opencv2/imgproc.hpp>
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonBDSP_AutoStory_BattleMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Whether at least 15% of the box's pixels are white (min channel over
//  `low`, spread under `spread`): a cursor bracket's corner.
bool white_corner(const ImageViewRGB32& screen, const ImageFloatBox& box, unsigned low_min, unsigned spread_max){
    const ImageViewRGB32 patch = extract_box_reference(screen, box);
    size_t white = 0;
    const size_t total = patch.width() * patch.height();
    for (size_t y = 0; y < patch.height(); y++){
        for (size_t x = 0; x < patch.width(); x++){
            const uint32_t pixel = patch.pixel(x, y);
            const unsigned r = (pixel >> 16) & 255;
            const unsigned g = (pixel >> 8) & 255;
            const unsigned b = pixel & 255;
            const unsigned low = std::min(r, std::min(g, b));
            const unsigned high = std::max(r, std::max(g, b));
            white += low > low_min && high - low < spread_max;
        }
    }
    return total != 0 && static_cast<double>(white) / total >= 0.15;
}

//  Whether the command menu's row `row` has the cursor's corner brackets.
//  They wiggle; all four must agree on one small offset.
bool row_has_brackets(const ImageViewRGB32& screen, int row){
    const double offset = row * 0.1075;
    const double offsets[] = {0.0, -0.003, 0.003, -0.006, 0.006};
    for (double dy : offsets){
        for (double dx : offsets){
            const ImageFloatBox corners[] = {
                {0.793 + dx, 0.562 + offset + dy, 0.018, 0.023},
                {0.976 + dx, 0.562 + offset + dy, 0.018, 0.023},
                {0.793 + dx, 0.652 + offset + dy, 0.018, 0.023},
                {0.976 + dx, 0.652 + offset + dy, 0.018, 0.023},
            };
            if (std::all_of(std::begin(corners), std::end(corners), [&](const ImageFloatBox& box){
                return white_corner(screen, box, 210, 35);
            })){
                return true;
            }
        }
    }
    return false;
}

//  The white PP panels in `patch` (the search box), top to bottom: nothing
//  for no panel, more than four, or geometry that is not a move list.
std::vector<cv::Rect> find_pp_panels(const ImageViewRGB32& screen, const ImageViewRGB32& patch){
    cv::Mat white((int)patch.height(), (int)patch.width(), CV_8UC1);
    for (size_t y = 0; y < patch.height(); y++){
        for (size_t x = 0; x < patch.width(); x++){
            uint32_t p = patch.pixel(x, y);
            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            white.at<uint8_t>((int)y, (int)x) = std::min(r, std::min(g, b)) > 225 &&
                std::max(r, std::max(g, b)) - std::min(r, std::min(g, b)) < 30 ? 255 : 0;
        }
    }
    cv::Mat labels, stats, centers;
    int components = cv::connectedComponentsWithStats(white, labels, stats, centers);
    std::vector<cv::Rect> panels;
    for (int component = 1; component < components; component++){
        cv::Rect box(stats.at<int>(component, cv::CC_STAT_LEFT), stats.at<int>(component, cv::CC_STAT_TOP),
            stats.at<int>(component, cv::CC_STAT_WIDTH), stats.at<int>(component, cv::CC_STAT_HEIGHT));
        const double area = (double)stats.at<int>(component, cv::CC_STAT_AREA);
        if (box.width < patch.width() * 0.60 || box.height < screen.height() * 0.055 ||
            box.height > screen.height() * 0.13 || area / box.area() < 0.55){
            continue;
        }
        panels.push_back(box);
    }
    if (panels.empty() || panels.size() > 4){
        return {};
    }
    std::sort(panels.begin(), panels.end(), [](const cv::Rect& a, const cv::Rect& b){ return a.y < b.y; });
    // Reject inconsistent/overlapping geometry rather than count noise as moves.
    for (size_t i = 1; i < panels.size(); i++){
        if (panels[i].y <= panels[i - 1].y + panels[i - 1].height ||
            std::abs(panels[i].height - panels[0].height) > screen.height() * 0.025){
            return {};
        }
    }
    return panels;
}

//  Whether the move button from `top` to `bottom` (of the screen) has the
//  cursor's four corners, at a small offset.
bool move_button_selected(const ImageViewRGB32& screen, double top, double bottom){
    auto is_corner = [&](double x, double y){
        if (x < 0 || y < 0 || x + 0.018 > 1 || y + 0.023 > 1){
            return false;
        }
        return white_corner(screen, ImageFloatBox{x, y, 0.018, 0.023}, 210, 35);
    };
    for (double dy : {0.0, -0.003, 0.003, -0.006, 0.006, -0.010, 0.010}){
        for (double dx : {0.0, -0.003, 0.003}){
            if (is_corner(0.663 + dx, top - 0.010 + dy) &&
                is_corner(0.976 + dx, top - 0.010 + dy) &&
                is_corner(0.663 + dx, bottom - 0.014 + dy) &&
                is_corner(0.976 + dx, bottom - 0.014 + dy)){
                return true;
            }
        }
    }
    return false;
}

}


BattleMenuOption battle_menu_selected_option(BattleMenuDetector& menu, const ImageViewRGB32& screen){
    if (!screen || !menu.detect(screen)){
        return BattleMenuOption::UNKNOWN;
    }
    int selected = -1;
    for (int row = 0; row < 4; row++){
        if (!row_has_brackets(screen, row)){
            continue;
        }
        if (selected >= 0){
            return BattleMenuOption::UNKNOWN;
        }
        selected = row;
    }
    return static_cast<BattleMenuOption>(selected);
}

BattleMoveMenuState detect_battle_move_menu(const ImageViewRGB32& screen){
    BattleMoveMenuState result;
    if (!screen){
        return result;
    }
    // Every populated move has a white PP panel, irrespective of move type or
    // selected row. Find connected panels rather than assume a 4-row layout.
    const ImageFloatBox search{0.90, 0.48, 0.075, 0.505};
    const std::vector<cv::Rect> panels = find_pp_panels(screen, extract_box_reference(screen, search));
    if (panels.empty()){
        return result;
    }
    result.move_count = panels.size();
    for (size_t slot = 0; slot < panels.size(); slot++){
        double top = search.y + (double)panels[slot].y / screen.height();
        double bottom = top + (double)panels[slot].height / screen.height();
        result.buttons[slot] = ImageFloatBox{0.665, std::max(0.0, top - 0.01), 0.333,
            std::min(1.0, bottom + 0.01) - std::max(0.0, top - 0.01)};
        if (!move_button_selected(screen, top, bottom)){
            continue;
        }
        if (result.selected_slot){
            result.selected_slot = 0;
            return result;
        }
        result.selected_slot = slot + 1;
    }
    return result;
}




}
}
}

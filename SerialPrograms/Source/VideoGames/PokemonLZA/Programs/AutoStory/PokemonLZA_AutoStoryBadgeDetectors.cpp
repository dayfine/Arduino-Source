/*  PokemonLZA AutoStory - Badge detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdint.h>
#include <utility>
#include <vector>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryBadgeDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The share of `box`'s pixels that `matches`.
template <typename Matches>
double share_of(const ImageViewRGB32& screen, const ImageFloatBox& box, Matches&& matches){
    const ImageViewRGB32 area = extract_box_reference(screen, box);
    const size_t total = area.width() * area.height();
    if (total == 0){
        return 0;
    }
    size_t count = 0;
    for (size_t y = 0; y < area.height(); y++){
        for (size_t x = 0; x < area.width(); x++){
            if (matches(Color(area.pixel(x, y)))){
                count++;
            }
        }
    }
    return (double)count / (double)total;
}

//  The share of `box`'s pixels that glow cyan: blue over red by 50, green
//  over red by 30, blue over 120.
double cyan_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > pixel.red() + 50 && pixel.green() > pixel.red() + 30 && pixel.blue() > 120;
    });
}

//  The share of `box`'s pixels whose every channel is above 200.
double white_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200;
    });
}

//  The share of `box`'s pixels in the controls hint card's sky blue (about
//  125, 210, 250): blue over 200, green over 160, blue over red by 60.
double sky_blue_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.blue() > 200 && pixel.green() > 160 && pixel.blue() > pixel.red() + 60;
    });
}

//  The share of `box`'s pixels in the mission banner's yellow (about 250,
//  210, 40 over a scene): red over 200, green over 140, blue under 130,
//  red over blue by 110.
double banner_yellow_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.red() > 200 && pixel.green() > 140 && pixel.blue() < 130 &&
            pixel.red() > pixel.blue() + 110;
    });
}

//  The share of `box`'s pixels in the mission complete card's green (about
//  165, 240, 125): green over 180, over red by 40 and over blue by 70.
double complete_green_share(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return share_of(screen, box, [](Color pixel){
        return pixel.green() > 180 && pixel.green() > pixel.red() + 40 && pixel.green() > pixel.blue() + 70;
    });
}

ImageFloatBox shifted(ImageFloatBox box, double dx){
    box.x += dx / 1920;
    return box;
}


//  The icon, and the strips around it (left, above, below) clear of white
//  and cyan, `dx` pixels (of 1920) right of where the Chinese badge has
//  them.
bool autosave_icon_at(
    const ImageViewRGB32& screen, const ImageFloatBox& icon,
    std::initializer_list<ImageFloatBox> around, double dx
){
    const ImageFloatBox icon_here = shifted(icon, dx);
    const double white = white_share(screen, icon_here);
    if (white < 0.08 || white > 0.40 || cyan_share(screen, icon_here) < 0.25){
        return false;
    }
    for (const ImageFloatBox& box : around){
        const ImageFloatBox here = shifted(box, dx);
        if (white_share(screen, here) >= 0.05 || cyan_share(screen, here) >= 0.05){
            return false;
        }
    }
    return true;
}

} // namespace


//  The badge on 1920x1080 (frame 0050 of the opening): the white (+) icon
//  at x 1564-1586, y 128-150; the text x 1630-1875, y 122-158; the strip
//  under the text x 1640-1880, y 161-169.
SkipPromptDetector::SkipPromptDetector(Color color)
    : m_color(color)
    , m_icon(1564.0 / 1920, 128.0 / 1080, 22.0 / 1920, 22.0 / 1080)
    , m_text(1630.0 / 1920, 122.0 / 1080, 245.0 / 1920, 36.0 / 1080)
    , m_strip(1640.0 / 1920, 161.0 / 1080, 240.0 / 1920, 8.0 / 1080)
{}

void SkipPromptDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_text);
    items.add(m_color, m_strip);
}

//  On frame 0050 the icon is 70% white, the text 24%, and the strip is
//  dark and even (average 44, deviation 2). No other opening frame has the
//  icon over 40% white with a dark strip: the brightest (0044, a white
//  flash) has the icon at 38% and the strip at 220.
bool SkipPromptDetector::detect(const ImageViewRGB32& screen){
    if (white_share(screen, m_icon) < 0.40){
        return false;
    }
    const double text = white_share(screen, m_text);
    if (text < 0.10 || text > 0.45){
        return false;
    }
    const ImageStats strip = image_stats(extract_box_reference(screen, m_strip));
    return strip.average.sum() < 3 * 100.0 && strip.stddev.sum() < 3 * 15.0;
}



AutosaveDetector::AutosaveDetector(Color color)
    : m_color(color)
    , m_icon(1603.0 / 1920, 73.0 / 1080, 27.0 / 1920, 31.0 / 1080)
    , m_left(1570.0 / 1920, 73.0 / 1080, 28.0 / 1920, 31.0 / 1080)
    , m_above(1603.0 / 1920, 56.0 / 1080, 27.0 / 1920, 14.0 / 1080)
    , m_below(1603.0 / 1920, 108.0 / 1080, 27.0 / 1920, 14.0 / 1080)
{}

void AutosaveDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_icon);
    items.add(m_color, m_left);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
}

//  Measured on the user's run (console B, 2026-11-10, frames 1.5 s apart):
//  the badge's one frame (rec1_u132) has the icon 19% white and 48% cyan,
//  nothing left of it. The only other frames over the icon bounds (u103,
//  u104: a bright cutscene flash) are 70%+ white and 39%+ cyan left of it
//  too; none of the opening's 56 lab frames come near.
//  The badge is seen in Chinese only; with a shorter text (English
//  "Saving...") a right-aligned badge moves the icon right, so the icon is
//  looked for from where it is in Chinese to 240 px right of it.
//  The 01.01 run (console B, 2026-11-10 13:00) took two flyover frames for
//  the badge: 0033, a glowing blade at dx 40, and 0038, the sky at dx
//  152-172. Both glow on past the icon: the strips 14 px above and below it
//  are 22%+ white or cyan there, 0% on u132; they must stay under 5%.
bool AutosaveDetector::detect(const ImageViewRGB32& screen){
    for (double dx = 0; dx <= 240; dx += 4){
        if (autosave_icon_at(screen, m_icon, {m_left, m_above, m_below}, dx)){
            return true;
        }
    }
    return false;
}



//  Measured on 1920x1080 (console B, 2026-11-10): the card at x 946-971,
//  y 868-901 with its spine at x 974-976; the "?" at x 953-965, y 876-894.
ControlsHintDetector::ControlsHintDetector(Color color)
    : m_color(color)
    , m_card(947.0 / 1920, 872.0 / 1080, 24.0 / 1920, 28.0 / 1080)
    , m_glyph(955.0 / 1920, 879.0 / 1080, 8.0 / 1920, 7.0 / 1080)
    , m_around{
        {928.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {980.0 / 1920, 872.0 / 1080, 14.0 / 1920, 28.0 / 1080},
        {945.0 / 1920, 856.0 / 1080, 30.0 / 1920, 8.0 / 1080},
        {945.0 / 1920, 906.0 / 1080, 30.0 / 1920, 8.0 / 1080},
    }
{}

void ControlsHintDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_card);
    for (const ImageFloatBox& box : m_around){
        items.add(m_color, box);
    }
}

//  The hint's 13 frames (01.01 run 0001-0006, the user's u132-u138): the
//  card 88%+ sky blue, the "?" at most 48%, nothing around it. Of the 376
//  frames without it, the closest are the 01.01 run's 0056 (the sky
//  behind the girl: card 76%, "?" 100%, around 54%), 0060 (card 35%,
//  around 27%) and the user's u126 (card 22%, around 32%).
bool ControlsHintDetector::detect(const ImageViewRGB32& screen){
    if (sky_blue_share(screen, m_card) < 0.80 || sky_blue_share(screen, m_glyph) > 0.70){
        return false;
    }
    for (const ImageFloatBox& box : m_around){
        if (sky_blue_share(screen, box) > 0.10){
            return false;
        }
    }
    return true;
}


namespace{

//  A 4-connected blob of a mask: its bounds and its pixel count.
struct Blob{
    size_t min_x;
    size_t min_y;
    size_t max_x;
    size_t max_y;
    size_t count = 0;
};

//  An area's mask split into blobs: each pixel's blob label (0 outside the
//  mask; blob i has label i + 1).
struct BlobImage{
    size_t width = 0;
    size_t height = 0;
    std::vector<uint32_t> labels;
    std::vector<Blob> blobs;

    uint32_t label(size_t x, size_t y) const{
        return labels[y * width + x];
    }
};

//  A mask pixel no blob has taken yet.
constexpr uint32_t UNLABELLED = UINT32_MAX;

//  Labels the blob of mask pixels 4-connected to (x, y).
void flood_blob(size_t x, size_t y, BlobImage& image){
    const uint32_t label = (uint32_t)image.blobs.size() + 1;
    Blob blob{x, y, x, y, 0};
    std::vector<std::pair<size_t, size_t>> stack{{x, y}};
    image.labels[y * image.width + x] = label;
    const auto take = [&](size_t nx, size_t ny){
        uint32_t& here = image.labels[ny * image.width + nx];
        if (here == UNLABELLED){
            here = label;
            stack.emplace_back(nx, ny);
        }
    };
    while (!stack.empty()){
        const auto [cx, cy] = stack.back();
        stack.pop_back();
        blob.count++;
        blob.min_x = std::min(blob.min_x, cx);
        blob.max_x = std::max(blob.max_x, cx);
        blob.min_y = std::min(blob.min_y, cy);
        blob.max_y = std::max(blob.max_y, cy);
        if (cx > 0){
            take(cx - 1, cy);
        }
        if (cx + 1 < image.width){
            take(cx + 1, cy);
        }
        if (cy > 0){
            take(cx, cy - 1);
        }
        if (cy + 1 < image.height){
            take(cx, cy + 1);
        }
    }
    image.blobs.push_back(blob);
}

//  The pixels of `area` that `in`, split into blobs.
template <typename In>
BlobImage find_blobs(const ImageViewRGB32& area, In&& in){
    BlobImage image;
    image.width = area.width();
    image.height = area.height();
    image.labels.assign(image.width * image.height, 0);
    for (size_t y = 0; y < image.height; y++){
        for (size_t x = 0; x < image.width; x++){
            if (in(Color(area.pixel(x, y)))){
                image.labels[y * image.width + x] = UNLABELLED;
            }
        }
    }
    for (size_t y = 0; y < image.height; y++){
        for (size_t x = 0; x < image.width; x++){
            if (image.label(x, y) == UNLABELLED){
                flood_blob(x, y, image);
            }
        }
    }
    return image;
}

//  How well blob `label` fills a V across its bounds: its overlap with the
//  V over their union. Each arm runs from 10% in at the top to the middle
//  at the bottom, 22% of the width across.
double v_match(const BlobImage& image, uint32_t label){
    const Blob& blob = image.blobs[label - 1];
    const double width = double(blob.max_x - blob.min_x + 1);
    const double height = double(blob.max_y - blob.min_y + 1);
    size_t both = 0;
    size_t either = 0;
    for (size_t y = blob.min_y; y <= blob.max_y; y++){
        const double v = (double(y - blob.min_y) + 0.5) / height;
        for (size_t x = blob.min_x; x <= blob.max_x; x++){
            const double u = (double(x - blob.min_x) + 0.5) / width;
            const bool in_v = std::abs(std::abs(u - 0.5) - (0.40 * (1 - v) + 0.03)) <= 0.11;
            const bool in_blob = image.label(x, y) == label;
            if (in_v && in_blob){
                both++;
            }
            if (in_v || in_blob){
                either++;
            }
        }
    }
    return either == 0 ? 0 : double(both) / double(either);
}

//  The mask pixels of other blobs within `margin` of blob `label`'s
//  bounds, per pixel of the blob.
double stray_share(const BlobImage& image, uint32_t label, size_t margin){
    const Blob& blob = image.blobs[label - 1];
    const size_t min_x = blob.min_x > margin ? blob.min_x - margin : 0;
    const size_t min_y = blob.min_y > margin ? blob.min_y - margin : 0;
    const size_t max_x = std::min(blob.max_x + margin, image.width - 1);
    const size_t max_y = std::min(blob.max_y + margin, image.height - 1);
    size_t stray = 0;
    for (size_t y = min_y; y <= max_y; y++){
        for (size_t x = min_x; x <= max_x; x++){
            const uint32_t here = image.label(x, y);
            if (here != 0 && here != label){
                stray++;
            }
        }
    }
    return double(stray) / double(blob.count);
}

//  Whether blob `label` is the chevron, on a screen `scale` times 1080p:
//  32x19 measured, 30-33 x 18-20 on every frame with it.
bool chevron_blob(const BlobImage& image, uint32_t label, double scale){
    const Blob& blob = image.blobs[label - 1];
    const double width = double(blob.max_x - blob.min_x + 1) / scale;
    const double height = double(blob.max_y - blob.min_y + 1) / scale;
    if (width < 26 || width > 40 || height < 15 || height > 24){
        return false;
    }
    const double aspect = width / height;
    if (aspect < 1.4 || aspect > 2.1){
        return false;
    }
    return stray_share(image, label, size_t(4 * scale + 0.5)) <= 0.05 && v_match(image, label) >= 0.45;
}

//  Whether a blob of the pixels of `area` that `in` is the chevron.
template <typename In>
bool chevron_in(const ImageViewRGB32& area, double scale, In&& in){
    const BlobImage image = find_blobs(area, in);
    for (uint32_t label = 1; label <= image.blobs.size(); label++){
        if (chevron_blob(image, label, scale)){
            return true;
        }
    }
    return false;
}

} // namespace


AdvanceArrowDetector::AdvanceArrowDetector(Color color)
    : m_color(color)
    , m_area(1370.0 / 1920, 930.0 / 1080, 110.0 / 1920, 120.0 / 1080)
{}

void AdvanceArrowDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_area);
}

//  Measured on 1920x1080 (console B, 2026-11-10; 01.01's runs 1 and 2, the
//  lab's and the user's opening frames, 459 frames): the chevron's 35
//  frames (the ad's subtitles, white boxes, the opening's blue box) fill
//  the V 0.53-0.70 with at most 1% stray pixels. Of the rest, the closest
//  in size and shape is run 1's 0033 (a dark flyover blade, V 0.49 but
//  16% stray); the closest with nothing around it fill the V 0.43 or less
//  and are out of the chevron's size.
bool AdvanceArrowDetector::detect(const ImageViewRGB32& screen){
    const double scale = double(screen.height()) / 1080;
    const ImageViewRGB32 area = extract_box_reference(screen, m_area);
    const bool white = chevron_in(area, scale, [](Color pixel){
        return pixel.red() >= 235 && pixel.green() >= 235 && pixel.blue() >= 235;
    });
    return white || chevron_in(area, scale, [](Color pixel){
        return pixel.red() <= 110 && pixel.green() <= 110 && pixel.blue() <= 110;
    });
}



//  Measured on 1920x1080 (console B, 2026-11-10, 01.01 run 3's 0181): the
//  bar's yellow fills x 430-820 and x 1100-1500 at y 425-585 (left and
//  right of the name), none of it 15 px above or below the bar; the icon's
//  hexagon is white at x 700-710 and 724-734, y 337-358, with the "!"'s
//  orange stroke at x 716-719 between them.
MainMissionBannerDetector::MainMissionBannerDetector(Color color)
    : m_color(color)
    , m_bar_left(430.0 / 1920, 425.0 / 1080, 390.0 / 1920, 160.0 / 1080)
    , m_bar_right(1100.0 / 1920, 425.0 / 1080, 400.0 / 1920, 160.0 / 1080)
    , m_above(430.0 / 1920, 375.0 / 1080, 1070.0 / 1920, 25.0 / 1080)
    , m_below(430.0 / 1920, 615.0 / 1080, 1070.0 / 1920, 25.0 / 1080)
    , m_icon_left(700.0 / 1920, 337.0 / 1080, 10.0 / 1920, 21.0 / 1080)
    , m_icon_right(724.0 / 1920, 337.0 / 1080, 10.0 / 1920, 21.0 / 1080)
    , m_icon_stroke(716.0 / 1920, 337.0 / 1080, 3.0 / 1920, 11.0 / 1080)
{}

void MainMissionBannerDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_bar_left);
    items.add(m_color, m_bar_right);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
    items.add(m_color, m_icon_left);
    items.add(m_color, m_icon_right);
    items.add(m_color, m_icon_stroke);
}

//  Measured on 01.01's runs 1-3, the lab's and the user's opening frames
//  and the other PLZA lab captures (746 frames, console B, 2026-11-10):
//  the banner's one frame (run 3's 0181) has both bar boxes 100% yellow,
//  the strips around it 0-1%, and the icon's three boxes 100% at dx 0. The
//  closest others: the bar boxes 8% at most, the icon's weakest box 27% at
//  most (run 2's 0032, the flyover). The icon is looked for 60 px either side, as a longer
//  or shorter header moves it.
bool MainMissionBannerDetector::detect(const ImageViewRGB32& screen){
    if (banner_yellow_share(screen, m_bar_left) < 0.6 || banner_yellow_share(screen, m_bar_right) < 0.6){
        return false;
    }
    if (banner_yellow_share(screen, m_above) > 0.3 || banner_yellow_share(screen, m_below) > 0.3){
        return false;
    }
    const auto bright = [](Color pixel){
        return pixel.red() > 225 && pixel.green() > 225 && pixel.blue() > 225;
    };
    const auto orange = [](Color pixel){
        return pixel.red() > 200 && pixel.blue() < 150;
    };
    for (double dx = -60; dx <= 60; dx += 2){
        if (share_of(screen, shifted(m_icon_left, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_right, dx), bright) >= 0.7 &&
            share_of(screen, shifted(m_icon_stroke, dx), orange) >= 0.7
        ){
            return true;
        }
    }
    return false;
}



//  Measured on 1920x1080 (console B, 2026-11-10, 01.03 run 2's
//  mission01-complete-fullres.png): the word's green fills x 615-1300,
//  y 271-349, none of it 20 px above or below; the bar's yellow fills
//  y 390-585, read left and right of the name as for the banner.
MissionCompleteDetector::MissionCompleteDetector(Color color)
    : m_color(color)
    , m_word(640.0 / 1920, 285.0 / 1080, 640.0 / 1920, 55.0 / 1080)
    , m_above(620.0 / 1920, 245.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_below(620.0 / 1920, 358.0 / 1080, 680.0 / 1920, 20.0 / 1080)
    , m_bar_left(430.0 / 1920, 440.0 / 1080, 390.0 / 1920, 140.0 / 1080)
    , m_bar_right(1100.0 / 1920, 440.0 / 1080, 400.0 / 1920, 140.0 / 1080)
{}

void MissionCompleteDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_word);
    items.add(m_color, m_above);
    items.add(m_color, m_below);
    items.add(m_color, m_bar_left);
    items.add(m_color, m_bar_right);
}

//  Measured on every PLZA lab capture (1109 frames, console B, 2026-11-10):
//  the card's three frames (01.03 run 2's 0045, 0046 and the full-res
//  one) have the word 70% green, nothing green around it and both bar
//  boxes 100% yellow. The closest others: the word 11% green at most
//  (plza-platform's boxes.png, no bar), and with the bar, the mission
//  banners (01.01 run 3's 0181, run 4's 0182-0189, 01.03 run 2's
//  0133-0139) at 2% at most.
bool MissionCompleteDetector::detect(const ImageViewRGB32& screen){
    if (complete_green_share(screen, m_word) < 0.40){
        return false;
    }
    if (complete_green_share(screen, m_above) > 0.10 || complete_green_share(screen, m_below) > 0.10){
        return false;
    }
    return banner_yellow_share(screen, m_bar_left) >= 0.6 && banner_yellow_share(screen, m_bar_right) >= 0.6;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA AutoStory - Opening detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryOpeningDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  A region of the frame and the cells it is averaged into. On 1920x1080
//  (the user's design, 2026-11-10): the top band y 0-300 across; the left
//  side x 0-640 and the right side x 1280-1920, both y 300-1080. The player
//  (x 780-1200, y 330-1080) is in none of them. Each cell is 30x30 (top) or
//  40x39 pixels (sides).
struct PlatformRegion{
    std::string_view name;
    ImageFloatBox box;
    size_t cols;
    size_t rows;
};

const PlatformRegion PLATFORM_REGIONS[] = {
    {"top",   {0.0,       0.0,         1.0,       300.0 / 1080}, 64, 10},
    {"left",  {0.0,       300.0 / 1080, 640.0 / 1920, 780.0 / 1080}, 16, 20},
    {"right", {1280.0 / 1920, 300.0 / 1080, 640.0 / 1920, 780.0 / 1080}, 16, 20},
};

//  The bounds of platform_matches(), from the opening's frames (console B,
//  1920x1080: the platform reference and the 56 cutscene frames before
//  it, measured with this code's cell averages):
//  - the closest cutscene frame is 0056, the train's interior just before
//    the platform: top 112.8, left 82.6, right 71.9; the next are the
//    city flyovers 0022-0024 at top 134.7+, left 95.6+;
//  - the reference against itself moved 16 px: top 42.2, left 20.8,
//    right 21.2; 10% darker: top 24.6, left 19.9, right 16.4.
//  - the platform after loading its autosave (camera turned a little,
//    2026-11-10): top 67.9-68.1, left 25.8-41.7, right 16.5-16.9.
//  So the bounds sit between the loaded view and 0056: top 90 (23 below
//  0056), left 55 (27 below). The right side holds a walking NPC and the companion, so it only
//  has to be roughly the platform (120: a menu or another place is 140+).
constexpr double PLATFORM_TOP_MAX = 90.0;
constexpr double PLATFORM_LEFT_MAX = 55.0;
constexpr double PLATFORM_RIGHT_MAX = 120.0;

using Cells = std::vector<FloatPixel>;

//  `region` of `screen`, averaged into its cells, row by row.
Cells region_cells(const ImageViewRGB32& screen, const PlatformRegion& region){
    Cells cells;
    cells.reserve(region.cols * region.rows);
    const double cell_width = region.box.width / (double)region.cols;
    const double cell_height = region.box.height / (double)region.rows;
    for (size_t row = 0; row < region.rows; row++){
        for (size_t col = 0; col < region.cols; col++){
            const ImageFloatBox cell(
                region.box.x + cell_width * (double)col, region.box.y + cell_height * (double)row,
                cell_width, cell_height
            );
            cells.push_back(image_stats(extract_box_reference(screen, cell)).average);
        }
    }
    return cells;
}

//  "rrggbb" to its color.
FloatPixel hex_pixel(const std::string& hex, size_t offset){
    const auto channel = [&](size_t index){
        return (double)std::stoi(hex.substr(offset + index * 2, 2), nullptr, 16);
    };
    return FloatPixel(channel(0), channel(1), channel(2));
}

//  The reference's cells of `region` from ReferenceData/plza-platform.json:
//  "regions": {<name>: {"cols": n, "rows": ["rrggbb...", ...]}}.
Cells reference_cells(const JsonObject& regions, const PlatformRegion& region){
    const std::string source = "plza-platform.json";
    const JsonObject& entry = regions.get_object_throw(std::string(region.name), source);
    const JsonArray& rows = entry.get_array_throw("rows", source);
    if ((size_t)entry.get_integer_throw("cols", source) != region.cols || rows.size() != region.rows){
        throw ParseException(std::format("{}: region {} is not {}x{} cells.", source, region.name, region.cols, region.rows));
    }
    Cells cells;
    for (const JsonValue& row : rows){
        const std::string& hex = row.to_string_throw(source);
        if (hex.size() != region.cols * 6){
            throw ParseException(std::format("{}: a row of region {} is not {} colors.", source, region.name, region.cols));
        }
        for (size_t col = 0; col < region.cols; col++){
            cells.push_back(hex_pixel(hex, col * 6));
        }
    }
    return cells;
}

//  The reference's cells, per PLATFORM_REGIONS entry.
const std::vector<Cells>& platform_reference(){
    static const std::vector<Cells> reference = []{
        for (const auto& [name, text] : autostory_reference_json()){
            if (name != "plza-platform"){
                continue;
            }
            const JsonValue json = parse_json(text);
            const JsonObject& regions = json.to_object_throw("plza-platform.json").get_object_throw("regions");
            std::vector<Cells> cells;
            for (const PlatformRegion& region : PLATFORM_REGIONS){
                cells.push_back(reference_cells(regions, region));
            }
            return cells;
        }
        throw FileException(nullptr, PA_CURRENT_FUNCTION, "The platform reference is not built in.", "plza-platform.json");
    }();
    return reference;
}

double cells_rmsd(const Cells& reference, const Cells& cells){
    double sum = 0;
    for (size_t index = 0; index < cells.size(); index++){
        const double r = reference[index].r - cells[index].r;
        const double g = reference[index].g - cells[index].g;
        const double b = reference[index].b - cells[index].b;
        sum += r * r + g * g + b * b;
    }
    return std::sqrt(sum / (double)cells.size());
}

} // namespace


PlatformDistances platform_distances(const ImageViewRGB32& screen){
    const std::vector<Cells>& reference = platform_reference();
    double distance[3] = {};
    for (size_t index = 0; index < 3; index++){
        distance[index] = cells_rmsd(reference[index], region_cells(screen, PLATFORM_REGIONS[index]));
    }
    return PlatformDistances{.top = distance[0], .left = distance[1], .right = distance[2]};
}

bool platform_matches(const PlatformDistances& distances){
    return distances.top <= PLATFORM_TOP_MAX
        && distances.left <= PLATFORM_LEFT_MAX
        && distances.right <= PLATFORM_RIGHT_MAX;
}

ImageRGB32 platform_reference_image(size_t width, size_t height){
    ImageRGB32 image(width, height);
    image.fill(0xff000000);
    const std::vector<Cells>& reference = platform_reference();
    for (size_t index = 0; index < 3; index++){
        const PlatformRegion& region = PLATFORM_REGIONS[index];
        const ImagePixelBox box = floatbox_to_pixelbox(width, height, region.box);
        for (size_t y = box.min_y; y < box.max_y; y++){
            for (size_t x = box.min_x; x < box.max_x; x++){
                const size_t col = (x - box.min_x) * region.cols / (box.max_x - box.min_x);
                const size_t row = (y - box.min_y) * region.rows / (box.max_y - box.min_y);
                const FloatPixel& cell = reference[index][row * region.cols + col];
                image.pixel(x, y) = (uint32_t)Color((uint8_t)std::lround(cell.r), (uint8_t)std::lround(cell.g), (uint8_t)std::lround(cell.b));
            }
        }
    }
    return image;
}

std::string to_string(const PlatformDistances& distances){
    return std::format("top {:.1f}, left {:.1f}, right {:.1f}", distances.top, distances.left, distances.right);
}


PlatformArrivalDetector::PlatformArrivalDetector(Color color)
    : m_color(color)
{}

void PlatformArrivalDetector::make_overlays(VideoOverlaySet& items) const{
    for (const PlatformRegion& region : PLATFORM_REGIONS){
        items.add(m_color, region.box);
    }
}

bool PlatformArrivalDetector::detect(const ImageViewRGB32& screen){
    m_last = platform_distances(screen);
    return platform_matches(m_last);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

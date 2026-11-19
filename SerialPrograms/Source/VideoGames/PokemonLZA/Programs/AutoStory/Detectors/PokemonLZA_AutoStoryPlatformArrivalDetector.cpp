/*  PokemonLZA AutoStory - Opening detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <iterator>
#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryPlatformArrivalDetector.h"
#include "PokemonLZA_AutoStoryReferenceCells.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  On 1920x1080: top band y 0-300; sides x 0-640 and 1280-1920, y 300-1080, leaving
//  out the player (x 780-1200). Cells 30x30 (top) or 40x39 (sides).
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

//  Measured against the reference: the train's interior just before the platform
//  top 112.8, left 82.6; the platform loaded from its autosave top 68.1, left 41.7 at
//  most. The right side holds moving NPCs, so it only has to be roughly the platform
//  (a menu or another place is 140+).
constexpr double PLATFORM_TOP_MAX = 90.0;
constexpr double PLATFORM_LEFT_MAX = 55.0;
constexpr double PLATFORM_RIGHT_MAX = 120.0;

//  The largest distance: every channel of every cell 255 apart, sqrt(3) * 255.
constexpr double MAX_PLATFORM_DISTANCE = 441.7;

using Cells = ReferenceCells;

Cells region_cells(const ImageViewRGB32& screen, const PlatformRegion& region){
    return grid_cells(screen, region.box, region.cols, region.rows);
}

//  The reference's cells of `region` from ReferenceData/plza-platform.json:
//  "regions": {<name>: {"cols": n, "rows": ["rrggbb...", ...]}}.
Cells reference_cells(const JsonObject& regions, const PlatformRegion& region){
    const std::string source = "plza-platform.json";
    const JsonObject& entry = regions.get_object_throw(std::string(region.name), source);
    return read_cell_grid(entry, region.cols, region.rows, std::format("{} region {}", source, region.name));
}

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

PlatformDistances platform_distances(const ImageViewRGB32& screen){
    const std::vector<Cells>& reference = platform_reference();
    double distance[std::size(PLATFORM_REGIONS)] = {};
    for (size_t index = 0; index < std::size(PLATFORM_REGIONS); index++){
        distance[index] = cells_rmsd(reference[index], region_cells(screen, PLATFORM_REGIONS[index]));
    }
    return PlatformDistances{.top = distance[0], .left = distance[1], .right = distance[2]};
}

bool platform_matches(const PlatformDistances& distances){
    return distances.top <= PLATFORM_TOP_MAX
        && distances.left <= PLATFORM_LEFT_MAX
        && distances.right <= PLATFORM_RIGHT_MAX;
}

} // namespace


ImageRGB32 platform_reference_image(size_t width, size_t height){
    ImageRGB32 image(width, height);
    image.fill(0xff000000);
    const std::vector<Cells>& reference = platform_reference();
    for (size_t index = 0; index < std::size(PLATFORM_REGIONS); index++){
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
    if (screen.width() == 0 || screen.height() == 0){
        //  No frame: as far from the platform as a frame can be.
        m_last = PlatformDistances{.top = MAX_PLATFORM_DISTANCE, .left = MAX_PLATFORM_DISTANCE, .right = MAX_PLATFORM_DISTANCE};
        return false;
    }
    m_last = platform_distances(screen);
    return platform_matches(m_last);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

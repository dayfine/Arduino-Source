/*  PokemonLZA AutoStory - Minimap match
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "PokemonLZA_AutoStoryMinimapMatch.h"
#include "PokemonLZA_AutoStoryReferenceCells.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The square around the minimap's disk: on 1280x720, centred on (118, 121),
//  132 pixels across (the disk with its rim is about 155), averaged into
//  16 x 16 cells of about 8 pixels.
const ImageFloatBox MINIMAP_BOX(52.0 / 1280, 55.0 / 720, 132.0 / 1280, 132.0 / 720);
constexpr size_t MINIMAP_CELLS = 16;

//  The ring of cells compared, by the distance of a cell's centre from the
//  disk's centre, as a share of the box's half width: from 22 of 66 pixels
//  (outside the player's arrow) to 62 (inside the rim, where the north
//  sign and the objective's marker sit). The camera's cone over the map
//  stays in: it is faint.
constexpr double RING_INNER = 22.0 / 66;
constexpr double RING_OUTER = 62.0 / 66;

//  The largest distance that is the reference's place. Against the 03.04
//  start (Station Front, Korean, console B, 1280x720):
//  - the start itself moved 2 pixels: 8.7 to 15.4; JPEG at quality 40:
//    4.4; scaled to 1920x1080: 2.0; 10% brighter: 22.3 (10% darker: 40.2,
//    no match);
//  - not there (Italian, 1920x1080): 2 s into 03.04's walk from it, 53.5;
//    the end of a failed 03.04 walk along a wall, 47.4; outside the Center
//    after 03.04, 53.5; 1,000+ other frames of Main Mission 03 (streets,
//    Wild Zone 1, an office, menus, Korean and German): 63.9 and up.
//  30 sits between the moved start (15.4) and the nearest other place (47.4).
constexpr double MINIMAP_MATCH_MAX = 30.0;

constexpr std::string_view REFERENCE_PREFIX = "plza-minimap-";

//  The indices of the ring's cells in a MINIMAP_CELLS square grid.
const std::vector<size_t>& ring_cells(){
    static const std::vector<size_t> ring = []{
        std::vector<size_t> indices;
        for (size_t row = 0; row < MINIMAP_CELLS; row++){
            for (size_t col = 0; col < MINIMAP_CELLS; col++){
                const double x = ((double)col + 0.5) / MINIMAP_CELLS * 2 - 1;
                const double y = ((double)row + 0.5) / MINIMAP_CELLS * 2 - 1;
                const double radius_squared = x * x + y * y;
                if (radius_squared >= RING_INNER * RING_INNER && radius_squared <= RING_OUTER * RING_OUTER){
                    indices.push_back(row * MINIMAP_CELLS + col);
                }
            }
        }
        return indices;
    }();
    return ring;
}

//  The ring's cells of a full grid.
ReferenceCells ring_of(const ReferenceCells& grid){
    ReferenceCells ring;
    ring.reserve(ring_cells().size());
    for (size_t index : ring_cells()){
        ring.push_back(grid[index]);
    }
    return ring;
}

//  The built-in minimap references' rings, by name (the file name after
//  "plza-minimap-"). Throws ParseException for a malformed file.
const std::map<std::string, ReferenceCells, std::less<>>& minimap_references(){
    static const std::map<std::string, ReferenceCells, std::less<>> references = []{
        std::map<std::string, ReferenceCells, std::less<>> rings;
        for (const auto& [file, text] : autostory_reference_json()){
            if (!file.starts_with(REFERENCE_PREFIX)){
                continue;
            }
            const std::string source = std::format("{}.json", file);
            const JsonValue json = parse_json(text);
            const JsonObject& grid = json.to_object_throw(source).get_object_throw("grid", source);
            rings.emplace(file.substr(REFERENCE_PREFIX.size()), ring_of(read_cell_grid(grid, MINIMAP_CELLS, MINIMAP_CELLS, source)));
        }
        return rings;
    }();
    return references;
}

} // namespace


std::optional<std::string> matching_minimap(const ImageViewRGB32& screen){
    if (minimap_references().empty()){
        return std::nullopt;
    }
    const ReferenceCells ring = ring_of(grid_cells(screen, MINIMAP_BOX, MINIMAP_CELLS, MINIMAP_CELLS));
    std::optional<std::string> closest;
    double closest_distance = MINIMAP_MATCH_MAX;
    for (const auto& [name, reference] : minimap_references()){
        const double distance = cells_rmsd(reference, ring);
        if (distance <= closest_distance){
            closest = name;
            closest_distance = distance;
        }
    }
    return closest;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

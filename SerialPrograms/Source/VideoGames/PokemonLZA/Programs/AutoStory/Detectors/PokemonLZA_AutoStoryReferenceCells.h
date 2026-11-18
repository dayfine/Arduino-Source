/*  PokemonLZA AutoStory - Reference cells
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A part of the screen averaged into a grid of cells, and the checked-in
 *  references (the JSON files in ReferenceData/) the detectors compare such grids to.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryReferenceCells_H
#define PokemonAutomation_PokemonLZA_AutoStoryReferenceCells_H

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTools/FloatPixel.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The checked-in reference files, ReferenceData/*.json, built into the
//  program (cmake/EmbedTextFiles.cmake): (name, text), by name.
//  plza-platform holds the platform reference's cell colors per region;
//  plza-minimap-<name> a minimap reference (PokemonLZA_AutoStoryMinimapMatch.h).
const std::vector<std::pair<std::string, std::string>>& autostory_reference_json();


//  Cell colors, row by row.
using ReferenceCells = std::vector<FloatPixel>;

//  `box` of `screen` averaged into `cols` x `rows` cells, row by row.
ReferenceCells grid_cells(const ImageViewRGB32& screen, const ImageFloatBox& box, size_t cols, size_t rows);

//  A grid of a reference file, {"cols": n, "rows": ["rrggbb...", ...]},
//  that must be `cols` x `rows` cells. Throws ParseException naming
//  `source` when it is not.
ReferenceCells read_cell_grid(const JsonObject& grid, size_t cols, size_t rows, std::string_view source);

//  How far `cells` are from `reference`, cell for cell: the RMSD over the
//  cells of the summed squared channel differences (0 = the same, at most
//  441.7). `cells` is as long as `reference`.
double cells_rmsd(const ReferenceCells& reference, const ReferenceCells& cells);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

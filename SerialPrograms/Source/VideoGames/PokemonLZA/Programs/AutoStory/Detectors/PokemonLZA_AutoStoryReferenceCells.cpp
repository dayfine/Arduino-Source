/*  PokemonLZA AutoStory - Reference cells
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <cmath>
#include <format>
#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "PokemonLZA_AutoStoryReferenceCells.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The "rrggbb" at `offset` of `hex` as a color. Throws ParseException
//  naming `source` for a character that is not a hex digit.
FloatPixel hex_pixel(std::string_view hex, size_t offset, std::string_view source){
    const auto channel = [&](size_t index){
        const std::string_view digits = hex.substr(offset + index * 2, 2);
        int value = 0;
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
        if (error != std::errc() || end != digits.data() + digits.size()){
            throw ParseException(std::format("{}: \"{}\" is not a hex color channel.", source, digits));
        }
        return (double)value;
    };
    return FloatPixel(channel(0), channel(1), channel(2));
}

} // namespace


ReferenceCells grid_cells(const ImageViewRGB32& screen, const ImageFloatBox& box, size_t cols, size_t rows){
    ReferenceCells cells;
    cells.reserve(cols * rows);
    const double cell_width = box.width / (double)cols;
    const double cell_height = box.height / (double)rows;
    for (size_t row = 0; row < rows; row++){
        for (size_t col = 0; col < cols; col++){
            const ImageFloatBox cell(
                box.x + cell_width * (double)col, box.y + cell_height * (double)row,
                cell_width, cell_height
            );
            cells.push_back(image_stats(extract_box_reference(screen, cell)).average);
        }
    }
    return cells;
}

ReferenceCells read_cell_grid(const JsonObject& grid, size_t cols, size_t rows, std::string_view source){
    const std::string where(source);
    const JsonArray& hex_rows = grid.get_array_throw("rows", where);
    if ((size_t)grid.get_integer_throw("cols", where) != cols || hex_rows.size() != rows){
        throw ParseException(std::format("{}: a grid is not {}x{} cells.", source, cols, rows));
    }
    ReferenceCells cells;
    cells.reserve(cols * rows);
    for (const JsonValue& row : hex_rows){
        const std::string& hex = row.to_string_throw(where);
        if (hex.size() != cols * 6){
            throw ParseException(std::format("{}: a row of a grid is not {} colors.", source, cols));
        }
        for (size_t col = 0; col < cols; col++){
            cells.push_back(hex_pixel(hex, col * 6, source));
        }
    }
    return cells;
}

double cells_rmsd(const ReferenceCells& reference, const ReferenceCells& cells){
    double sum = 0;
    for (size_t index = 0; index < cells.size(); index++){
        const double r = reference[index].r - cells[index].r;
        const double g = reference[index].g - cells[index].g;
        const double b = reference[index].b - cells[index].b;
        sum += r * r + g * g + b * b;
    }
    return std::sqrt(sum / (double)cells.size());
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BDSP AutoStory Map Files Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_MapFiles.h"
#include "PokemonBDSP_AutoStory_MapFiles_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::TileGrid;

struct GlyphCase{
    std::string name;
    std::string json;
    //  The grid's rows, or empty when reading it must throw.
    std::vector<std::string> rows;
};

const std::vector<GlyphCase>& glyph_cases(){
    static const std::vector<GlyphCase> cases{
        {
            .name = "every BDSP tile, and a person standing",
            .json = R"({"name": "T", "source": {"decomp_file": "f", "revision": "r", "date": "d"},
                "tiles": ["#.~", "vDS"],
                "objects": [{"name": "npc_1_0", "kind": "npc", "x": 1, "y": 0}]})",
            .rows = {"#N\"", "vDS"},
        },
        {
            .name = "water has no BDSP tile",
            .json = R"({"name": "T", "source": {"decomp_file": "f", "revision": "r", "date": "d"},
                "tiles": ["#w"]})",
            .rows = {},
        },
        {
            .name = "a wanderer has no BDSP tile",
            .json = R"({"name": "T", "source": {"decomp_file": "f", "revision": "r", "date": "d"},
                "tiles": ["#.."],
                "objects": [{"name": "lass", "kind": "npc", "x": 1, "y": 0, "wander": {"x0": 1, "y0": 0, "x1": 2, "y1": 0}}]})",
            .rows = {},
        },
    };
    return cases;
}

class Test_MapFileGlyphs : public UnitTest{
public:
    Test_MapFileGlyphs()
        : UnitTest("PokemonBDSP::AutoStory - Map files: JSON glyphs as tile grids")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const GlyphCase& item : glyph_cases()){
            std::optional<TileGrid> grid;
            std::string error;
            try{
                grid = tile_grid_from_map(Pokemon::parse_map(parse_json(item.json), item.name));
            }catch (ParseException& e){
                error = e.message();
            }
            if (item.rows.empty()){
                if (grid){
                    return std::format("{}: read a grid, expected a ParseException.", item.name);
                }
                continue;
            }
            if (!grid){
                return std::format("{}: expected a grid, read: {}", item.name, error);
            }
            if (const std::optional<std::string> difference = grid_difference(*grid, TileGrid(item.rows))){
                return std::format("{}: differs from the expected rows at {}.", item.name, *difference);
            }
        }
        return true;
    }
};

//  The files are resources, not in this repository: CI has none and skips.
class Test_MapFilesEqualTables : public UnitTest{
public:
    Test_MapFilesEqualTables()
        : UnitTest("PokemonBDSP::AutoStory - Map files: equal to the C++ tables cell for cell")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string folder = story_map_folder();
        if (!Filesystem::exists(Filesystem::Path(folder + story_map_files()[0].name + ".json"))){
            logger.log("No BDSP map files in " + folder + "; skipped.");
            return true;
        }
        for (const StoryMapFile& file : story_map_files()){
            const std::string path = folder + file.name + ".json";
            try{
                const TileGrid grid = tile_grid_from_map(Pokemon::parse_map(load_json_file(path), path));
                if (const std::optional<std::string> difference = grid_difference(grid, file.table())){
                    return std::format("{}: differs from the C++ table at {}.", path, *difference);
                }
            }catch (Exception& e){
                return std::format("{}: {}: {}", path, e.name(), e.message());
            }
            logger.log(std::format("{}: equal to the C++ table.", path));
        }
        return true;
    }
};

}


void add_tests_AutoStory_MapFiles(UnitTestDatabase& database){
    database.add<Test_MapFileGlyphs>();
    database.add<Test_MapFilesEqualTables>();
}


}
}
}

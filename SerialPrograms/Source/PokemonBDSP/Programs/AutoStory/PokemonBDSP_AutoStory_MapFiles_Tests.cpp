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
#include "Data/PokemonBDSP_AutoStory_MapGrids.h"
#include "Data/PokemonBDSP_AutoStory_Maps_TestData.h"
#include "PokemonBDSP_AutoStory_MapFiles_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::TileGrid;

//  Packages' files are resources, not in this repository: CI has none and
//  skips. Where they are (a lab build), each must equal this build's copy.
class Test_MapFilesEqualBuiltIn : public UnitTest{
public:
    Test_MapFilesEqualBuiltIn()
        : UnitTest("PokemonBDSP::AutoStory - Map files: Packages' equal the built-in copies")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string folder = story_map_folder();
        if (!Filesystem::exists(Filesystem::Path(folder + test_map_json()[0].first + ".json"))){
            logger.log("No BDSP map files in " + folder + "; skipped.");
            return true;
        }
        for (const auto& [name, json] : test_map_json()){
            const std::string path = folder + name + ".json";
            try{
                const TileGrid grid = tile_grid_from_map(Pokemon::parse_map(load_json_file(path), path));
                if (const std::optional<std::string> difference = grid_difference(grid, built_in_grid(name))){
                    return std::format("{}: differs from the built-in copy at {}.", path, *difference);
                }
            }catch (Exception& e){
                return std::format("{}: {}: {}", path, e.name(), e.message());
            }
            logger.log(std::format("{}: equal to the built-in copy.", path));
        }
        return true;
    }
};

}


void add_tests_AutoStory_MapFiles(UnitTestDatabase& database){
    database.add<Test_MapFilesEqualBuiltIn>();
}


}
}
}

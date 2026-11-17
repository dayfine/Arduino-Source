/*  PokemonLZA AutoStory - Launch Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryLaunch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

std::string describe(std::optional<size_t> value){
    return value ? std::format("{}", *value) : std::string("none");
}

//  HOME as the console showed it, one frame per tile from the first: the
//  search reads each frame in turn, a right move showing the next one.
struct TileSearchCase{
    std::string name;
    struct Input{
        std::vector<std::string> files;     //  Under the test images' PokemonLZA/.
        //  The last frame repeats past the end (HOME's row ends there).
    };
    Input input;
    struct Expectation{
        std::optional<size_t> tile = std::nullopt;
    };
    Expectation expectation;
};

const TileSearchCase TILE_SEARCH_CASES[] = {
    {
        //  The 01.01 load that launched FireRed (console B, 2026-11-15):
        //  FireRed played last, Z-A second.
        .name = "FireRed first, Z-A second",
        .input = {.files = {
            "AutoStoryNewGame/2026-11-15/home_firered_first.png",
            "AutoStoryNewGame/2026-11-15/home_plza.png",
        }},
        .expectation = {.tile = 1},
    },
    {
        .name = "Z-A first",
        .input = {.files = {"AutoStoryNewGame/2026-11-15/home_plza.png"}},
        .expectation = {.tile = 0},
    },
    {
        .name = "no Z-A tile",
        .input = {.files = {"AutoStoryNewGame/2026-11-15/home_firered.png"}},
        .expectation = {.tile = std::nullopt},
    },
};

std::vector<std::string> home_tile_images(const TileSearchCase& test){
    std::vector<std::string> paths;
    for (const std::string& file : test.input.files){
        paths.push_back(std::format("PokemonLZA/{}", file));
    }
    return paths;
}

class Test_HomeTileSearch : public ImageUnitTest{
public:
    explicit Test_HomeTileSearch(const TileSearchCase& test)
        : ImageUnitTest(std::format("PokemonLZA::AutoStory - launch: HOME tile search, {}", test.name), home_tile_images(test))
        , m_test(test)
    {}
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        size_t shown = 0;
        size_t moves = 0;
        const std::optional<size_t> tile = find_plza_tile(
            [&]{
                return is_home_tile_plza(images[shown]);
            },
            [&]{
                moves++;
                if (shown + 1 < images.size()){
                    shown++;
                }
            }
        );
        if (tile != m_test.expectation.tile){
            return std::format("Expected PLZA on tile {}, found {}.", describe(m_test.expectation.tile), describe(tile));
        }
        const size_t expected_moves = tile ? *tile : HOME_TILES_SEARCHED - 1;
        if (moves != expected_moves){
            return std::format("Expected {} right moves, made {}.", expected_moves, moves);
        }
        return true;
    }

private:
    const TileSearchCase& m_test;
};

} // namespace


void add_tests_AutoStoryLaunch(UnitTestDatabase& database){
    for (const TileSearchCase& test : TILE_SEARCH_CASES){
        database.add<Test_HomeTileSearch>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

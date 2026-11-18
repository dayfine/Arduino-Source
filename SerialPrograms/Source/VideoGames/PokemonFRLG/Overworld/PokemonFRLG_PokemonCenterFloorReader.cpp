/*  Pokemon Center Floor Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PokemonCenterFloorReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on console B's 1080p captures (2026-11-12/13): the Fuchsia
//  Center's floor reads 245..246, 241..242, 164..167. The range leaves out
//  Route 21's sand (248, 233, 120) and Pallet Town's paths.
bool is_floor_colour(uint32_t pixel){
    const int red = (pixel >> 16) & 0xff;
    const int green = (pixel >> 8) & 0xff;
    const int blue = pixel & 0xff;
    const int red_green = red > green ? red - green : green - red;
    return red >= 225 && green >= 225 && blue >= 140 && blue <= 200 && red_green <= 20;
}

//  The floor covered 15 to 21% of the screen in the Center's far corner,
//  where most of the view is the black outside the room; it covers more
//  below the nurse. Water, sand and town frames read under 2%.
constexpr double MIN_FLOOR_SHARE = 0.08;

} // namespace


double pokemon_center_floor_share(const ImageViewRGB32& game){
    const size_t width = game.width();
    const size_t height = game.height();
    if (width == 0 || height == 0){
        return 0;
    }
    size_t floor = 0;
    for (size_t row = 0; row < height; row++){
        for (size_t col = 0; col < width; col++){
            if (is_floor_colour(game.pixel(col, row))){
                floor++;
            }
        }
    }
    return (double)floor / (double)(width * height);
}

bool shows_pokemon_center_floor(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return pokemon_center_floor_share(game) >= MIN_FLOOR_SHARE;
}


namespace{

//  A console B frame: <anything>_<True|False>.png, True where the player is
//  in a Center.
class Test_PokemonCenterFloorReader : public UnitTest{
public:
    Test_PokemonCenterFloorReader(std::string image, bool expected)
        : UnitTest("PokemonFRLG::PokemonCenterFloorReader - " + image)
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_expected(expected)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        bool read = false;
        try{
            read = shows_pokemon_center_floor(ImageRGB32(m_image));
        }catch (FileException& e){
            //  The frames aren't in CI's CommandLineTests yet.
            return UnitTestResult(UnitTestResult::SKIPPED, "File not found: " + e.message());
        }
        if (read == m_expected){
            return true;
        }
        return std::format("Expected {}, read {}.", m_expected ? "a Center" : "no Center", read ? "a Center" : "none");
    }

private:
    std::string m_image;
    bool m_expected;
};

//  Made-up screens, so the rule is tested without the frames: a third of
//  the floor colour reads as a Center; sand, water and an empty image don't.
class Test_PokemonCenterFloorFixture : public UnitTest{
public:
    Test_PokemonCenterFloorFixture()
        : UnitTest("PokemonFRLG::PokemonCenterFloorReader - fixture")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            std::string_view name;
            uint32_t colour;
            bool expected;
        };
        const Case cases[] = {
            {.name = "floor", .colour = 0xfff6f2a7, .expected = true},
            {.name = "sand", .colour = 0xfff8e978, .expected = false},
            {.name = "water", .colour = 0xff5890e8, .expected = false},
        };
        for (const Case& c : cases){
            ImageRGB32 game(240, 160);
            game.fill(0xff000000);
            for (size_t row = 100; row < 160; row++){
                for (size_t col = 0; col < 240; col++){
                    game.pixel(col, row) = c.colour;
                }
            }
            if ((pokemon_center_floor_share(game) >= MIN_FLOOR_SHARE) != c.expected){
                return std::format("{}: expected {}.", c.name, c.expected ? "a Center" : "no Center");
            }
        }
        if (shows_pokemon_center_floor(ImageRGB32()) || pokemon_center_floor_share(ImageRGB32()) != 0){
            return "An empty image read as a Center.";
        }
        return true;
    }
};

} // namespace


void add_tests_PokemonCenterFloorReader(UnitTestDatabase& database){
    database.add<Test_PokemonCenterFloorFixture>();
    //  LeafGreen on the Switch, console B's 1080p card.
    const std::string folder = "PokemonFRLG/PokemonCenterFloorReader/";
    database.add<Test_PokemonCenterFloorReader>(folder + "FuchsiaCenterWestWall_True.png", true);
    database.add<Test_PokemonCenterFloorReader>(folder + "FuchsiaCenterNorthwestCorner_True.png", true);
    //  12.03, 2026-11-13 04:15-04:16: the Center entry and heal were faked
    //  on Route 21's water.
    database.add<Test_PokemonCenterFloorReader>(folder + "Route21WaterSaveCard_False.png", false);
    database.add<Test_PokemonCenterFloorReader>(folder + "Route21SandBar_False.png", false);
    database.add<Test_PokemonCenterFloorReader>(folder + "Route21Water_False.png", false);
    database.add<Test_PokemonCenterFloorReader>(folder + "PalletTownShore_False.png", false);
    database.add<Test_PokemonCenterFloorReader>(folder + "FuchsiaCityCenterDoor_False.png", false);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

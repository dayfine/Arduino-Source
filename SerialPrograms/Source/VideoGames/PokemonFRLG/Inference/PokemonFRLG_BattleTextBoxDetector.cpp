/*  Battle Text Box Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonTools/Images/SolidColorTest.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_BattleTextBoxDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on the console's captures (1920x1080): blue (34, 88, 121),
//  white (236, 229, 235), gold (222, 176, 52).
const FloatPixel BOX_BLUE{0.176, 0.357, 0.467};
const FloatPixel FRAME_GOLD{0.493, 0.391, 0.116};

}


BattleTextBoxDetector::BattleTextBoxDetector(Color color)
    : m_color(color)
    //  As BattleDialogDetector's.
    , m_blue_top(0.0372308, 0.750154, 0.925538, 0.00934615)
    , m_blue_right(0.956615, 0.7595, 0.00615385, 0.185885)
    , m_white_line(0.0385, 0.7337, 0.923, 0.0058)
    , m_gold_band(0.0385, 0.7163, 0.923, 0.0087)
{}

void BattleTextBoxDetector::make_overlays(VideoOverlaySet& items) const{
    const BoxOption& GAME_BOX = GameSettings::instance().GAME_BOX;
    items.add(m_color, GAME_BOX.inner_to_outer(m_blue_top));
    items.add(m_color, GAME_BOX.inner_to_outer(m_blue_right));
    items.add(m_color, GAME_BOX.inner_to_outer(m_white_line));
    items.add(m_color, GAME_BOX.inner_to_outer(m_gold_band));
}

bool BattleTextBoxDetector::detect(const ImageViewRGB32& screen){
    const ImageViewRGB32 game_screen = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return is_solid(extract_box_reference(game_screen, m_blue_top), BOX_BLUE, 0.25, 20)
        && is_solid(extract_box_reference(game_screen, m_blue_right), BOX_BLUE, 0.25, 20)
        && is_white(extract_box_reference(game_screen, m_white_line), 500, 20)
        && is_solid(extract_box_reference(game_screen, m_gold_band), FRAME_GOLD, 0.10, 40);
}



class Test_BattleTextBoxDetector : public ImageUnitTest{
public:
    Test_BattleTextBoxDetector(const std::string& image, bool expected)
        : ImageUnitTest("PokemonFRLG::BattleTextBoxDetector - " + image, {image})
        , m_expected(expected)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        BattleTextBoxDetector detector(COLOR_RED);
        return detector.detect(images[0]) == m_expected;
    }

private:
    bool m_expected;
};

void add_tests_BattleTextBoxDetector(UnitTestDatabase& database){
    //  Console B's captures: Mt. Moon B1F and Route 1, a battle's opening
    //  with the box still empty, after a leg's watch saw nothing; Viridian
    //  Forest, "Wild CATERPIE" with its HP box.
    database.add<Test_BattleTextBoxDetector>("PokemonFRLG/BattleTextBoxDetector/MtMoonB1F_Opening_True.png", true);
    database.add<Test_BattleTextBoxDetector>("PokemonFRLG/BattleTextBoxDetector/Route1_Opening_True.png", true);
    database.add<Test_BattleTextBoxDetector>("PokemonFRLG/BattleTextBoxDetector/ViridianForest_WildCaterpie_True.png", true);
    //  The cave just before that battle, and black below Pewter's Pokemon
    //  Center, which BattleDialogDetector reads as the box.
    database.add<Test_BattleTextBoxDetector>("PokemonFRLG/BattleTextBoxDetector/MtMoonB1F_Overworld_False.png", false);
    database.add<Test_BattleTextBoxDetector>("PokemonFRLG/BattleTextBoxDetector/PewterPokemonCenter_BlackBelow_False.png", false);
}



}
}
}

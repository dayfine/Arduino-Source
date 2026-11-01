/*  Menu Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonBDSP_MenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


const double STRIPE_TOP = 0.110;
const double STRIPE_TOP_BELOW_BANNER = 0.170;
const double STRIPE_BOTTOM = 0.598;

static ImageFloatBox menu_stripe(size_t index, bool skip_new_banner){
    double top = skip_new_banner ? STRIPE_TOP_BELOW_BANNER : STRIPE_TOP;
    return ImageFloatBox(0.160 + 0.166 * (double)index, top, 0.015, STRIPE_BOTTOM - top);
}

MenuDetector::MenuDetector(Color color, bool skip_new_banner)
    : m_color(color)
    , m_line0(menu_stripe(0, skip_new_banner))
    , m_line1(menu_stripe(1, skip_new_banner))
    , m_line2(menu_stripe(2, skip_new_banner))
    , m_line3(menu_stripe(3, skip_new_banner))
    , m_line4(menu_stripe(4, skip_new_banner))
    , m_cross(0.20, 0.15, 0.60, 0.37)
{}

void MenuDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_line0);
    items.add(m_color, m_line1);
    items.add(m_color, m_line2);
    items.add(m_color, m_line3);
    items.add(m_color, m_line4);
    items.add(m_color, m_cross);
}
//  Whether the five columns between the icons are white from `top` down.
static bool stripes_white(const ImageViewRGB32& screen, double top){
    for (size_t index = 0; index < 5; index++){
        const ImageFloatBox stripe(0.160 + 0.166 * (double)index, top, 0.015, STRIPE_BOTTOM - top);
        if (!is_white(image_stats(extract_box_reference(screen, stripe)))){
            return false;
        }
    }
    return true;
}
bool MenuDetector::detect(const ImageViewRGB32& screen){
    //  A "NEW" tag above an icon (a new game's OPTIONS, next to the third
    //  column) breaks a column at the top; below the tags the columns are
    //  white either way.
    if (!stripes_white(screen, m_line0.y) && !stripes_white(screen, STRIPE_TOP_BELOW_BANNER)){
        return false;
    }
    ImageStats cross = image_stats(extract_box_reference(screen, m_cross));
//    cout << cross.average << cross.stddev << endl;
    if (cross.stddev.sum() < 100){
        return false;
    }
    return true;
}


MenuWatcher::MenuWatcher(Color color, bool skip_new_banner)
    : MenuDetector(color, skip_new_banner)
    , VisualInferenceCallback("MenuWatcher")
{}
void MenuWatcher::make_overlays(VideoOverlaySet& items) const{
    MenuDetector::make_overlays(items);
}
bool MenuWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    return detect(frame);
}




class Test_MenuDetector : public ImageUnitTest{
public:
    Test_MenuDetector(const std::string& image, bool expected)
        : ImageUnitTest("PokemonBDSP::MenuDetector - " + image, {image})
        , m_expected(expected)
    {}

    virtual UnitTestResult run_on_images(
        Logger&, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        return MenuDetector().detect(images[0]) == m_expected;
    }

private:
    bool m_expected;
};

void add_tests_MenuDetector(UnitTestDatabase& database){
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/NewGameBagOptions_True.png", true);
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/Home_False.png", false);
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/OverworldSandgemMart_False.png", false);
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/OverworldOreburghMine_False.png", false);
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/IntroDialog_False.png", false);
    database.add<Test_MenuDetector>("PokemonBDSP/MenuDetector/IntroBarry_False.png", false);
}


}
}
}

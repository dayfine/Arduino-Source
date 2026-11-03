/*  Battle Text Box Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A battle's text box, by its frame: blue inside a white line and a gold
 *  band. It shows from the moment a battle's opening ends, before the HP
 *  boxes and the "Wild <Pokemon> appeared!" that BattleScreenDetector needs:
 *  in Mt. Moon the HP boxes came over 5s after the step that set the battle
 *  off (console B, 2026-11-03). Black below a small room, which passes for
 *  the box's blue (BattleDialogDetector), has neither line.
 *
 *  English layout only: the Japanese box sits a little lower and narrower
 *  (BattleDialogDetector) and wasn't checked.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BattleTextBoxDetector_H
#define PokemonAutomation_PokemonFRLG_BattleTextBoxDetector_H

#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


class BattleTextBoxDetector : public StaticScreenDetector{
public:
    BattleTextBoxDetector(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_blue_top;
    ImageFloatBox m_blue_right;
    ImageFloatBox m_white_line;
    ImageFloatBox m_gold_band;
};


void add_tests_BattleTextBoxDetector(UnitTestDatabase& database);



}
}
}
#endif

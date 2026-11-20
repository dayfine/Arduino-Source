/*  PokemonLZA AutoStory - Mission banner detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detectors for main-mission start banners and mission-complete cards.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMissionBannerDetectors_H
#define PokemonAutomation_PokemonLZA_AutoStoryMissionBannerDetectors_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects a main mission's start banner without OCR (language-independent).
class MainMissionBannerDetector : public StaticScreenDetector{
public:
    explicit MainMissionBannerDetector(Color color = COLOR_ORANGE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_bar_left;
    ImageFloatBox m_bar_right;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
    ImageFloatBox m_icon_left;
    ImageFloatBox m_icon_right;
    ImageFloatBox m_icon_stroke;
};

//  Detects the "COMPLETE" card shown when a main mission finishes (waits for A).
class MissionCompleteDetector : public StaticScreenDetector{
public:
    explicit MissionCompleteDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_word;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
    ImageFloatBox m_bar_left;
    ImageFloatBox m_bar_right;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

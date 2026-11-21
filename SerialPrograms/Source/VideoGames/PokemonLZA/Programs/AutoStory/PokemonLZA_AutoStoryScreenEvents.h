/*  PokemonLZA AutoStory - Screen events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's detectors exposed as shared ScreenEvents.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H
#define PokemonAutomation_PokemonLZA_AutoStoryScreenEvents_H

#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryDialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Maps PLZA screen detectors to shared ScreenEvents.
class PlzaScreenObserver : public ScreenEventObserver{
public:
    PlzaScreenObserver();

    virtual ScreenEvents observe(const VideoSnapshot& frame, WallClock now) override;

    //  Runs detection directly on an image view without a VideoSnapshot wrapper.
    ScreenEvents read(const ImageViewRGB32& screen);

    virtual void make_overlays(VideoOverlaySet& items) const override;

private:
    StoryDialogDetector m_dialog;
    SelectionArrowDetector m_choice_arrow;
    ButtonDetector m_interact;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

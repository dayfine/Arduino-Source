/*  PokemonLZA AutoStory - Dialog Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Any of the story's dialogue boxes, from the upstream dialog detectors.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogDetector_H

#include "Common/Cpp/Time.h"
#include "CommonTools/VisualDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How much of a white dialogue box StoryDialogDetector needs.
enum class WhiteDialogPart{
    BOX_AND_ARROW,  //  FlatWhiteDialogDetector: the box with its advance arrow
    BOX_ONLY,       //  its bottom edge only: the box while the text still types
};

//  Which dialogue boxes StoryDialogDetector counts, besides blue and teal.
struct StoryDialogKinds{
    WhiteDialogPart white = WhiteDialogPart::BOX_AND_ARROW;
    bool item_receive = false;  //  ItemReceiveDetector's item card
};

//  Any dialogue box the story shows: white, blue or teal (and the item card
//  when `kinds` asks for it).
class StoryDialogDetector : public StaticScreenDetector{
public:
    explicit StoryDialogDetector(StoryDialogKinds kinds = {});

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    StoryDialogKinds m_kinds;
    FlatWhiteDialogDetector m_white;
    BlueDialogDetector m_blue;
    TealDialogDetector m_teal;
    ItemReceiveDetector m_item_receive;
};
//  StoryDialogDetector on the video, held `hold_duration`.
class StoryDialogWatcher : public DetectorToFinder<StoryDialogDetector>{
public:
    explicit StoryDialogWatcher(StoryDialogKinds kinds = {}, Milliseconds hold_duration = Milliseconds(250))
        : DetectorToFinder("StoryDialogWatcher", hold_duration, kinds)
    {}
};


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

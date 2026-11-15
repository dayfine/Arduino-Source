/*  PokemonLZA AutoStory - Dialog Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonLZA_AutoStoryDialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


StoryDialogDetector::StoryDialogDetector(StoryDialogKinds kinds)
    : m_kinds(kinds)
    , m_white(COLOR_RED)
    , m_blue(COLOR_BLUE)
    , m_teal(COLOR_GREEN)
    , m_item_receive(COLOR_CYAN)
{}

void StoryDialogDetector::make_overlays(VideoOverlaySet& items) const{
    m_white.make_overlays(items);
    m_blue.make_overlays(items);
    m_teal.make_overlays(items);
    if (m_kinds.item_receive){
        m_item_receive.make_overlays(items);
    }
}

bool StoryDialogDetector::detect(const ImageViewRGB32& screen){
    const bool white = m_kinds.white == WhiteDialogPart::BOX_ONLY ? m_white.detect_box(screen) : m_white.detect(screen);
    return white || m_blue.detect(screen) || m_teal.detect(screen) ||
        (m_kinds.item_receive && m_item_receive.detect(screen));
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

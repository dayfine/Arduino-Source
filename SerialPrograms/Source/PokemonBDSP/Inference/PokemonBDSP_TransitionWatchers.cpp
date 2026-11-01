#include "PokemonBDSP_TransitionWatchers.h"
namespace PokemonAutomation{ namespace NintendoSwitch{ namespace PokemonBDSP{
WhiteScreenOverWatcher::WhiteScreenOverWatcher(Color color, const ImageFloatBox& box)
    : VisualInferenceCallback("WhiteScreenOverWatcher")
    , m_on(color, box, WhiteScreenWatcher::FinderType::PRESENT, std::chrono::milliseconds(100))
    , m_off(color, box, WhiteScreenWatcher::FinderType::GONE, std::chrono::milliseconds(100))
{}
void WhiteScreenOverWatcher::make_overlays(VideoOverlaySet& items) const{
    m_on.make_overlays(items);
}
bool WhiteScreenOverWatcher::process_frame(const ImageViewRGB32& frame, WallClock timestamp){
    if (m_white_is_over.load(std::memory_order_acquire)){
        return true;
    }
    if (!m_has_been_white){
        m_has_been_white = m_on.process_frame(frame, timestamp);
        return false;
    }
    bool is_over = m_off.process_frame(frame, timestamp);
    if (!is_over){
        return false;
    }
    m_white_is_over.store(true, std::memory_order_release);
    return true;
}
bool WhiteScreenOverWatcher::white_is_over(const ImageViewRGB32& frame){
    return m_white_is_over.load(std::memory_order_acquire);
}

}}}

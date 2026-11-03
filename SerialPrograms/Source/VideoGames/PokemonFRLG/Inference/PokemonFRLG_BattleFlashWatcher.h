/*  Battle Flash Watcher
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Every wild battle starts with the overworld flashing dark twice, within
 *  half a second of the step that set it off, before the battle's own wipe.
 *  The wipe and the battle screen differ by map and come seconds later, so
 *  the flash is the earliest and the same sign of a battle everywhere.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BattleFlashWatcher_H
#define PokemonAutomation_PokemonFRLG_BattleFlashWatcher_H

#include <deque>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Fires when the game screen darkens by a fifth or more and starts to come
//  back within a moment. A warp's fade, or the Switch dimming its screen,
//  only darkens and so doesn't count.
class BattleFlashWatcher : public VisualInferenceCallback{
public:
    BattleFlashWatcher(Color color);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;

private:
    Color m_color;
    //  Recent brightness of the game screen.
    std::deque<std::pair<WallClock, double>> m_history;
};



}
}
}
#endif

/*  AutoStory Slope Message
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When a search on Route 17's slope (PokemonFRLG_AutoStorySlope.h) takes
 *  a message for its item's, shared by the console's watcher and the
 *  slope's model in the tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySlopeMessage_H
#define PokemonAutomation_PokemonFRLG_AutoStorySlopeMessage_H

#include <optional>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether a search's message opened, from whether each frame of a batch
//  shows one. A message already showing when the batch began is the last
//  item's, still closing under the batch's first presses: taken for the
//  MAX ELIXIR's 0.4 s after the RARE CANDY's opened, it cut that search
//  short before its first tap, and the MAX ELIXIR was never picked up
//  (console B, 2026-11-12). A message counts once it has shown for HOLD
//  after a frame without one.
class SlopeMessageGate{
public:
    //  As WhiteDialogWatcher holds its dialog.
    static constexpr Milliseconds HOLD = Milliseconds(250);

    bool opened(bool shows, WallClock now){
        if (!shows){
            m_clear_seen = true;
            m_since.reset();
            return false;
        }
        if (!m_clear_seen){
            return false;
        }
        if (!m_since){
            m_since = now;
        }
        return now - *m_since >= HOLD;
    }

private:
    bool m_clear_seen = false;
    std::optional<WallClock> m_since = std::nullopt;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

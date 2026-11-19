/*  PokemonLZA AutoStory - Signal probe
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  For the log only: when each of the overlapping screen signals first
 *  showed over one wait, so a decision's "[Signals]" line shows which signal
 *  decided it and which others backed it (which are redundant). No decision
 *  reads it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySignalProbe_H
#define PokemonAutomation_PokemonLZA_AutoStorySignalProbe_H

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The signals a "[Signals]" line times, in its order.
constexpr size_t PROBED_SIGNALS = 9;

class SignalProbe{
public:
    //  Notes what `look` shows; the first look after a restart starts the wait.
    void look(const ScreenObservation& look);
    //  A new wait from the next look (after a press).
    void restart();

    //  "[Signals] <where> <decision> by=<signal> after=<ms> first: party_bar@<ms>
    //  ... last_busy@<ms>": ms from the wait's first look, "-" for never
    //  seen; the decision is on the last look. Busy is text, a fade or a choice.
    std::string line(std::string_view where, std::string_view decision, std::string_view by) const;

private:
    std::optional<WallClock> m_start;
    WallClock m_last = WallClock::min();
    std::array<std::optional<WallClock>, PROBED_SIGNALS> m_first = {};
    std::optional<WallClock> m_last_busy;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

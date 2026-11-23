/*  BDSP Blink Sample Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A text record of one blink recovery: every eye-match sample, the
 *  thresholds, the blink events and each solver decision, so a failed
 *  attempt can be replayed offline. SerialPrograms/Scripts/PokemonBDSP/
 *  blink_log_to_stream.py turns it into JSON lines.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_BlinkSampleLog_H
#define PokemonAutomation_PokemonBDSP_BlinkSampleLog_H

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Pokemon/Pokemon_Xorshift128.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_BlinkExtraction.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_EyeBlinkDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


struct BlinkSolverDecision{
    //  "provisional", "changed", "dropped", "confirmed", "solve_failed" or "gave_up"
    std::string_view kind = {};
    size_t rolls = 0;
    std::optional<Pokemon::Xorshift128State> state = {};
    std::string_view note = {};
};


//  Writes nothing when default-constructed or when the file cannot be opened.
//  Times are seconds since the log was opened.
class BlinkSampleLog{
public:
    BlinkSampleLog() = default;
    explicit BlinkSampleLog(const std::string& path);

    bool is_open() const{ return m_file.is_open(); }

    //  Starts a new recovery (bedroom, lake) with fresh watchers.
    void begin_recovery(
        std::string_view phase, uint8_t npcs,
        const std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers
    );

    //  Takes each poll's whole sample window and writes only the samples
    //  newer than those already written for that watcher.
    void add_samples(const std::vector<std::vector<BlinkMatchSample>>& matches);

    //  "estimated" (from `samples` samples) or "frozen".
    void threshold(size_t watcher, std::string_view kind, double value, size_t samples);

    //  Samples before `keep_from` were dropped from the solver's window.
    //  Written only when the cut moves.
    void window(size_t watcher, WallClock keep_from);

    //  The decision, then the blink events it was made from.
    void decision(
        const BlinkSolverDecision& decision,
        const std::vector<std::vector<BlinkEvent>>& streams,
        WallClock origin
    );

private:
    double seconds(WallClock time) const;
    void write(std::string_view line);

private:
    FileIO m_file;
    WallClock m_epoch = current_time();
    std::vector<WallClock> m_written_until;
    std::vector<WallClock> m_window;
};


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

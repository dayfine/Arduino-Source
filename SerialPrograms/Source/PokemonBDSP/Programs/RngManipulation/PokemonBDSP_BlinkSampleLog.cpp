/*  BDSP Blink Sample Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "PokemonBDSP_BlinkSampleLog.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Record lines are tab-separated, one record per line:
//    recovery  <t> <phase> <npcs>
//    watcher   <index> <label>
//    sample    <t> <watcher> <score>
//    threshold <t> <watcher> estimated|frozen <value> <samples>
//    window    <t> <watcher> <keep_from>
//    decision  <t> <kind> <rolls> <state or -> <note>
//    events    <t> <watcher> <seconds>:<S|D>,...
constexpr std::string_view BLINK_SAMPLE_LOG_HEADER =
    "# BDSP blink sample log, format 1\n"
    "# t: seconds since this log opened, host steady clock.\n"
    "# sample t: frame arrival, the host time Qt delivered the video frame\n"
    "#   (VideoSnapshot::timestamp), not the time the eye matcher read it.\n";


BlinkSampleLog::BlinkSampleLog(const std::string& path)
    : m_file(Filesystem::Path(path), FileMode::WRITE | FileMode::BINARY)
{
    write(BLINK_SAMPLE_LOG_HEADER);
    write(std::format("# opened {}\n", now_to_filestring()));
}

double BlinkSampleLog::seconds(WallClock time) const{
    return std::chrono::duration<double>(time - m_epoch).count();
}

void BlinkSampleLog::write(std::string_view line){
    m_file.write(line.data(), line.size());
}

void BlinkSampleLog::begin_recovery(
    std::string_view phase, uint8_t npcs,
    const std::vector<std::unique_ptr<EyeBlinkWatcher>>& watchers
){
    write(std::format("recovery\t{:.3f}\t{}\t{}\n", seconds(current_time()), phase, npcs));
    for (size_t w = 0; w < watchers.size(); w++){
        write(std::format("watcher\t{}\t{}\n", w, watchers[w]->label()));
    }
    m_written_until.assign(watchers.size(), WallClock::min());
    m_window.assign(watchers.size(), WallClock::min());
    m_file.flush();
}

void BlinkSampleLog::add_samples(const std::vector<std::vector<BlinkMatchSample>>& matches){
    if (m_written_until.size() < matches.size()){
        m_written_until.resize(matches.size(), WallClock::min());
    }
    std::string text;
    for (size_t w = 0; w < matches.size(); w++){
        for (const BlinkMatchSample& sample : matches[w]){
            if (sample.timestamp <= m_written_until[w]){
                continue;
            }
            text += std::format("sample\t{:.4f}\t{}\t{:.4f}\n", seconds(sample.timestamp), w, sample.match);
            m_written_until[w] = sample.timestamp;
        }
    }
    write(text);
    m_file.flush();
}

void BlinkSampleLog::threshold(size_t watcher, std::string_view kind, double value, size_t samples){
    write(std::format(
        "threshold\t{:.3f}\t{}\t{}\t{:.5f}\t{}\n",
        seconds(current_time()), watcher, kind, value, samples
    ));
}

void BlinkSampleLog::window(size_t watcher, WallClock keep_from){
    if (m_window.size() <= watcher){
        m_window.resize(watcher + 1, WallClock::min());
    }
    if (m_window[watcher] == keep_from){
        return;
    }
    m_window[watcher] = keep_from;
    write(std::format("window\t{:.3f}\t{}\t{:.4f}\n", seconds(current_time()), watcher, seconds(keep_from)));
}

void BlinkSampleLog::decision(
    const BlinkSolverDecision& decision,
    const std::vector<std::vector<BlinkEvent>>& streams,
    WallClock origin
){
    const double now = seconds(current_time());
    write(std::format(
        "decision\t{:.3f}\t{}\t{}\t{}\t{}\n",
        now, decision.kind, decision.rolls,
        decision.state ? decision.state->to_string() : "-",
        decision.note.empty() ? "-" : decision.note
    ));
    const double offset = streams.empty() ? 0 : seconds(origin);
    for (size_t w = 0; w < streams.size(); w++){
        std::string events;
        for (const BlinkEvent& event : streams[w]){
            events += std::format(
                "{}{:.4f}:{}", events.empty() ? "" : ",",
                offset + event.seconds, event.type == BlinkType::Double ? 'D' : 'S'
            );
        }
        write(std::format("events\t{:.3f}\t{}\t{}\n", now, w, events));
    }
    m_file.flush();
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

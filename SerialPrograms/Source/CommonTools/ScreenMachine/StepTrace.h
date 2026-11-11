/*  Step Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step log for closed-loop routines: every change of recognized screen is
 *  logged and saved as a numbered screenshot, every input is logged, and a
 *  failure reports the screens that led up to it. A trace of observations
 *  (ScreenObservation.h), which may come from a simulation, logs the same
 *  lines with no screenshots.
 *
 */

#ifndef PokemonAutomation_CommonTools_StepTrace_H
#define PokemonAutomation_CommonTools_StepTrace_H

#include <deque>
#include <functional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"

namespace PokemonAutomation{


// Recorder that captures screen transitions and recent actions for failure diagnostics.
class StepTrace{
public:
    //  Screenshots go to <Screenshots>/StepTrace/<time>-<label>/.
    StepTrace(VideoStream& stream, std::string label);

    //  A trace with no video: lines go to `log`, no screenshots are saved.
    StepTrace(std::string label, std::function<void(const std::string& line)> log);

    //  Records the screen the routine recognized. Logs and saves a screenshot only
    //  when it differs from the previous one.
    void screen(const std::string& name);

    //  Records an input or decision.
    void action(const std::string& what);

    //  The last few screens, oldest first, e.g. "dialog -> keyboard -> dialog".
    std::string recent_screens() const;

    //  Throws OperationFailedExceptionWithScreenshot with the recent screens
    //  appended; OperationFailedException for a trace with no video.
    [[noreturn]] void fail(const std::string& message) const;

private:
    void log(const std::string& line) const;
    //  Saves the screen as screenshot `m_count`; the path, or why there is none.
    std::string save_screenshot(const std::string& name) const;

    //  Null for a trace with no video.
    VideoStream* m_stream = nullptr;
    std::function<void(const std::string& line)> m_log;
    std::string m_label;
    std::string m_folder;
    WallClock m_start;
    size_t m_count = 0;
    std::string m_last;
    std::deque<std::string> m_recent;
};



} // namespace PokemonAutomation
#endif

/*  BDSP AutoStory - Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A step log for closed-loop routines: every change of recognized screen is
 *  logged and saved as a numbered screenshot, every input is logged, and a
 *  failure reports the screens that led up to it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Trace_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Trace_H

#include <deque>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class AutoStoryTrace{
public:
    //  Screenshots go to <Screenshots>/AutoStory/<time>-<label>/.
    AutoStoryTrace(VideoStream& stream, std::string label);

    //  Records the screen the routine recognized. Logs and saves a screenshot only
    //  when it differs from the previous one.
    void screen(const std::string& name);

    //  Records an input or decision.
    void action(const std::string& what);

    //  The last few screens, oldest first, e.g. "dialog -> keyboard -> dialog".
    std::string recent_screens() const;

    //  Throws OperationFailedExceptionWithScreenshot with the recent screens appended.
    [[noreturn]] void fail(const std::string& message) const;

private:
    VideoStream& m_stream;
    std::string m_label;
    std::string m_folder;
    WallClock m_start;
    size_t m_count = 0;
    std::string m_last;
    std::deque<std::string> m_recent;
};



}
}
}
#endif

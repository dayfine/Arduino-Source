/*  BDSP AutoStory - Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdio>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "PokemonBDSP_AutoStory_Trace.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


AutoStoryTrace::AutoStoryTrace(VideoStream& stream, std::string label)
    : m_stream(stream)
    , m_label(std::move(label))
    , m_folder(SCREENSHOTS_PATH() + "AutoStory/" + now_to_filestring() + "-" + m_label + "/")
    , m_start(current_time())
{
    Filesystem::create_directories(Filesystem::Path(m_folder));
    m_stream.log("[" + m_label + "] Trace screenshots: " + m_folder);
}

void AutoStoryTrace::screen(const std::string& name){
    if (name == m_last){
        return;
    }
    m_last = name;
    m_recent.push_back(name);
    if (m_recent.size() > 12){
        m_recent.pop_front();
    }
    m_count++;
    const double seconds = std::chrono::duration<double>(current_time() - m_start).count();
    char prefix[16];
    std::snprintf(prefix, sizeof(prefix), "%03zu-", m_count);
    std::string path = m_folder + prefix + name + ".png";
    VideoSnapshot screen = m_stream.video().snapshot();
    if (screen){
        screen.frame->save(path);
    }else{
        path = "(no video frame)";
    }
    m_stream.log("[" + m_label + "] screen " + std::to_string(m_count) + ": " + name +
        " at " + tostr_fixed(seconds, 1) + "s, " + path);
}

void AutoStoryTrace::action(const std::string& what){
    m_stream.log("[" + m_label + "] " + what);
}

std::string AutoStoryTrace::recent_screens() const{
    std::string text;
    for (const std::string& name : m_recent){
        text += (text.empty() ? "" : " -> ") + name;
    }
    return text.empty() ? "(none)" : text;
}

void AutoStoryTrace::fail(const std::string& message) const{
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        m_label + ": " + message + " Recent screens: " + recent_screens() + ". Trace: " + m_folder,
        m_stream
    );
}



}
}
}

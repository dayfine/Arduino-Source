/*  Step Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdio>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "StepTrace.h"

namespace PokemonAutomation{


StepTrace::StepTrace(VideoStream& stream, std::string label)
    : m_stream(stream)
    , m_label(std::move(label))
    , m_folder(std::format("{}StepTrace/{}-{}/", SCREENSHOTS_PATH(), now_to_filestring(), m_label))
    , m_start(current_time())
{
    Filesystem::create_directories(Filesystem::Path(m_folder));
    m_stream.log(std::format("[{}] Trace screenshots: {}", m_label, m_folder));
}

void StepTrace::screen(const std::string& name){
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
    std::string path = std::format("{}{:03}-{}.png", m_folder, m_count, name);
    VideoSnapshot screen = m_stream.video().snapshot();
    if (screen){
        screen.frame->save(path);
    }else{
        path = "(no video frame)";
    }
    m_stream.log(std::format(
        "[{}] screen {}: {} at {}s, {}", m_label, m_count, name, tostr_fixed(seconds, 1), path
    ));
}

void StepTrace::action(const std::string& what){
    m_stream.log(std::format("[{}] {}", m_label, what));
}

std::string StepTrace::recent_screens() const{
    std::string text;
    for (const std::string& name : m_recent){
        if (!text.empty()){
            text += " -> ";
        }
        text += name;
    }
    return text.empty() ? "(none)" : text;
}

void StepTrace::fail(const std::string& message) const{
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("{}: {} Recent screens: {}. Trace: {}", m_label, message, recent_screens(), m_folder),
        m_stream
    );
}



} // namespace PokemonAutomation

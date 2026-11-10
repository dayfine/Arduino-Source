/*  Step Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdio>
#include <format>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "StepTrace.h"

namespace PokemonAutomation{


StepTrace::StepTrace(VideoStream& stream, std::string label)
    : m_stream(&stream)
    , m_label(std::move(label))
    , m_folder(SCREENSHOTS_PATH() + "StepTrace/" + now_to_filestring() + "-" + m_label + "/")
    , m_start(current_time())
{
    Filesystem::create_directories(Filesystem::Path(m_folder));
    log(std::format("[{}] Trace screenshots: {}", m_label, m_folder));
}

StepTrace::StepTrace(std::string label, std::function<void(const std::string& line)> log)
    : m_log(std::move(log))
    , m_label(std::move(label))
    , m_start(current_time())
{}

void StepTrace::log(const std::string& line) const{
    if (m_stream != nullptr){
        m_stream->log(line);
    }else if (m_log){
        m_log(line);
    }
}

std::string StepTrace::save_screenshot(const std::string& name) const{
    if (m_stream == nullptr){
        return "(no video)";
    }
    const std::string path = std::format("{}{:03}-{}.png", m_folder, m_count, name);
    VideoSnapshot screen = m_stream->video().snapshot();
    if (!screen){
        return "(no video frame)";
    }
    screen.frame->save(path);
    return path;
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
    const std::string path = save_screenshot(name);
    log(std::format("[{}] screen {}: {} at {}s, {}", m_label, m_count, name, tostr_fixed(seconds, 1), path));
}

void StepTrace::action(const std::string& what){
    log(std::format("[{}] {}", m_label, what));
}

std::string StepTrace::recent_screens() const{
    std::string text;
    for (const std::string& name : m_recent){
        text += (text.empty() ? "" : " -> ") + name;
    }
    return text.empty() ? "(none)" : text;
}

void StepTrace::fail(const std::string& message) const{
    if (m_stream == nullptr){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("{}: {} Recent screens: {}.", m_label, message, recent_screens())
        );
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        m_label + ": " + message + " Recent screens: " + recent_screens() + ". Trace: " + m_folder,
        *m_stream
    );
}



} // namespace PokemonAutomation

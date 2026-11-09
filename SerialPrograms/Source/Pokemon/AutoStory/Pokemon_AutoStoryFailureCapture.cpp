/*  AutoStory Failure Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Logging/GlobalLogger.h"
#include "Common/Cpp/Logging/LastLogTracker.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Recording/StreamHistorySession.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Pokemon_AutoStoryFailureCapture.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  Log lines kept in Failure.json.
constexpr size_t LOG_LINES = 30;

bool folder_safe(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '.' || c == '-' || c == '_';
}

std::string folder_safe(std::string_view text){
    std::string safe(text);
    for (char& c : safe){
        if (!folder_safe(c)){
            c = '_';
        }
    }
    return safe;
}

std::vector<std::string> recent_log(){
    std::vector<std::string> lines;
    for (const LogLine& line : global_last_log_history().get_recent(LOG_LINES)){
        lines.push_back(line.text);
    }
    return lines;
}

} // namespace


std::string failure_folder_name(std::string_view time, const FailureRecord& record){
    const std::string_view event = record.event.empty() ? std::string_view("checkpoint") : std::string_view(record.event);
    return folder_safe(time) + "-" + folder_safe(record.checkpoint) + "-" + folder_safe(event);
}

JsonObject failure_json(const FailureRecord& record){
    JsonObject json;
    json["game"] = record.game;
    json["checkpoint"] = record.checkpoint;
    json["event"] = record.event;
    json["error"] = record.error;
    JsonArray log;
    for (const std::string& line : record.log){
        log.push_back(line);
    }
    json["log"] = std::move(log);
    return json;
}

StatusStep step_of_status(std::string_view line){
    const size_t space = line.find(' ');
    const size_t slash = line.find('/');
    if (space == std::string_view::npos || slash == std::string_view::npos || slash > space){
        return {};
    }
    const size_t colon = line.find(':', space + 1);
    if (colon == std::string_view::npos){
        return {};
    }
    return StatusStep{
        std::string(line.substr(0, slash)),
        std::string(line.substr(space + 1, colon - space - 1)),
    };
}


FailureCapture::FailureCapture(VideoStream& stream, std::string game)
    : m_stream(stream)
    , m_game(std::move(game))
{}

void FailureCapture::attach(SegmentHooks& hooks){
    hooks.status = [this, status = std::move(hooks.status)](const std::string& line) mutable{
        StatusStep step = step_of_status(line);
        if (!step.checkpoint.empty()){
            m_last_step = std::move(step);
        }
        if (status){
            status(line);
        }
    };
    hooks.failed = [this](const std::string& checkpoint, const std::string& event, const Outcome& failure){
        save(checkpoint, event, to_string(failure));
    };
}

void FailureCapture::save_thrown(const std::string& error){
    const std::string checkpoint = m_last_step.checkpoint.empty() ? "unknown" : m_last_step.checkpoint;
    save(checkpoint, m_last_step.event, error);
}

std::string FailureCapture::save(const std::string& checkpoint, const std::string& event, const std::string& error){
    try{
        const VideoSnapshot snapshot = m_stream.video().snapshot();
        FailureRecord record{m_game, checkpoint, event, error, recent_log()};
        const std::string folder = USER_FILE_PATH() + m_game + "/AutoStoryFailures/"
            + failure_folder_name(now_to_filestring(), record) + "/";
        Filesystem::create_directories(Filesystem::Path(folder));
        if (!Filesystem::is_directory(Filesystem::Path(folder))){
            m_stream.log("AutoStory failure capture: cannot make " + folder, COLOR_RED);
            return "";
        }
        if (!snapshot || !snapshot->save(folder + "Screenshot.png")){
            m_stream.log("AutoStory failure capture: no screenshot.", COLOR_ORANGE);
        }
        JsonValue(failure_json(record)).dump(folder + "Failure.json");
        if (!m_stream.save_stream_history(folder + "Video.mp4")){
            m_stream.log("AutoStory failure capture: no video (is Stream History on?).", COLOR_ORANGE);
        }
        m_stream.log("AutoStory failure capture: " + folder, COLOR_BLUE);
        return folder;
    }catch (Exception& e){
        m_stream.log("AutoStory failure capture failed: " + e.message(), COLOR_RED);
    }catch (std::exception& e){
        m_stream.log(std::string("AutoStory failure capture failed: ") + e.what(), COLOR_RED);
    }
    return "";
}



} // namespace Pokemon
} // namespace PokemonAutomation

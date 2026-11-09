/*  Command File Recorder
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/FilePath.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "NintendoSwitch_CommandFileRecorder.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


namespace{

//  UTC, to the millisecond: "2026-11-08T21:04:12.517Z".
std::string utc_time(WallClock time){
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::milliseconds>(time));
}

} // namespace


std::string command_file_step_json(const CommandFileStep& step){
    JsonObject json;
    json["line"] = (int64_t)step.line;
    json["command"] = step.words.empty() ? std::string() : step.words[0];
    JsonArray args;
    for (size_t c = 1; c < step.words.size(); c++){
        args.push_back(step.words[c]);
    }
    json["args"] = std::move(args);
    json["start"] = utc_time(step.start);
    json["end"] = utc_time(step.end);
    json["duration_ms"] = (int64_t)std::chrono::duration_cast<Milliseconds>(step.end - step.start).count();
    if (!step.before.empty()){
        json["before"] = step.before.clone();
    }
    if (!step.after.empty()){
        json["after"] = step.after.clone();
    }
    json["ok"] = !step.error.has_value();
    if (step.error){
        json["error"] = *step.error;
    }
    json["screenshot"] = step.screenshot;
    //  -1: no indentation and no line breaks.
    return json.dump(-1);
}

void append_command_file_step(const std::string& path, const CommandFileStep& step){
    const std::string line = command_file_step_json(step) + "\n";
    FileIO file(Filesystem::Path(path), FileMode::APPEND | FileMode::BINARY);
    if (file.is_open()){
        file.write(line.data(), line.size());
    }
}



} // namespace NintendoSwitch
} // namespace PokemonAutomation

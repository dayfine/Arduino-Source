/*  Lab Run Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <ctime>
#include <exception>
#include <format>
#include <sstream>
#include <vector>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonFramework/BuildCommit.h"
#include "LabRunFiles.h"

namespace PokemonAutomation{

namespace{

//  A file name from a checkpoint or step label: anything but letters,
//  digits, '.', '_' and '-' becomes '_'.
std::string file_name_part(const std::string& text){
    std::string part = text;
    for (char& ch : part){
        const bool keep = std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' || ch == '-';
        if (!keep){
            ch = '_';
        }
    }
    return part;
}

//  Saves the frame a terminal run ended with. Returns its file name.
std::optional<std::string> save_result_frame(const RunRecord& record, const std::string& folder){
    if (record.status != RunStatus::TERMINAL || !record.result || !record.result->frame){
        return std::nullopt;
    }
    std::string name = record.result->outcome == RunOutcome::FAILED ? "frame-failure.png" : "frame-end.png";
    if (!record.result->frame->save(folder + name)){
        return std::nullopt;
    }
    return name;
}

JsonObject event_json(const RunEvent& event, const std::optional<std::string>& frame_file){
    JsonObject json;
    json["time"] = lab_time_text(event.time);
    json["kind"] = event.kind;
    if (event.checkpoint){
        json["checkpoint"] = *event.checkpoint;
    }
    if (event.step){
        json["step"] = *event.step;
    }
    if (event.attempt){
        json["attempt"] = *event.attempt;
    }
    json["text"] = event.text;
    if (frame_file){
        json["frame"] = *frame_file;
    }
    if (event.pose){
        json["pose"] = *event.pose;
    }
    return json;
}

std::string probe_json_line(
    WallClock time, const std::string& command, const std::string& output,
    const std::optional<std::string>& frame_file
){
    JsonObject json;
    json["time"] = lab_time_text(time);
    json["kind"] = "probe";
    json["text"] = command;
    json["output"] = output;
    if (frame_file){
        json["frame"] = *frame_file;
    }
    return JsonValue(std::move(json)).dump(-1);
}

void append_file(const std::string& path, const std::string& text){
    FileIO file;
    if (file.open(Filesystem::Path(path), FileMode::APPEND | FileMode::BINARY)){
        file.write(text);
    }
}

}



std::string lab_time_text(WallClock time){
#if _WIN32 && _MSC_VER
#pragma warning(disable:4996)
#endif
    using namespace std::chrono;
    const auto since_epoch = time.time_since_epoch();
    const auto millis = duration_cast<milliseconds>(since_epoch - duration_cast<seconds>(since_epoch)).count();
    const std::time_t tt = system_clock::to_time_t(time);
    const std::tm local = *std::localtime(&tt);
    return std::format(
        "{}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}",
        local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
        local.tm_hour, local.tm_min, local.tm_sec, millis
    );
}


std::string run_result_json(
    const RunRecord& record,
    const std::optional<std::string>& frame_file,
    const std::map<size_t, std::string>& event_frames
){
    JsonObject json;
    json["run_id"] = record.run_id;
    json["program"] = record.program;
    json["status"] = run_status_name(record.status);
    json["start_time"] = lab_time_text(record.start_time);
    if (record.end_time){
        json["end_time"] = lab_time_text(*record.end_time);
        json["duration_ms"] = std::chrono::duration_cast<Milliseconds>(*record.end_time - record.start_time).count();
    }
    if (record.result){
        json["outcome"] = run_outcome_name(record.result->outcome);
        if (record.result->error){
            json["error"] = *record.result->error;
        }
    }
    if (frame_file){
        json["frame"] = *frame_file;
    }
    json["event_count"] = record.events.size();
    json["dropped_events"] = record.dropped_events;
    json["log_lines"] = record.log_lines;
    json["dropped_log_lines"] = record.dropped_log_lines;
    if (!record.events.empty()){
        const size_t last = record.events.size() - 1;
        std::optional<std::string> last_frame;
        if (auto it = event_frames.find(last); it != event_frames.end()){
            last_frame = it->second;
        }
        json["last_event"] = event_json(record.events[last], last_frame);
    }
    return JsonValue(std::move(json)).dump(4) + "\n";
}

std::string run_meta_json(const RunRecord& record){
    JsonObject json;
    json["run_id"] = record.run_id;
    json["program"] = record.program;
    json["version"] = record.version;
    json["executable"] = record.executable;
    json["commit"] = PROGRAM_COMMIT;
    if (!record.settings.empty()){
        try{
            json["settings"] = parse_json(record.settings);
        }catch (std::exception&){
            json["settings"] = record.settings;
        }
    }
    return JsonValue(std::move(json)).dump(4) + "\n";
}

std::string run_event_json_line(const RunEvent& event, const std::optional<std::string>& frame_file){
    return JsonValue(event_json(event, frame_file)).dump(-1);
}

std::string event_frame_file(size_t index, const RunEvent& event){
    std::string name = std::format("frame-{:03}", index);
    if (event.checkpoint){
        name += "-" + file_name_part(*event.checkpoint);
    }
    if (event.step){
        name += "-s" + file_name_part(*event.step);
    }
    if (event.attempt){
        name += std::format("-a{}", *event.attempt);
    }
    return name + ".png";
}


void write_lab_file(const Filesystem::Path& path, const std::string& text){
    FileIO file;
    if (file.open(path, FileMode::WRITE | FileMode::BINARY)){
        file.write(text);
    }
}
void write_lab_file_atomically(const Filesystem::Path& path, const std::string& text){
    const Filesystem::Path temp = Filesystem::unique_temp_path(path);
    write_lab_file(temp, text);
    std::error_code ec;
    Filesystem::rename(temp, path, ec);
}



LabRunWriter::LabRunWriter(std::string folder, size_t keep)
    : m_folder(std::move(folder))
    , m_keep(std::max<size_t>(keep, 1))
{}

void LabRunWriter::sync(const RunRecordStore& store){
    const std::vector<std::string> ids = store.run_ids();
    for (const std::string& id : ids){
        Written& written = m_written[id];
        if (written.finished){
            continue;
        }
        const std::optional<RunRecord> record = store.get(id);
        if (!record){
            continue;
        }
        if (m_last_run < id){
            m_last_run = id;
            m_last_end.reset();
        }
        write_run(*record, written);
    }
    //  Forget the runs the store has dropped.
    for (auto it = m_written.begin(); it != m_written.end();){
        it = std::ranges::find(ids, it->first) == ids.end() ? m_written.erase(it) : std::next(it);
    }
}

void LabRunWriter::write_run(const RunRecord& record, Written& written){
    const std::string folder = run_folder(record.run_id);
    if (written.result_text.empty()){
        Filesystem::create_directories(Filesystem::Path(folder));
        prune();
        write_lab_file_atomically(Filesystem::Path(folder + "meta.json"), run_meta_json(record));
    }

    std::string lines;
    for (size_t i = written.events; i < record.events.size(); i++){
        const RunEvent& event = record.events[i];
        std::optional<std::string> frame_file;
        if (event.frame){
            frame_file = event_frame_file(i, event);
            if (event.frame->save(folder + *frame_file)){
                written.event_frames[i] = *frame_file;
            }else{
                frame_file.reset();
            }
        }
        lines += run_event_json_line(event, frame_file) + "\n";
    }
    if (!lines.empty()){
        append_file(folder + "events.jsonl", lines);
    }
    written.events = record.events.size();

    const std::optional<std::string> frame_file = save_result_frame(record, folder);
    append_log(record, written);

    const std::string text = run_result_json(record, frame_file, written.event_frames);
    if (text != written.result_text){
        write_lab_file_atomically(Filesystem::Path(folder + "result.json"), text);
        written.result_text = text;
    }
    if (record.status == RunStatus::TERMINAL){
        written.finished = true;
        if (record.run_id == m_last_run){
            m_last_end = record.end_time;
        }
    }
}

//  Appends the lines the store has that log.txt doesn't. Lines that left the
//  store's tail before this poll got to them are marked, not lost silently.
void LabRunWriter::append_log(const RunRecord& record, Written& written){
    const size_t first_held = record.log_lines - record.log_tail.size();
    std::string text;
    if (written.log_lines < first_held){
        text += std::format("[{} lines not copied: the writer fell behind]\n", first_held - written.log_lines);
        written.log_lines = first_held;
    }
    for (size_t i = written.log_lines - first_held; i < record.log_tail.size(); i++){
        text += record.log_tail[i] + "\n";
    }
    written.log_lines = record.log_lines;
    if (record.status == RunStatus::TERMINAL && record.dropped_log_lines > 0){
        text += std::format(
            "[log.txt stops at {} bytes: {} more lines are in SerialPrograms.log]\n",
            RunRecordStore::MAX_LOG_BYTES, record.dropped_log_lines
        );
    }
    if (!text.empty()){
        append_file(run_folder(record.run_id) + "log.txt", text);
    }
}

void LabRunWriter::prune(){
    std::vector<std::string> names;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(m_folder))){
        if (Filesystem::is_directory(path)){
            names.emplace_back(path.filename().string());
        }
    }
    std::ranges::sort(names);
    for (size_t i = 0; i + m_keep < names.size(); i++){
        Filesystem::remove_all(Filesystem::Path(m_folder + names[i]));
        m_written.erase(names[i]);
    }
}

std::optional<std::string> LabRunWriter::run_for_probe(WallClock now) const{
    if (m_last_run.empty()){
        return std::nullopt;
    }
    if (m_last_end && now - *m_last_end > PROBE_WINDOW){
        return std::nullopt;
    }
    return m_last_run;
}

void LabRunWriter::attach_probe(const std::string& run_id, const std::string& command, const std::string& output){
    const std::string folder = run_folder(run_id);
    if (!Filesystem::is_directory(Filesystem::Path(folder))){
        return;
    }
    std::optional<std::string> frame_file;
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)){
        if (line.rfind("shot ", 0) != 0){
            continue;
        }
        const Filesystem::Path source(line.substr(5));
        const std::string copy = "probe-" + source.filename().string();
        try{
            Filesystem::remove(Filesystem::Path(folder + copy));
            if (Filesystem::copy_file(source, Filesystem::Path(folder + copy)) && !frame_file){
                frame_file = copy;
            }
        }catch (std::exception&){
            //  A probe that printed a path it did not save: nothing to copy.
        }
    }
    append_file(folder + "events.jsonl", probe_json_line(current_time(), command, output, frame_file) + "\n");
}



}

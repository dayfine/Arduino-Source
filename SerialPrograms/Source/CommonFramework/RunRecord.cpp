/*  Run Record
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <ranges>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "RunRecord.h"

namespace PokemonAutomation{



std::string run_status_name(RunStatus status){
    switch (status){
    case RunStatus::RUNNING:
        return "running";
    case RunStatus::STOPPING:
        return "stopping";
    case RunStatus::TERMINAL:
        return "terminal";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown run status.");
}
std::string run_outcome_name(RunOutcome outcome){
    switch (outcome){
    case RunOutcome::COMPLETED:
        return "completed";
    case RunOutcome::CANCELLED:
        return "cancelled";
    case RunOutcome::FAILED:
        return "failed";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown run outcome.");
}



RunRecordStore& RunRecordStore::instance(){
    static RunRecordStore store;
    return store;
}

RunRecordStore::RunRecordStore(size_t capacity)
    //  The app's start time, to the microsecond: two instances started in
    //  the same second still get different IDs.
    : m_app_token(now_to_filestring())
    , m_capacity(std::max<size_t>(capacity, 1))
{}

std::string RunRecordStore::begin(std::string program){
    std::lock_guard<Mutex> lg(m_lock);
    while (m_records.size() >= m_capacity){
        m_records.pop_front();
    }
    RunRecord& record = m_records.emplace_back();
    record.run_id = std::format("{}-{:03}", m_app_token, ++m_counter);
    record.program = std::move(program);
    record.start_time = current_time();
    return record.run_id;
}

void RunRecordStore::stopping(const std::string& run_id){
    std::lock_guard<Mutex> lg(m_lock);
    RunRecord* record = find(run_id);
    if (record != nullptr && record->status == RunStatus::RUNNING){
        record->status = RunStatus::STOPPING;
    }
}

void RunRecordStore::event(const std::string& run_id, RunEvent event){
    std::lock_guard<Mutex> lg(m_lock);
    RunRecord* record = find(run_id);
    if (record == nullptr){
        return;
    }
    if (record->events.size() >= MAX_EVENTS){
        record->dropped_events++;
        return;
    }
    if (event.time == WallClock::min()){
        event.time = current_time();
    }
    record->events.emplace_back(std::move(event));

    //  Keep the frames of the newest events only.
    size_t frames = 0;
    for (RunEvent& e : std::ranges::reverse_view(record->events)){
        if (e.frame == nullptr){
            continue;
        }
        if (++frames > MAX_EVENT_FRAMES){
            e.frame.reset();
        }
    }
}

void RunRecordStore::log_line(const std::string& run_id, const std::string& text){
    std::lock_guard<Mutex> lg(m_lock);
    RunRecord* record = find(run_id);
    if (record == nullptr){
        return;
    }
    record->log_tail.emplace_back(text);
    size_t bytes = 0;
    for (const std::string& line : record->log_tail){
        bytes += line.size() + 1;
    }
    size_t drop = 0;
    while (bytes > MAX_LOG_BYTES && drop + 1 < record->log_tail.size()){
        bytes -= record->log_tail[drop].size() + 1;
        drop++;
    }
    if (drop > 0){
        record->log_tail.erase(record->log_tail.begin(), record->log_tail.begin() + drop);
        record->log_truncated = true;
    }
}

bool RunRecordStore::finish(const std::string& run_id, RunResult result){
    std::lock_guard<Mutex> lg(m_lock);
    RunRecord* record = find(run_id);
    if (record == nullptr || record->status == RunStatus::TERMINAL){
        return false;
    }
    record->status = RunStatus::TERMINAL;
    record->end_time = current_time();
    record->result = std::move(result);

    //  Frames stay with the newest terminal runs; older ones keep the rest
    //  of their record.
    size_t terminal = 0;
    for (RunRecord& r : std::ranges::reverse_view(m_records)){
        if (r.status == RunStatus::TERMINAL && ++terminal > RUNS_WITH_FRAMES){
            release_frames(r);
        }
    }
    return true;
}

std::vector<std::string> RunRecordStore::run_ids() const{
    std::lock_guard<Mutex> lg(m_lock);
    std::vector<std::string> ids;
    for (const RunRecord& record : m_records){
        ids.emplace_back(record.run_id);
    }
    return ids;
}

std::optional<RunRecord> RunRecordStore::get(const std::string& run_id) const{
    std::lock_guard<Mutex> lg(m_lock);
    for (const RunRecord& record : m_records){
        if (record.run_id == run_id){
            return record;
        }
    }
    return std::nullopt;
}

RunRecord* RunRecordStore::find(const std::string& run_id){
    auto it = std::ranges::find_if(m_records, [&](const RunRecord& r){ return r.run_id == run_id; });
    return it == m_records.end() ? nullptr : &*it;
}

void RunRecordStore::release_frames(RunRecord& record){
    for (RunEvent& event : record.events){
        event.frame.reset();
    }
    if (record.result){
        record.result->frame.reset();
    }
}



SessionRunRecorder::SessionRunRecorder(RunRecordStore& store, Logger& base_logger)
    : m_store(store)
    , m_base_logger(base_logger)
{}

std::string SessionRunRecorder::begin(std::string program){
    std::lock_guard<Mutex> lg(m_lock);
    m_run_id = m_store.begin(std::move(program));
    m_active = true;
    return m_run_id;
}
void SessionRunRecorder::stopping(){
    std::lock_guard<Mutex> lg(m_lock);
    if (m_active){
        m_store.stopping(m_run_id);
    }
}
void SessionRunRecorder::finish(RunResult result){
    std::lock_guard<Mutex> lg(m_lock);
    if (!m_active){
        return;
    }
    m_active = false;
    m_store.finish(m_run_id, std::move(result));
}

std::string SessionRunRecorder::last_run_id() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_run_id;
}

void SessionRunRecorder::event(RunEvent event){
    std::lock_guard<Mutex> lg(m_lock);
    if (m_active){
        m_store.event(m_run_id, std::move(event));
    }
}
void SessionRunRecorder::log(const std::string& msg, Color color){
    m_base_logger.log(msg, color);
    std::lock_guard<Mutex> lg(m_lock);
    if (m_active){
        m_store.log_line(m_run_id, msg);
    }
}



}

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
#include "CommonFramework/BuildCommit.h"
#include "CommonFramework/Globals.h"
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



namespace{
std::atomic<bool>& app_run_records_enabled(){
    static std::atomic<bool> enabled{false};
    return enabled;
}
}

RunRecordStore& app_run_records(){
    static RunRecordStore store(RunRecordStore::DEFAULT_CAPACITY, &app_run_records_enabled());
    return store;
}
void enable_app_run_records(){
    app_run_records_enabled().store(true, std::memory_order_release);
}

RunRecordStore::RunRecordStore(size_t capacity, const std::atomic<bool>* enabled)
    //  The app's start time, to the microsecond: two instances started in
    //  the same second still get different IDs.
    : m_app_token(now_to_filestring())
    , m_capacity(std::max<size_t>(capacity, 1))
    , m_enabled(enabled)
{}

std::string RunRecordStore::begin(std::string program, std::string settings){
    if (m_enabled != nullptr && !m_enabled->load(std::memory_order_acquire)){
        return "";
    }
    std::lock_guard<Mutex> lg(m_lock);
    while (m_records.size() >= m_capacity){
        m_records.pop_front();
    }
    RunRecord& record = m_records.emplace_back();
    record.run_id = std::format("{}-{:03}", m_app_token, ++m_counter);
    record.program = std::move(program);
    record.version = PROGRAM_VERSION;
    record.commit = PROGRAM_COMMIT;
    record.settings = std::move(settings);
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
    const size_t bytes = text.size() + 1;
    if (record->log_bytes + bytes > MAX_LOG_BYTES){
        record->dropped_log_lines++;
        return;
    }
    record->log_lines++;
    record->log_bytes += bytes;
    record->log_tail.emplace_back(text);
    record->log_tail_bytes += bytes;
    while (record->log_tail_bytes > MAX_LOG_TAIL_BYTES && record->log_tail.size() > 1){
        record->log_tail_bytes -= record->log_tail.front().size() + 1;
        record->log_tail.pop_front();
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

std::string SessionRunRecorder::begin(std::string program, std::string settings){
    std::lock_guard<Mutex> lg(m_lock);
    m_run_id = m_store.begin(std::move(program), std::move(settings));
    m_active = !m_run_id.empty();
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

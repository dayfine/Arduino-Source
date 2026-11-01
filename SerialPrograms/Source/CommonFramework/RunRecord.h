/*  Run Record
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one run of a program did, as the framework saw it: when it started,
 *  how it ended (exactly one outcome per accepted run), the error and the
 *  frame it died with, the events the program reported on the way, and the
 *  tail of its log. ProgramSession writes these at its execution boundary;
 *  RunRecordStore keeps the last few for whoever asks, such as the Lab
 *  channel, which writes them to files (Windows/Lab.md, "Run records").
 *  Nothing here depends on the UI, on JSON or on a game.
 *
 */

#ifndef PokemonAutomation_CommonFramework_RunRecord_H
#define PokemonAutomation_CommonFramework_RunRecord_H

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"

namespace PokemonAutomation{


enum class RunStatus{
    RUNNING,
    STOPPING,   //  A stop was requested; the outcome is not known yet.
    TERMINAL,
};
enum class RunOutcome{
    COMPLETED,  //  The program returned or threw ProgramFinishedException.
    CANCELLED,  //  The program ended through the stop path.
    FAILED,     //  A setup, connection or execution error ended it.
};
std::string run_status_name(RunStatus status);      //  "running", "stopping", "terminal"
std::string run_outcome_name(RunOutcome outcome);   //  "completed", "cancelled", "failed"


//  Something the program says about its own progress: a checkpoint reached,
//  a step started, an attempt that failed, a pose. Every field but `kind`
//  and `text` is optional; a game fills the ones it has. (The `= {}` let a
//  designated initializer leave fields out on every compiler.)
struct RunEvent{
    WallClock time = WallClock::min();  //  Set by the recorder when left at min().
    std::string kind = {};              //  e.g. "checkpoint", "step", "attempt", "error"
    std::optional<std::string> checkpoint = {};
    std::optional<std::string> step = {};
    std::optional<size_t> attempt = {};
    std::string text = {};
    std::shared_ptr<const ImageRGB32> frame = {};   //  The screen at that moment, if the program had it.
    std::optional<std::string> pose = {};           //  Where the player is, as the program puts it.
};

//  How a run ended.
struct RunResult{
    RunOutcome outcome = RunOutcome::FAILED;
    std::optional<std::string> error = {};          //  The primary error of a failed run.
    std::shared_ptr<const ImageRGB32> frame = {};   //  The frame the exception carried, if any.
};

struct RunRecord{
    std::string run_id = {};
    std::string program = {};           //  The program's identifier, "<game>:<program>".
    WallClock start_time = WallClock::min();
    std::optional<WallClock> end_time = {};
    RunStatus status = RunStatus::RUNNING;
    std::optional<RunResult> result = {};   //  Set once, when the status turns TERMINAL.
    std::vector<RunEvent> events = {};
    size_t dropped_events = 0;          //  Events past RunRecordStore::MAX_EVENTS.
    std::vector<std::string> log_tail = {};     //  The run's last log lines, oldest first.
    bool log_truncated = false;         //  Whether older lines were dropped from the tail.
};


//  The program's progress hook, reachable as ProgramEnvironment::run_recorder().
class RunRecorder{
public:
    virtual ~RunRecorder() = default;
    virtual void event(RunEvent event) = 0;
};


//  The last few runs of this app instance. Thread-safe; every call is short
//  and copies, so a reader never holds a lock while writing files.
//
//  Run IDs are opaque strings unique to this app instance: a restart cannot
//  resolve an old ID to a new run. They sort in the order the runs started.
//
//  Memory: at most MAX_EVENTS events per run; the frames of a run's newest
//  MAX_EVENT_FRAMES events; all frames only for the active run and the
//  newest RUNS_WITH_FRAMES terminal runs; a log tail of MAX_LOG_BYTES.
//  A consumer that wants a frame reads it soon after the event or the end.
class RunRecordStore{
public:
    static constexpr size_t DEFAULT_CAPACITY = 32;
    static constexpr size_t MAX_EVENTS = 4096;
    static constexpr size_t MAX_EVENT_FRAMES = 8;
    static constexpr size_t RUNS_WITH_FRAMES = 4;
    static constexpr size_t MAX_LOG_BYTES = 64 * 1024;

    static RunRecordStore& instance();

    explicit RunRecordStore(size_t capacity = DEFAULT_CAPACITY);
    RunRecordStore(const RunRecordStore&) = delete;
    void operator=(const RunRecordStore&) = delete;

    //  Opens a record for an accepted start and returns its run ID. The
    //  oldest record goes when the store is full.
    std::string begin(std::string program);

    //  Marks a running run as stopping. No effect once it is terminal.
    void stopping(const std::string& run_id);

    //  Appends to the run. Both are dropped for an unknown run.
    void event(const std::string& run_id, RunEvent event);
    void log_line(const std::string& run_id, const std::string& text);

    //  Closes the run with its one result. Returns false, and changes
    //  nothing, when the run is unknown or already terminal.
    bool finish(const std::string& run_id, RunResult result);

    //  The IDs of the records held, oldest first.
    std::vector<std::string> run_ids() const;

    //  A copy of the record, or nothing for an unknown run.
    std::optional<RunRecord> get(const std::string& run_id) const;


private:
    RunRecord* find(const std::string& run_id);
    void release_frames(RunRecord& record);

    mutable Mutex m_lock;
    const std::string m_app_token;
    const size_t m_capacity;
    uint64_t m_counter = 0;
    std::deque<RunRecord> m_records;
};


//  One session's end of the store: a RunRecorder for the run active now and
//  a Logger that copies what passes through it into that run's log tail.
//  The session's loggers chain off `log()`, so a run's tail holds the lines
//  the program and its consoles logged, and nothing from other sessions.
class SessionRunRecorder : public RunRecorder, public Logger{
public:
    SessionRunRecorder(RunRecordStore& store, Logger& base_logger);

    //  Opens a run and makes it the active one. Returns its ID.
    std::string begin(std::string program);
    void stopping();
    //  Closes the active run; nothing happens when none is active.
    void finish(RunResult result);

    //  The run the last begin() opened, active or finished; "" before the first.
    std::string last_run_id() const;

    virtual void event(RunEvent event) override;
    virtual void log(const std::string& msg, Color color = Color()) override;

private:
    RunRecordStore& m_store;
    Logger& m_base_logger;
    mutable Mutex m_lock;
    std::string m_run_id;
    bool m_active = false;
};



}
#endif

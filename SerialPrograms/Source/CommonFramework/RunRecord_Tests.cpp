/*  Run Record - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The store on its own, and the run lifecycle through a ProgramSession
 *  whose program is a scripted fake: no console, no window.
 *
 */

#include <atomic>
#include <deque>
#include <format>
#include <functional>
#include <stdexcept>
#include <thread>
#include <QCoreApplication>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/Panels/PanelSession.h"
#include "CommonFramework/Panels/ProgramDescriptor.h"
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "RunRecord.h"
#include "RunRecord_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


std::shared_ptr<const ImageRGB32> make_frame(size_t width){
    ImageRGB32 image(width, width);
    image.fill(0xff000000);
    return std::make_shared<const ImageRGB32>(std::move(image));
}

//  Polls `done` every few milliseconds until it holds or `timeout` passes.
bool wait_until(const std::function<bool()>& done, Milliseconds timeout = 5000ms){
    const WallClock deadline = current_time() + timeout;
    while (!done()){
        if (current_time() > deadline){
            return false;
        }
        std::this_thread::sleep_for(5ms);
    }
    return true;
}



//  The run the store opened last, "" when none.
std::string newest_run(const RunRecordStore& store){
    const std::vector<std::string> ids = store.run_ids();
    return ids.empty() ? "" : ids.back();
}

UnitTestResult test_store_ids(){
    RunRecordStore store(2);
    const std::string a = store.begin("Test:A", R"({"START": 3})");
    const std::string b = store.begin("Test:B");
    if (a.empty() || a == b || !(a < b)){
        return std::format("Run IDs should be distinct and sort in start order: <{}>, <{}>", a, b);
    }
    if (store.run_ids() != std::vector<std::string>{a, b}){
        return "run_ids() lists the records oldest first.";
    }
    const std::optional<RunRecord> record = store.get(a);
    if (!record || record->program != "Test:A" || record->status != RunStatus::RUNNING || record->result){
        return "A new record is running, with its program and no result.";
    }
    if (record->start_time == WallClock::min() || record->end_time){
        return "A new record has a start time and no end time.";
    }
    if (record->version != PROGRAM_VERSION || (QCoreApplication::instance() != nullptr && record->executable.empty())){
        return std::format("A record names the app: version <{}>, executable <{}>.", record->version, record->executable);
    }
    if (record->settings != R"({"START": 3})" || !store.get(b)->settings.empty()){
        return "A record keeps the settings it was begun with.";
    }
    const std::string c = store.begin("Test:C");
    if (store.get(a) || store.run_ids() != std::vector<std::string>{b, c}){
        return "A full store drops its oldest record.";
    }
    if (store.get("nobody")){
        return "An unknown run has no record.";
    }
    return true;
}

UnitTestResult test_store_one_result(){
    RunRecordStore store;
    const std::string id = store.begin("Test:A");
    store.stopping(id);
    if (store.get(id)->status != RunStatus::STOPPING){
        return "stopping() marks a running run.";
    }
    if (!store.finish(id, {.outcome = RunOutcome::FAILED, .error = "first"})){
        return "The first finish() takes.";
    }
    store.stopping(id);
    if (store.finish(id, {.outcome = RunOutcome::COMPLETED})){
        return "A second finish() is refused.";
    }
    const RunRecord record = *store.get(id);
    if (record.status != RunStatus::TERMINAL || !record.end_time || !record.result){
        return "A finished record is terminal with an end time and a result.";
    }
    if (record.result->outcome != RunOutcome::FAILED || record.result->error != "first"){
        return std::format("The first result stands: got {}", run_outcome_name(record.result->outcome));
    }
    if (store.finish("nobody", {})){
        return "Finishing an unknown run is refused.";
    }
    return true;
}

UnitTestResult test_store_events(){
    RunRecordStore store;
    const std::string id = store.begin("Test:A");
    store.event(id, {.kind = "checkpoint", .checkpoint = "00.01", .text = "ok"});
    store.event("nobody", {.kind = "checkpoint"});
    for (size_t c = 0; c < RunRecordStore::MAX_EVENT_FRAMES + 2; c++){
        store.event(id, {.kind = "step", .step = std::to_string(c), .frame = make_frame(4)});
    }
    RunRecord record = *store.get(id);
    if (record.events.size() != RunRecordStore::MAX_EVENT_FRAMES + 3){
        return std::format("Expected {} events, got {}.", RunRecordStore::MAX_EVENT_FRAMES + 3, record.events.size());
    }
    if (record.events[0].time == WallClock::min() || record.events[0].checkpoint != "00.01"){
        return "An event gets the time it was recorded and keeps its fields.";
    }
    if (record.events[1].frame || record.events[2].frame || !record.events[3].frame || !record.events.back().frame){
        return "Only the newest MAX_EVENT_FRAMES events keep their frames.";
    }

    for (size_t c = record.events.size(); c < RunRecordStore::MAX_EVENTS + 5; c++){
        store.event(id, {.kind = "step"});
    }
    record = *store.get(id);
    if (record.events.size() != RunRecordStore::MAX_EVENTS || record.dropped_events != 5){
        return std::format("Events past the cap are counted, not kept: {} events, {} dropped.", record.events.size(), record.dropped_events);
    }
    return true;
}

UnitTestResult test_store_log(){
    RunRecordStore store;
    const std::string id = store.begin("Test:A");
    RunRecord record;

    //  Lines of 1000 bytes, numbered: 300 are more than the tail holds.
    const std::string line(995, 'x');
    for (size_t c = 0; c < 300; c++){
        store.log_line(id, std::format("{:04} {}", c, line));
    }
    record = *store.get(id);
    size_t bytes = 0;
    for (const std::string& l : record.log_tail){
        bytes += l.size() + 1;
    }
    if (record.log_lines != 300 || record.log_bytes != 300 * 1001 || record.dropped_log_lines != 0){
        return std::format("Every line is counted: {} lines, {} bytes.", record.log_lines, record.log_bytes);
    }
    if (bytes != record.log_tail_bytes || bytes > RunRecordStore::MAX_LOG_TAIL_BYTES || record.log_tail.size() == 300){
        return std::format("The tail keeps at most {} bytes: {} lines, {} bytes.", RunRecordStore::MAX_LOG_TAIL_BYTES, record.log_tail.size(), bytes);
    }
    const size_t first = record.log_lines - record.log_tail.size();
    if (record.log_tail.front().rfind(std::format("{:04} ", first), 0) != 0 || record.log_tail.back().rfind("0299 ", 0) != 0){
        return "The tail holds the newest lines, from line log_lines - log_tail.size().";
    }

    //  Past MAX_LOG_BYTES in all, lines are counted and not kept.
    const size_t fit = RunRecordStore::MAX_LOG_BYTES / 1001;
    for (size_t c = 300; c < fit + 10; c++){
        store.log_line(id, std::format("{:04} {}", c % 10000, line));
    }
    record = *store.get(id);
    if (record.log_lines != fit || record.dropped_log_lines != 10 || record.log_bytes > RunRecordStore::MAX_LOG_BYTES){
        return std::format("A run records at most {} bytes of log: {} lines, {} dropped.", RunRecordStore::MAX_LOG_BYTES, record.log_lines, record.dropped_log_lines);
    }
    return true;
}

UnitTestResult test_store_disabled(){
    struct CountingLogger : public Logger{
        size_t lines = 0;
        virtual void log(const std::string&, Color) override{ lines++; }
    };
    std::atomic<bool> enabled{false};
    RunRecordStore store(4, &enabled);
    if (!store.begin("Test:A").empty() || !store.run_ids().empty()){
        return "A disabled store opens no record.";
    }
    CountingLogger base;
    SessionRunRecorder recorder(store, base);
    if (!recorder.begin("Test:A").empty() || !recorder.last_run_id().empty()){
        return "A recorder over a disabled store has no run.";
    }
    recorder.log("line");
    recorder.event({.kind = "checkpoint"});
    recorder.finish({.outcome = RunOutcome::COMPLETED});
    if (base.lines != 1 || !store.run_ids().empty()){
        return "Lines still reach the base logger; nothing is recorded.";
    }
    enabled.store(true);
    const std::string id = recorder.begin("Test:A");
    recorder.log("line");
    if (id.empty() || store.get(id)->log_lines != 1){
        return "Once enabled, the store records.";
    }
    return true;
}

UnitTestResult test_store_releases_old_frames(){
    RunRecordStore store;
    std::vector<std::string> ids;
    for (size_t c = 0; c < RunRecordStore::RUNS_WITH_FRAMES + 1; c++){
        ids.emplace_back(store.begin("Test:A"));
        store.event(ids.back(), {.kind = "step", .frame = make_frame(4)});
        store.finish(ids.back(), {.outcome = RunOutcome::FAILED, .error = "e", .frame = make_frame(4)});
    }
    const RunRecord oldest = *store.get(ids.front());
    if (oldest.events[0].frame || oldest.result->frame){
        return "A run older than the newest RUNS_WITH_FRAMES terminal runs keeps no frames.";
    }
    if (oldest.result->error != "e" || oldest.events.size() != 1){
        return "It keeps the rest of its record.";
    }
    const RunRecord newest = *store.get(ids.back());
    if (!newest.events[0].frame || !newest.result->frame){
        return "The newest terminal runs keep their frames.";
    }
    return true;
}

UnitTestResult test_session_recorder(){
    struct CountingLogger : public Logger{
        size_t lines = 0;
        virtual void log(const std::string&, Color) override{ lines++; }
    };
    RunRecordStore store;
    CountingLogger base;
    SessionRunRecorder recorder(store, base);
    recorder.log("before");
    recorder.event({.kind = "lost"});
    if (!recorder.last_run_id().empty() || !store.run_ids().empty()){
        return "Nothing is recorded before a run begins.";
    }
    const std::string id = recorder.begin("Test:A");
    if (recorder.last_run_id() != id){
        return "last_run_id() is the run begin() opened.";
    }
    recorder.log("during");
    recorder.event({.kind = "checkpoint"});
    recorder.finish({.outcome = RunOutcome::COMPLETED});
    recorder.log("after");
    recorder.event({.kind = "late"});
    recorder.finish({.outcome = RunOutcome::FAILED});
    const RunRecord record = *store.get(id);
    if (base.lines != 3){
        return std::format("Every line reaches the base logger: {} of 3.", base.lines);
    }
    if (record.log_tail != std::deque<std::string>{"during"} || record.events.size() != 1){
        return "Only lines and events during the run are recorded.";
    }
    if (record.result->outcome != RunOutcome::COMPLETED || recorder.last_run_id() != id){
        return "The result stands and the run stays known after finish().";
    }
    return true;
}



//  A program with no console: what it does is scripted per test.
struct FakeScript{
    std::string validity = {};      //  check_validity(): "" accepts the start.
    bool fail_dispatch = false;     //  The program thread cannot be started.
    std::function<void(ProgramEnvironment& env)> start_checks = {};
    std::function<void(ProgramSession& program, ProgramEnvironment& env, CancellableScope& scope)> body = {};
    std::string settings = {};      //  settings_json().
};

class FakeDescriptor : public ProgramDescriptor{
public:
    FakeDescriptor()
        : ProgramDescriptor(
            "Test:RunRecordFake", "Test", "Run Record Fake",
            "", "", PanelQualityStanding::DEVELOPER, false, {}
        )
    {}
    virtual std::unique_ptr<PanelSession> make_panel() const override{ return nullptr; }
};

class FakeInstance : public ProgramInstance{
public:
    explicit FakeInstance(std::function<void(ProgramEnvironment& env)> start_checks)
        : m_start_checks(std::move(start_checks))
    {}
    virtual void run_start_program_checks(const ProgramDescriptor&, ProgramEnvironment& env) override{
        if (m_start_checks){
            m_start_checks(env);
        }
    }
private:
    std::function<void(ProgramEnvironment& env)> m_start_checks;
};

class FakeProgram : public ProgramSession{
public:
    FakeProgram(const FakeDescriptor& descriptor, RunRecordStore& store, FakeScript script)
        : ProgramSession(descriptor, store)
        , m_script(std::move(script))
    {
        m_instance = std::make_unique<FakeInstance>(m_script.start_checks);
    }
    virtual ~FakeProgram(){
        internal_stop_program();
        join_program_thread();
    }

    //  Whether the body has started running.
    bool running() const{ return m_running.load(std::memory_order_acquire); }

    //  Waits for the program thread to end, so the run's record is closed.
    bool wait_until_stopped(){
        if (!wait_until([this]{ return current_state() == ProgramState::STOPPED; })){
            return false;
        }
        join_program_thread();
        return true;
    }

    virtual std::string check_validity() const override{ return m_script.validity; }

protected:
    virtual std::unique_ptr<ProgramEnvironment> make_env(const ProgramInfo& program_info) override{
        return std::make_unique<ProgramEnvironment>(program_info, *this, current_stats_tracker(), historical_stats_tracker());
    }
    virtual void internal_run_program(ProgramEnvironment& env) override{
        m_running.store(true, std::memory_order_release);
        if (m_script.body){
            m_script.body(*this, env, *m_scope);
        }
    }
    virtual std::string settings_json() const override{ return m_script.settings; }
    virtual AsyncTask dispatch_program_thread(std::function<void()> body) override{
        if (m_script.fail_dispatch){
            throw std::runtime_error("no thread");
        }
        return ProgramSession::dispatch_program_thread(std::move(body));
    }

private:
    FakeScript m_script;
    std::atomic<bool> m_running{false};
};

//  Starts the program, waits for it to end, and returns its record.
UnitTestResult run_to_end(FakeProgram& program, RunRecordStore& store, RunRecord& record){
    const std::string error = program.start_program();
    if (!error.empty()){
        return "The start was refused: " + error;
    }
    if (!program.wait_until_stopped()){
        return "The program did not stop.";
    }
    const std::optional<RunRecord> found = store.get(newest_run(store));
    if (!found){
        return "The run has no record.";
    }
    record = *found;
    if (record.status != RunStatus::TERMINAL || !record.result){
        return "The record is not terminal after the program stopped.";
    }
    return true;
}

UnitTestResult test_rejection(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {.validity = "Pick a save."});
    if (program.start_program() != "Pick a save."){
        return "A start refused by check_validity() returns its message.";
    }
    if (!store.run_ids().empty() || !newest_run(store).empty()){
        return "A refused start opens no run.";
    }
    return true;
}

UnitTestResult test_immediate_completion(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {
        .body = [](ProgramSession&, ProgramEnvironment& env, CancellableScope&){
            record_run_event(env, {.kind = "checkpoint", .checkpoint = "00.01", .step = "3/3", .attempt = 1, .text = "ok", .pose = "home (2, 4) N"});
        },
        .settings = R"({"START": 1})",
    });
    RunRecord record;
    UnitTestResult result = run_to_end(program, store, record);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    if (record.result->outcome != RunOutcome::COMPLETED || record.result->error || record.result->frame){
        return "A program that returns completed its run, with no error and no frame.";
    }
    if (record.program != "Test:RunRecordFake" || record.start_time > *record.end_time){
        return "The record names the program and its times.";
    }
    if (record.settings != R"({"START": 1})" || record.version != PROGRAM_VERSION){
        return "The record has the session's settings and the app's version.";
    }
    if (record.events.size() != 1 || record.events[0].checkpoint != "00.01" || record.events[0].attempt != size_t(1) || record.events[0].pose != "home (2, 4) N"){
        return std::format("The program's event is in the record: {} events.", record.events.size());
    }
    bool stopped_logged = false;
    for (const std::string& line : record.log_tail){
        stopped_logged |= line.find("Program State: STOPPED") != std::string::npos;
    }
    if (!stopped_logged){
        return "The run's log tail runs through the session's last state change.";
    }
    return true;
}

UnitTestResult test_setup_failure(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {
        .start_checks = [](ProgramEnvironment& env){
            throw UserSetupError(env.logger(), "The controller is not ready.");
        },
    });
    RunRecord record;
    UnitTestResult result = run_to_end(program, store, record);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    if (record.result->outcome != RunOutcome::FAILED || record.result->error != "The controller is not ready."){
        return "A setup error fails the run with its message.";
    }
    if (record.events.size() != 1 || record.events[0].kind != "error" || record.events[0].text != "The controller is not ready."){
        return "The reported error is an \"error\" event.";
    }
    return true;
}

UnitTestResult test_failure_with_frame(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {
        .body = [](ProgramSession&, ProgramEnvironment&, CancellableScope&){
            ImageRGB32 screen(8, 8);
            screen.fill(0xffff0000);
            throw OperationFailedExceptionWithScreenshot(
                ErrorReportMode::NO_ERROR_REPORT, "The sign's box did not open.", nullptr, std::move(screen)
            );
        },
    });
    RunRecord record;
    UnitTestResult result = run_to_end(program, store, record);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    if (record.result->outcome != RunOutcome::FAILED || record.result->error != "The sign's box did not open."){
        return "The exception's message is the primary error.";
    }
    if (!record.result->frame || record.result->frame->width() != 8){
        return "The exception's frame is kept with the result.";
    }
    return true;
}

UnitTestResult test_cancellation(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {
        .body = [](ProgramSession&, ProgramEnvironment&, CancellableScope& scope){
            while (true){
                scope.throw_if_cancelled();
                std::this_thread::sleep_for(5ms);
            }
        },
    });
    if (!program.start_program().empty() || !wait_until([&]{ return program.running(); })){
        return "The program should start and run.";
    }
    const std::string id = newest_run(store);
    if (!program.stop_program().empty() || !program.wait_until_stopped()){
        return "The program should stop.";
    }
    const RunRecord record = *store.get(id);
    if (record.status != RunStatus::TERMINAL || record.result->outcome != RunOutcome::CANCELLED || record.result->error){
        return "A stopped program's run is cancelled, with no error.";
    }
    return true;
}

//  The body waits for the stop request and then ends its own way.
UnitTestResult run_racing_a_stop(const std::function<void()>& ending, RunOutcome expected, const char* what){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {
        .body = [&](ProgramSession& self, ProgramEnvironment&, CancellableScope&){
            wait_until([&]{ return self.current_state() == ProgramState::STOPPING; });
            ending();
        },
    });
    if (!program.start_program().empty() || !wait_until([&]{ return program.running(); })){
        return "The program should start and run.";
    }
    const std::string id = newest_run(store);
    program.stop_program();
    if (!program.wait_until_stopped()){
        return "The program should stop.";
    }
    const RunRecord record = *store.get(id);
    if (record.result->outcome != expected){
        return std::format("{}: expected {}, got {}.", what, run_outcome_name(expected), run_outcome_name(record.result->outcome));
    }
    return true;
}
UnitTestResult test_late_stop(){
    UnitTestResult result = run_racing_a_stop(
        []{ throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "Failed anyway."); },
        RunOutcome::FAILED, "A failure observed after the stop request stays a failure"
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    return run_racing_a_stop(
        []{}, RunOutcome::COMPLETED, "A normal return racing the stop request stays completed"
    );
}

UnitTestResult test_dispatch_failure(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram program(descriptor, store, {.fail_dispatch = true});
    const std::string error = program.start_program();
    if (error.find("could not be started") == std::string::npos){
        return "A start whose thread cannot be dispatched returns an error: <" + error + ">";
    }
    const std::optional<RunRecord> record = store.get(newest_run(store));
    if (!record || record->status != RunStatus::TERMINAL || record->result->outcome != RunOutcome::FAILED){
        return "The accepted run is closed as failed.";
    }
    if (record->result->error != error){
        return "The run's error is the start's error.";
    }
    if (program.current_state() != ProgramState::STOPPED){
        return "The session is stopped again, so the next start can try.";
    }
    return true;
}

UnitTestResult test_second_run_preserves_first(){
    RunRecordStore store;
    FakeDescriptor descriptor;
    FakeProgram failing(descriptor, store, {
        .body = [](ProgramSession&, ProgramEnvironment&, CancellableScope&){
            throw OperationFailedExceptionWithScreenshot(
                ErrorReportMode::NO_ERROR_REPORT, "first", nullptr, ImageRGB32(8, 8)
            );
        },
    });
    RunRecord first;
    UnitTestResult result = run_to_end(failing, store, first);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    FakeProgram completing(descriptor, store, {});
    RunRecord second;
    result = run_to_end(completing, store, second);
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    if (first.run_id == second.run_id || !(first.run_id < second.run_id)){
        return "Each run has its own ID, sorting after the earlier one.";
    }
    const std::optional<RunRecord> kept = store.get(first.run_id);
    if (!kept || kept->result->outcome != RunOutcome::FAILED || kept->result->error != "first" || !kept->result->frame){
        return "The first run's result and frame are still there after the second run.";
    }
    if (second.result->outcome != RunOutcome::COMPLETED || store.run_ids() != std::vector<std::string>{first.run_id, second.run_id}){
        return "The second run has its own, completed, record.";
    }
    return true;
}


class Test_RunRecord : public UnitTest{
public:
    Test_RunRecord(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("CommonFramework::RunRecord - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_RunRecord(UnitTestDatabase& database){
    database.add<Test_RunRecord>("store: ids and capacity", test_store_ids);
    database.add<Test_RunRecord>("store: one result per run", test_store_one_result);
    database.add<Test_RunRecord>("store: events and frames", test_store_events);
    database.add<Test_RunRecord>("store: numbered log and its cap", test_store_log);
    database.add<Test_RunRecord>("store: old runs release frames", test_store_releases_old_frames);
    database.add<Test_RunRecord>("store: disabled", test_store_disabled);
    database.add<Test_RunRecord>("session recorder", test_session_recorder);
    database.add<Test_RunRecord>("lifecycle: rejected start", test_rejection);
    database.add<Test_RunRecord>("lifecycle: immediate completion", test_immediate_completion);
    database.add<Test_RunRecord>("lifecycle: setup failure", test_setup_failure);
    database.add<Test_RunRecord>("lifecycle: failure with frame", test_failure_with_frame);
    database.add<Test_RunRecord>("lifecycle: cancellation", test_cancellation);
    database.add<Test_RunRecord>("lifecycle: late stop", test_late_stop);
    database.add<Test_RunRecord>("lifecycle: dispatch failure", test_dispatch_failure);
    database.add<Test_RunRecord>("lifecycle: second run preserves the first", test_second_run_preserves_first);
}



}

/*  Run Record - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The store on its own.
 *
 */

#include <format>
#include <functional>
#include "Common/Cpp/TestRunners/UnitTest.h"
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

UnitTestResult test_store_ids(){
    RunRecordStore store(2);
    const std::string a = store.begin("Test:A");
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

UnitTestResult test_store_events_and_log(){
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

    const std::string line(1000, 'x');
    for (size_t c = 0; c < 100; c++){
        store.log_line(id, std::format("{} {}", c, line));
    }
    record = *store.get(id);
    size_t bytes = 0;
    for (const std::string& l : record.log_tail){
        bytes += l.size() + 1;
    }
    if (!record.log_truncated || bytes > RunRecordStore::MAX_LOG_BYTES || record.log_tail.back().rfind("99 ", 0) != 0){
        return std::format("The log tail keeps the newest {} bytes: {} lines, {} bytes.", RunRecordStore::MAX_LOG_BYTES, record.log_tail.size(), bytes);
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
    if (record.log_tail != std::vector<std::string>{"during"} || record.events.size() != 1){
        return "Only lines and events during the run are recorded.";
    }
    if (record.result->outcome != RunOutcome::COMPLETED || recorder.last_run_id() != id){
        return "The result stands and the run stays known after finish().";
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
    database.add<Test_RunRecord>("store: events, frames and log tail", test_store_events_and_log);
    database.add<Test_RunRecord>("store: old runs release frames", test_store_releases_old_frames);
    database.add<Test_RunRecord>("session recorder", test_session_recorder);
}



}

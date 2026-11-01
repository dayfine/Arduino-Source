/*  Lab Channel - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The channel's text formats, with no app: request files, the journal, the
 *  "state" file, the owner lock, the keep-awake schedule and the run files.
 *
 */

#include <functional>
#include <format>
#include <string>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabRequest.h"
#include "LabRunFiles.h"
#include "LabChannel_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


UnitTestResult test_split_line(){
    if (split_command_line("  press   A 200   100  ") != std::vector<std::string>{"press", "A", "200", "100"}){
        return "Expected 4 words.";
    }
    if (!split_command_line("# just a comment").empty() || !split_command_line("").empty()){
        return "A comment or blank line has no command.";
    }
    if (split_command_line("wait 500 # half a second") != std::vector<std::string>{"wait", "500"}){
        return "A comment after a command should be dropped.";
    }
    if (split_command_line("set KEY \"a # b\" # c") != std::vector<std::string>{"set", "KEY", "\"a", "#", "b\""}){
        return "A '#' inside a quoted string is not a comment.";
    }
    return true;
}

UnitTestResult test_line_after_words(){
    struct Case{
        std::string line;
        std::string json;
    };
    const Case CASES[] = {
        {.line = "set KEY \"a  b\"", .json = "\"a  b\""},
        {.line = "  set   KEY   \"a # b\"   # a comment", .json = "\"a # b\""},
        {.line = "set KEY \"q\\\"#\\\"\" # c", .json = "\"q\\\"#\\\"\""},
        {.line = "set KEY {\"a\": [1, 2]}", .json = "{\"a\": [1, 2]}"},
        {.line = "set KEY", .json = ""},
        {.line = "set KEY # nothing", .json = ""},
        {.line = "set", .json = ""},
    };
    for (const Case& c : CASES){
        const std::string json = line_after_words(c.line, 2);
        if (json != c.json){
            return std::format("{}: expected <{}>, got <{}>", c.line, c.json, json);
        }
    }
    return true;
}

UnitTestResult test_next_file(){
    const Filesystem::Path inbox = Filesystem::unique_temp_path(
        Filesystem::application_scratch_path() / "LabChannelTest"
    );
    Filesystem::create_directories(inbox);
    auto touch = [&](const std::string& name){
        FileIO file(inbox / name, FileMode::WRITE);
    };
    std::string error;
    if (next_request_file(inbox.string())){
        error = "An empty inbox has no next file.";
    }
    touch("b.txt");
    touch("a.out");
    touch("c.txt");
    Filesystem::create_directories(inbox / "a.txt");
    const std::optional<Filesystem::Path> next = next_request_file(inbox.string());
    if (error.empty() && (!next || next->filename().string() != "b.txt")){
        error = "Expected b.txt: the first .txt file by name, skipping other files and folders.";
    }
    Filesystem::remove_all(inbox);
    if (!error.empty()){
        return error;
    }
    return true;
}

UnitTestResult test_parse_request(){
    const LabRequest request = parse_lab_request(
        "# a comment\r\n"
        "owner lab-B\r\n"
        "\r\n"
        "select PokemonBDSP:AutoStory   # pick it\r\n"
        "set START_SEGMENT \"0\"\r\n"
        "start"
    );
    if (request.owner != "lab-B"){
        return "The owner line should be read: got \"" + request.owner + "\".";
    }
    if (request.lines.size() != 3){
        return std::format("Expected 3 command lines, got {}.", request.lines.size());
    }
    if (request.lines[0].number != 4 || request.lines[0].text != "select PokemonBDSP:AutoStory   # pick it"){
        return "A line keeps its number in the file and its text.";
    }
    if (request.lines[0].words != std::vector<std::string>{"select", "PokemonBDSP:AutoStory"}){
        return "A line's words drop the comment.";
    }
    if (request.lines[1].words != std::vector<std::string>{"set", "START_SEGMENT", "\"0\""} || request.lines[2].words[0] != "start"){
        return "Unexpected words.";
    }

    //  "owner" after a command is a command (and an error later), not the owner line.
    const LabRequest late = parse_lab_request("state\nowner lab-B\n");
    if (!late.owner.empty() || late.lines.size() != 2){
        return "An owner line below a command should not set the owner.";
    }
    //  "owner claim TOKEN" has three words and is a command.
    const LabRequest claim = parse_lab_request("owner claim lab-B\n");
    if (!claim.owner.empty() || claim.lines.size() != 1){
        return "\"owner claim\" is a command, not the owner line.";
    }
    return true;
}

UnitTestResult test_journal(){
    if (journal_ok(2, "press HOME", 0ms, 322ms) != "ok 2 press HOME @0-322ms\n"){
        return "Unexpected ok line: " + journal_ok(2, "press HOME", 0ms, 322ms);
    }
    if (journal_error(3, "expect home 3000", "expect home: not seen within 3000ms.")
        != "error 3 (expect home 3000): expect home: not seen within 3000ms.\n"
    ){
        return "Unexpected error line: " + journal_error(3, "expect home 3000", "expect home: not seen within 3000ms.");
    }
    return true;
}

UnitTestResult test_state_text(){
    LabState state{
        .seq = "r1", .state = "RUNNING", .program = "PokemonBDSP:AutoStory",
        .run = "20261101-153012123456-007", .owner = "lab-B", .error = "",
    };
    if (lab_state_text(state) != "seq=r1\nstate=RUNNING\nprogram=PokemonBDSP:AutoStory\nrun=20261101-153012123456-007\nowner=lab-B\nerror=\n"){
        return "Unexpected state text: " + lab_state_text(state);
    }
    state = {.state = "NO_PROGRAM", .error = "The selected panel is not a program."};
    if (lab_state_text(state) != "seq=\nstate=NO_PROGRAM\nprogram=\nrun=\nowner=\nerror=The selected panel is not a program.\n"){
        return "Unexpected state text: " + lab_state_text(state);
    }
    return true;
}

UnitTestResult test_owner(){
    LabOwner owner;
    if (!owner.check("").empty() || !owner.check("anyone").empty()){
        return "An unclaimed channel refuses nobody.";
    }
    if (!owner.claim("", "lab-B").empty() || owner.token() != "lab-B"){
        return "The first claim should succeed.";
    }
    if (!owner.check("lab-B").empty()){
        return "The holder's requests pass.";
    }
    if (owner.check("") != "owned by lab-B" || owner.check("lab-A") != "owned by lab-B"){
        return "Other requests are refused naming the holder: " + owner.check("lab-A");
    }
    if (owner.claim("", "lab-A") != "owned by lab-B" || owner.token() != "lab-B"){
        return "A second claim by someone else is refused.";
    }
    if (!owner.claim("lab-B", "lab-B").empty()){
        return "The holder may claim again.";
    }
    if (owner.release("lab-A") != "owned by lab-B" || owner.token() != "lab-B"){
        return "Only the holder releases.";
    }
    if (!owner.release("lab-B").empty() || !owner.token().empty()){
        return "The holder's release should clear the lock.";
    }
    if (!owner.release("").empty()){
        return "Releasing an unclaimed channel is fine.";
    }
    if (owner.claim("", "") != "owner claim: missing token"){
        return "A claim needs a token.";
    }
    return true;
}

UnitTestResult test_keep_awake(){
    const WallClock t0 = current_time();
    KeepAwakeSchedule schedule;
    if (schedule.on() || schedule.due(t0, false)){
        return "Off by default.";
    }
    if (!schedule.set(split_command_line("keepawake on"), t0).empty() || !schedule.on() || schedule.interval() != 15s){
        return "\"keepawake on\" should turn it on with the 15 s default.";
    }
    if (schedule.due(t0 + 14s, false)){
        return "Not due before an interval has passed.";
    }
    if (!schedule.due(t0 + 15s, false)){
        return "Due once the interval has passed.";
    }
    if (schedule.due(t0 + 16s, false)){
        return "A press resets the interval.";
    }
    if (schedule.due(t0 + 40s, true)){
        return "Never due while a program runs.";
    }
    if (schedule.due(t0 + 50s, false)){
        return "The interval restarts when the program stops.";
    }
    if (!schedule.due(t0 + 65s, false)){
        return "Due an interval after the program stopped.";
    }
    if (!schedule.set(split_command_line("keepawake on 5"), t0).empty() || schedule.interval() != 5s){
        return "The interval can be set.";
    }
    if (!schedule.set(split_command_line("keepawake off"), t0).empty() || schedule.on() || schedule.due(t0 + 60s, false)){
        return "\"keepawake off\" should turn it off.";
    }
    for (const char* line : {"keepawake", "keepawake maybe", "keepawake on 0", "keepawake on soon", "keepawake on 2.5"}){
        if (schedule.set(split_command_line(line), t0).empty()){
            return std::string(line) + " should be an error.";
        }
    }
    return true;
}


std::shared_ptr<const ImageRGB32> test_frame(){
    ImageRGB32 image(4, 4);
    image.fill(0xff0000ff);
    return std::make_shared<const ImageRGB32>(std::move(image));
}

//  Whether `text` has the line `"key": value,` at `indent` spaces, as the
//  4-space result.json puts a top-level (4) or last_event (8) field, which is
//  what labctl.sh's "wait" greps for.
bool has_json_line(const std::string& text, size_t indent, const std::string& key, const std::string& value){
    const std::string line = std::format("\n{}\"{}\": {}", std::string(indent, ' '), key, value);
    return text.find(line + ",\n") != std::string::npos || text.find(line + "\n") != std::string::npos;
}

UnitTestResult test_time_text(){
    const std::string text = lab_time_text(current_time());
    if (text.size() != 23 || text[4] != '-' || text[7] != '-' || text[10] != ' ' || text[13] != ':' || text[16] != ':' || text[19] != '.'){
        return "Expected \"YYYY-MM-DD HH:MM:SS.mmm\", got " + text;
    }
    return true;
}

UnitTestResult test_event_json(){
    const WallClock t0 = current_time();
    RunEvent event{
        .time = t0, .kind = "attempt", .checkpoint = "01.08", .step = "4/7", .attempt = 2,
        .text = "the sign's box did not open", .frame = test_frame(), .pose = "route_204 (12, 41) N",
    };
    const std::string frame = event_frame_file(4, event);
    if (frame != "frame-004-01.08-s4_7-a2.png"){
        return "Unexpected frame file name: " + frame;
    }
    const std::string expected = std::format(
        "{{\"attempt\":2,\"checkpoint\":\"01.08\",\"frame\":\"{}\",\"kind\":\"attempt\",\"pose\":\"route_204 (12, 41) N\","
        "\"step\":\"4/7\",\"text\":\"the sign's box did not open\",\"time\":\"{}\"}}",
        frame, lab_time_text(t0)
    );
    if (run_event_json_line(event, frame) != expected){
        return "Unexpected event line: " + run_event_json_line(event, frame);
    }
    const RunEvent bare{.time = t0, .kind = "checkpoint", .text = "00.01 ok"};
    if (event_frame_file(0, bare) != "frame-000.png"){
        return "A bare event's frame is named by its index alone: " + event_frame_file(0, bare);
    }
    const std::string line = run_event_json_line(bare, std::nullopt);
    if (line != std::format("{{\"kind\":\"checkpoint\",\"text\":\"00.01 ok\",\"time\":\"{}\"}}", lab_time_text(t0))){
        return "Fields an event does not have are left out: " + line;
    }
    return true;
}

UnitTestResult test_result_json(){
    const WallClock t0 = current_time();
    RunRecord record{
        .run_id = "20261101-153012123456-007", .program = "PokemonBDSP:AutoStory",
        .start_time = t0, .status = RunStatus::RUNNING,
    };
    std::string text = run_result_json(record, std::nullopt, {});
    if (!has_json_line(text, 4, "status", "\"running\"") || !has_json_line(text, 4, "event_count", "0") ||
        text.find("outcome") != std::string::npos || text.find("last_event") != std::string::npos
    ){
        return "A running record has a status and no outcome or last event:\n" + text;
    }

    record.events.push_back({.time = t0, .kind = "checkpoint", .checkpoint = "01.07", .text = "ok"});
    record.events.push_back({.time = t0, .kind = "attempt", .checkpoint = "01.08", .step = "4/7", .attempt = 2, .text = "failed"});
    record.status = RunStatus::TERMINAL;
    record.end_time = t0 + std::chrono::seconds(90);
    record.result = RunResult{.outcome = RunOutcome::FAILED, .error = "expect_failed: the Route 204 sign's box did not open"};
    record.log_lines = 12;
    record.dropped_log_lines = 3;
    text = run_result_json(record, "frame-failure.png", {{1, "frame-001-01.08-s4_7-a2.png"}});
    const JsonValue json = parse_json(text);
    const JsonObject* object = json.to_object();
    if (object == nullptr){
        return "result.json should parse:\n" + text;
    }
    if (!has_json_line(text, 4, "status", "\"terminal\"") ||
        !has_json_line(text, 4, "outcome", "\"failed\"") ||
        !has_json_line(text, 4, "error", "\"expect_failed: the Route 204 sign's box did not open\"") ||
        !has_json_line(text, 4, "frame", "\"frame-failure.png\"") ||
        !has_json_line(text, 4, "duration_ms", "90000") ||
        !has_json_line(text, 4, "event_count", "2") ||
        !has_json_line(text, 4, "log_lines", "12") ||
        !has_json_line(text, 4, "dropped_log_lines", "3") ||
        !has_json_line(text, 4, "run_id", "\"20261101-153012123456-007\"")
    ){
        return "Unexpected top-level fields:\n" + text;
    }
    if (!has_json_line(text, 8, "checkpoint", "\"01.08\"") ||
        !has_json_line(text, 8, "step", "\"4/7\"") ||
        !has_json_line(text, 8, "attempt", "2") ||
        !has_json_line(text, 8, "frame", "\"frame-001-01.08-s4_7-a2.png\"")
    ){
        return "last_event should be the newest event, with its frame, at 8 spaces:\n" + text;
    }
    if (object->get_string_throw("end_time") != lab_time_text(*record.end_time)){
        return "end_time is the record's end.";
    }
    return true;
}

UnitTestResult test_meta_json(){
    RunRecord record{
        .run_id = "20261101-153012123456-007", .program = "PokemonFRLG:AutoStory",
        .version = "v0.72.1-beta", .commit = "0123456789ab-dirty",
        .settings = R"({"START_CHECKPOINT": 8})",
    };
    const std::string text = run_meta_json(record);
    if (!has_json_line(text, 4, "program", "\"PokemonFRLG:AutoStory\"") ||
        !has_json_line(text, 4, "version", "\"v0.72.1-beta\"") ||
        !has_json_line(text, 4, "commit", "\"0123456789ab-dirty\"") ||
        !has_json_line(text, 8, "START_CHECKPOINT", "8")
    ){
        return "meta.json names the program and build and embeds the settings as JSON:\n" + text;
    }
    record.settings = "not json";
    if (!has_json_line(run_meta_json(record), 4, "settings", "\"not json\"")){
        return "Settings that don't parse are kept as a string.";
    }
    record.settings.clear();
    if (run_meta_json(record).find("settings") != std::string::npos){
        return "No settings, no field.";
    }
    return true;
}

UnitTestResult test_run_writer(){
    const Filesystem::Path root = Filesystem::unique_temp_path(
        Filesystem::application_scratch_path() / "LabRunWriterTest"
    );
    const std::string runs = root.string() + "/runs/";
    Filesystem::create_directories(root);
    std::string error;
    {
        RunRecordStore store;
        LabRunWriter writer(runs, 2);
        const WallClock t0 = current_time();

        if (writer.run_for_probe(t0)){
            error = "No run, no probe target.";
        }
        const std::string a = store.begin("Test:A", R"({"K": 1})");
        store.event(a, {.kind = "checkpoint", .checkpoint = "00.01", .text = "ok", .frame = test_frame()});
        store.log_line(a, "line 0");
        writer.sync(store);
        const std::string folder = writer.run_folder(a);
        std::string text;
        if (error.empty() && (!file_to_string(folder + "result.json", text) || !has_json_line(text, 4, "status", "\"running\""))){
            error = "After the first sync the run has a folder with a running result.json: " + text;
        }
        if (error.empty() && (!file_to_string(folder + "meta.json", text) || !has_json_line(text, 4, "program", "\"Test:A\"") || !has_json_line(text, 8, "K", "1"))){
            error = "The run's meta.json is written when it starts: " + text;
        }
        if (error.empty() && (!file_to_string(folder + "log.txt", text) || text != "line 0\n")){
            error = "log.txt grows while the run is going: " + text;
        }
        if (error.empty() && !Filesystem::is_regular_file(Filesystem::Path(folder + "frame-000-00.01.png"))){
            error = "The event's frame is saved under its event name.";
        }
        if (error.empty() && writer.run_for_probe(t0) != a){
            error = "A probe during the run belongs to it.";
        }

        //  A probe: its text goes into events.jsonl and its shot is copied.
        const std::string shot = root.string() + "/probe_after_a.png";
        test_frame()->save(shot);
        writer.attach_probe(a, "shot after_a", "shot " + shot + "\n");
        store.event(a, {.kind = "step", .step = "2/3", .text = "walk"});
        store.log_line(a, "line 1");
        store.log_line(a, "line 2");
        store.finish(a, {.outcome = RunOutcome::FAILED, .error = "the box did not open", .frame = test_frame()});
        writer.sync(store);
        if (error.empty() && (!file_to_string(folder + "result.json", text) || !has_json_line(text, 4, "outcome", "\"failed\"") || !has_json_line(text, 4, "frame", "\"frame-failure.png\""))){
            error = "After the run ends result.json is terminal with the outcome and the frame: " + text;
        }
        if (error.empty() && !Filesystem::is_regular_file(Filesystem::Path(folder + "frame-failure.png"))){
            error = "The failure frame is saved.";
        }
        if (error.empty() && (!file_to_string(folder + "log.txt", text) || text != "line 0\nline 1\nline 2\n")){
            error = "log.txt has each of the run's lines once: " + text;
        }
        if (error.empty() && file_to_string(folder + "events.jsonl", text)){
            size_t lines = 0;
            for (char ch : text){
                lines += ch == '\n';
            }
            if (lines != 3 || text.find("\"kind\":\"probe\"") == std::string::npos || text.find("\"frame\":\"probe-probe_after_a.png\"") == std::string::npos){
                error = "events.jsonl has the two events and the probe between them, naming the copied shot:\n" + text;
            }
        }else if (error.empty()){
            error = "events.jsonl should exist.";
        }
        if (error.empty() && !Filesystem::is_regular_file(Filesystem::Path(folder + "probe-probe_after_a.png"))){
            error = "The probe's shot is copied into the run's folder.";
        }
        if (error.empty() && writer.run_for_probe(current_time()) != a){
            error = "A probe right after the run ended still belongs to it.";
        }
        if (error.empty() && writer.run_for_probe(current_time() + LabRunWriter::PROBE_WINDOW + std::chrono::seconds(1))){
            error = "A probe long after the run ended belongs to no run.";
        }

        //  The newest `keep` folders stay.
        const std::string b = store.begin("Test:B");
        //  More lines than the store's tail holds between two polls: the gap is marked.
        const std::string line(1000, 'x');
        for (size_t c = 0; c < 300; c++){
            store.log_line(b, line);
        }
        store.log_line(b, "last");
        writer.sync(store);
        if (error.empty() && (!file_to_string(writer.run_folder(b) + "log.txt", text) || text.rfind("[", 0) != 0 || text.find("lines not copied") == std::string::npos || !text.ends_with("\nlast\n"))){
            error = "Lines the writer missed are marked at the start of log.txt.";
        }
        const std::string c = store.begin("Test:C");
        writer.sync(store);
        if (error.empty() && (Filesystem::is_directory(Filesystem::Path(folder)) || !Filesystem::is_directory(Filesystem::Path(writer.run_folder(b))) || !Filesystem::is_directory(Filesystem::Path(writer.run_folder(c))))){
            error = "With keep=2, the third run removes the first run's folder and keeps the other two.";
        }
        if (error.empty() && writer.run_for_probe(current_time()) != c){
            error = "A probe belongs to the newest run.";
        }
    }
    Filesystem::remove_all(root);
    if (!error.empty()){
        return error;
    }
    return true;
}


class Test_LabChannel : public UnitTest{
public:
    Test_LabChannel(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("CommonFramework::LabChannel - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_LabChannel(UnitTestDatabase& database){
    database.add<Test_LabChannel>("split a line", test_split_line);
    database.add<Test_LabChannel>("line after words", test_line_after_words);
    database.add<Test_LabChannel>("next inbox file", test_next_file);
    database.add<Test_LabChannel>("parse a request", test_parse_request);
    database.add<Test_LabChannel>("journal lines", test_journal);
    database.add<Test_LabChannel>("state text", test_state_text);
    database.add<Test_LabChannel>("owner lock", test_owner);
    database.add<Test_LabChannel>("keep-awake schedule", test_keep_awake);
    database.add<Test_LabChannel>("run files: time text", test_time_text);
    database.add<Test_LabChannel>("run files: event line and frame name", test_event_json);
    database.add<Test_LabChannel>("run files: result.json", test_result_json);
    database.add<Test_LabChannel>("run files: meta.json", test_meta_json);
    database.add<Test_LabChannel>("run files: writer, probes and pruning", test_run_writer);
}



}

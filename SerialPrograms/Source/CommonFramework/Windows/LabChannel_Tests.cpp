/*  Lab Channel - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The channel's text formats, with no app: request files, the journal, the
 *  "state" file, the owner lock and the keep-awake schedule.
 *
 */

#include <functional>
#include <string>
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabRequest.h"
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
        return "Expected 3 command lines, got " + std::to_string(request.lines.size()) + ".";
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
    LabState state{"r1", "RUNNING", "PokemonBDSP:AutoStory", "lab-B", ""};
    if (lab_state_text(state) != "seq=r1\nstate=RUNNING\nprogram=PokemonBDSP:AutoStory\nowner=lab-B\nerror=\n"){
        return "Unexpected state text: " + lab_state_text(state);
    }
    state = {"", "NO_PROGRAM", "", "", "The selected panel is not a program."};
    if (lab_state_text(state) != "seq=\nstate=NO_PROGRAM\nprogram=\nowner=\nerror=The selected panel is not a program.\n"){
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
    database.add<Test_LabChannel>("next inbox file", test_next_file);
    database.add<Test_LabChannel>("parse a request", test_parse_request);
    database.add<Test_LabChannel>("journal lines", test_journal);
    database.add<Test_LabChannel>("state text", test_state_text);
    database.add<Test_LabChannel>("owner lock", test_owner);
    database.add<Test_LabChannel>("keep-awake schedule", test_keep_awake);
}



}

/*  Lab Channel - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The channel's text formats, with no app: request files, the journal and
 *  the "state" file.
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
        "\r\n"
        "select PokemonBDSP:AutoStory   # pick it\r\n"
        "set START_SEGMENT \"0\"\r\n"
        "start"
    );
    if (request.lines.size() != 3){
        return "Expected 3 command lines, got " + std::to_string(request.lines.size()) + ".";
    }
    if (request.lines[0].number != 3 || request.lines[0].text != "select PokemonBDSP:AutoStory   # pick it"){
        return "A line keeps its number in the file and its text.";
    }
    if (request.lines[0].words != std::vector<std::string>{"select", "PokemonBDSP:AutoStory"}){
        return "A line's words drop the comment.";
    }
    if (request.lines[1].words != std::vector<std::string>{"set", "START_SEGMENT", "\"0\""} || request.lines[2].words[0] != "start"){
        return "Unexpected words.";
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
    LabState state{"r1", "RUNNING", "PokemonBDSP:AutoStory", ""};
    if (lab_state_text(state) != "seq=r1\nstate=RUNNING\nprogram=PokemonBDSP:AutoStory\nerror=\n"){
        return "Unexpected state text: " + lab_state_text(state);
    }
    state = {"", "NO_PROGRAM", "", "The selected panel is not a program."};
    if (lab_state_text(state) != "seq=\nstate=NO_PROGRAM\nprogram=\nerror=The selected panel is not a program.\n"){
        return "Unexpected state text: " + lab_state_text(state);
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
}



}

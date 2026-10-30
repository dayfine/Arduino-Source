/*  Command File Runner - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reading command files, with no console: splitting lines, button and
 *  D-pad names, and which inbox file runs next.
 */

#include <functional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "NintendoSwitch_CommandFileParsing.h"
#include "NintendoSwitch_CommandFileRunner_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{

using namespace std::chrono_literals;


//  Whether `body` throws ParseException.
bool throws_parse_error(const std::function<void()>& body){
    try{
        body();
    }catch (ParseException&){
        return true;
    }
    return false;
}


UnitTestResult test_split_line(){
    const CommandLine line("  press   A 200   100  ");
    if (line.words() != std::vector<std::string>{"press", "A", "200", "100"}){
        return "Expected 4 words.";
    }
    if (line.name() != "press" || line.arg(1) != "A"){
        return "Wrong name or first argument.";
    }
    if (line.duration(2, 160ms) != 200ms || line.duration(4, 160ms) != 160ms){
        return "Durations should read the argument, or fall back when it is missing.";
    }
    return true;
}

UnitTestResult test_comments(){
    if (!CommandLine("# just a comment").empty() || !CommandLine("").empty()){
        return "A comment or blank line has no command.";
    }
    const CommandLine line("wait 500 # half a second");
    if (line.words() != std::vector<std::string>{"wait", "500"}){
        return "A comment after a command should be dropped.";
    }
    return true;
}

UnitTestResult test_argument_errors(){
    const CommandLine line("wait soon");
    if (!throws_parse_error([&]{ line.arg(2); })){
        return "A missing argument should be a parse error.";
    }
    if (!throws_parse_error([&]{ line.number(1, 0); })){
        return "A word that isn't a number should be a parse error.";
    }
    return true;
}

UnitTestResult test_buttons(){
    if (parse_buttons("A") != BUTTON_A){
        return "A";
    }
    if (parse_buttons("L+R") != (Button)(BUTTON_L | BUTTON_R)){
        return "L+R should press both.";
    }
    if (!throws_parse_error([]{ parse_buttons("a"); }) || !throws_parse_error([]{ parse_buttons("A+Q"); })){
        return "Unknown or lower-case names should be parse errors.";
    }
    return true;
}

UnitTestResult test_dpad(){
    if (parse_dpad("LEFT") != DPAD_LEFT){
        return "LEFT";
    }
    if (!throws_parse_error([]{ parse_dpad("NORTH"); })){
        return "An unknown direction should be a parse error.";
    }
    return true;
}

UnitTestResult test_next_file(){
    const Filesystem::Path inbox = Filesystem::unique_temp_path(
        Filesystem::application_scratch_path() / "CommandFileRunnerTest"
    );
    Filesystem::create_directories(inbox);
    auto touch = [&](const std::string& name){
        FileIO file(inbox / name, FileMode::WRITE);
    };
    std::string error;
    if (next_command_file(inbox.string())){
        error = "An empty inbox has no next file.";
    }
    touch("b.txt");
    touch("a.out");
    touch("c.txt");
    Filesystem::create_directories(inbox / "a.txt");
    const std::optional<Filesystem::Path> next = next_command_file(inbox.string());
    if (error.empty() && (!next || next->filename().string() != "b.txt")){
        error = "Expected b.txt: the first .txt file by name, skipping other files and folders.";
    }
    Filesystem::remove_all(inbox);
    if (!error.empty()){
        return error;
    }
    return true;
}


class Test_CommandFileRunner : public UnitTest{
public:
    Test_CommandFileRunner(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("NintendoSwitch::CommandFileRunner - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_CommandFileRunner(UnitTestDatabase& database){
    database.add<Test_CommandFileRunner>("split a line", test_split_line);
    database.add<Test_CommandFileRunner>("comments", test_comments);
    database.add<Test_CommandFileRunner>("argument errors", test_argument_errors);
    database.add<Test_CommandFileRunner>("buttons", test_buttons);
    database.add<Test_CommandFileRunner>("D-pad", test_dpad);
    database.add<Test_CommandFileRunner>("next inbox file", test_next_file);
}



}
}

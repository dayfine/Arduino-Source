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
    if (split_command_line("  press   A 200   100  ") != std::vector<std::string>{"press", "A", "200", "100"}){
        return "Expected 4 words.";
    }
    return true;
}

UnitTestResult test_comments(){
    if (!split_command_line("# just a comment").empty() || !split_command_line("").empty()){
        return "A comment or blank line has no command.";
    }
    if (split_command_line("wait 500 # half a second") != std::vector<std::string>{"wait", "500"}){
        return "A comment after a command should be dropped.";
    }
    return true;
}

UnitTestResult test_arguments(){
    const std::vector<std::string> words = split_command_line("stick L 0.5 -1 200");
    if (required_arg(words, 1) != "L"){
        return "required_arg should return the word.";
    }
    if (optional_number(words, 2, 0) != 0.5 || optional_number(words, 3, 0) != -1 || optional_number(words, 5, 7) != 7){
        return "optional_number should read the argument, or fall back when it is missing.";
    }
    if (optional_duration(words, 4, 160ms) != 200ms || optional_duration(words, 5, 160ms) != 160ms){
        return "optional_duration should read the argument, or fall back when it is missing.";
    }
    return true;
}

//  The message of the ParseException `body` throws, or "" if it throws none.
std::string parse_error_message(const std::function<void()>& body){
    try{
        body();
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

UnitTestResult test_argument_errors(){
    const std::vector<std::string> words = split_command_line("wait soon");
    if (parse_error_message([&]{ required_arg(words, 2); }) != "wait: missing argument 2"){
        return "A missing argument should be a parse error naming the command.";
    }
    if (parse_error_message([&]{ optional_number(words, 1, 0); }) != "wait: argument 1 is not a number: soon"){
        return "A word that isn't a number should be a parse error.";
    }
    if (parse_error_message([&]{ optional_duration(words, 1, 0ms); }) != "wait: argument 1 is not a number: soon"){
        return "A duration that isn't a number should be a parse error.";
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

UnitTestResult test_taps(){
    struct Case{
        std::string line;
        TapsArguments expected;
    };
    const Case CASES[] = {
        {.line = "taps UP 3", .expected = {.direction = DPAD_UP, .count = 3, .all = false, .hold = 160ms, .release = 640ms, .still = 16}},
        {.line = "taps LEFT 4 all", .expected = {.direction = DPAD_LEFT, .count = 4, .all = true, .hold = 160ms, .release = 640ms, .still = 16}},
        {.line = "taps RIGHT 2 100 500", .expected = {.direction = DPAD_RIGHT, .count = 2, .all = false, .hold = 100ms, .release = 500ms, .still = 16}},
        {.line = "taps DOWN 1 all 200 700 20", .expected = {.direction = DPAD_DOWN, .count = 1, .all = true, .hold = 200ms, .release = 700ms, .still = 20}},
    };
    for (const Case& c : CASES){
        const TapsArguments taps = parse_taps(split_command_line(c.line));
        if (taps.direction != c.expected.direction || taps.count != c.expected.count || taps.all != c.expected.all){
            return c.line + ": wrong direction, count or all flag.";
        }
        if (taps.hold != c.expected.hold || taps.release != c.expected.release || taps.still != c.expected.still){
            return c.line + ": expected hold " + std::to_string(c.expected.hold.count())
                + ", release " + std::to_string(c.expected.release.count()) + ", still " + std::to_string(c.expected.still)
                + "; read " + std::to_string(taps.hold.count()) + ", " + std::to_string(taps.release.count()) + ", " + std::to_string(taps.still);
        }
    }
    for (const char* line : {"taps UP", "taps UP x", "taps UP 0", "taps UP 2.5", "taps NORTH 2", "taps UP 2 all soon"}){
        if (!throws_parse_error([&]{ parse_taps(split_command_line(line)); })){
            return std::string(line) + " should be a parse error.";
        }
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
    database.add<Test_CommandFileRunner>("arguments", test_arguments);
    database.add<Test_CommandFileRunner>("argument errors", test_argument_errors);
    database.add<Test_CommandFileRunner>("buttons", test_buttons);
    database.add<Test_CommandFileRunner>("D-pad", test_dpad);
    database.add<Test_CommandFileRunner>("taps arguments", test_taps);
    database.add<Test_CommandFileRunner>("next inbox file", test_next_file);
}



}
}

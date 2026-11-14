/*  PokemonLZA AutoStory - Command File Runner commands Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Windows/LabRequest.h"
#include "NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.h"
#include "PokemonLZA_AutoStoryCommands.h"
#include "PokemonLZA_AutoStoryCommands_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Words = std::vector<std::string>;

//  What a command's parser read, as text, so one table covers every command.
const std::map<std::string, std::function<std::string(const Words&)>, std::less<>>& parsed_summaries(){
    static const std::map<std::string, std::function<std::string(const Words&)>, std::less<>> SUMMARIES{
        {"lza_talk", [](const Words& words){
            return std::string(parse_lza_talk(words) == BUTTON_A ? "button A" : "button B");
        }},
        {"lza_talk_to", [](const Words& words){ parse_no_arguments(words); return std::string("no arguments"); }},
        {"lza_pick", [](const Words& words){ return std::format("choice {}", parse_lza_pick(words)); }},
        {"lza_move", [](const Words& words){
            const StoryMove move = parse_lza_move(words);
            const std::string heading = move.heading ? std::format("heading {}", *move.heading) : "ahead";
            std::string legs;
            for (const StoryWalkLeg& leg : move.legs){
                legs += std::format(", turn {} walk {} ms", leg.turn, leg.walk.count());
            }
            return heading + legs;
        }},
        {"lza_face", [](const Words& words){
            const FaceArguments face = parse_lza_face(words);
            return std::format("heading {} +/- {}", face.heading, face.tolerance);
        }},
        {"lza_fly", [](const Words& words){
            const FlyArguments fly = parse_lza_fly(words);
            return std::format("{} in {}", get_location_item_from_enum(fly.location).slug, language_data(fly.language).code);
        }},
        {"lza_battle", [](const Words& words){
            const BattleArguments battle = parse_lza_battle(words);
            return battle.in_progress ? std::format("in progress, {} s", battle.timeout.count()) : std::string("starting");
        }},
        {"lza_save", [](const Words& words){ parse_no_arguments(words); return std::string("no arguments"); }},
    };
    return SUMMARIES;
}

//  The summary of `line`'s arguments, or "error: <message>" for a ParseException.
std::string parse_line(const std::string& line){
    const Words words = split_command_line(line);
    try{
        return parsed_summaries().find(words[0])->second(words);
    }catch (ParseException& e){
        return "error: " + e.message();
    }
}


struct ParseCase{
    std::string name;
    struct Input{
        std::string line;
    };
    Input input;
    struct Expectation{
        std::string parsed;     //  What the parser read, or "error: <message>".
    };
    Expectation expectation;
};

const ParseCase PARSE_CASES[] = {
    {.name = "talk mashes B by default", .input = {.line = "lza_talk"}, .expectation = {.parsed = "button B"}},
    {.name = "talk with A", .input = {.line = "lza_talk A"}, .expectation = {.parsed = "button A"}},
    {.name = "talk with another button", .input = {.line = "lza_talk X"}, .expectation = {.parsed = "error: lza_talk: expected A or B, got X"}},
    {.name = "talk_to takes nothing", .input = {.line = "lza_talk_to now"}, .expectation = {.parsed = "error: lza_talk_to: expected 0 to 0 arguments, got 1"}},
    {.name = "pick the second choice", .input = {.line = "lza_pick 1"}, .expectation = {.parsed = "choice 1"}},
    {.name = "pick needs a whole number", .input = {.line = "lza_pick 1.5"}, .expectation = {.parsed = "error: lza_pick: the choice must be a whole number from 0, got 1.5"}},
    {.name = "pick needs a choice", .input = {.line = "lza_pick"}, .expectation = {.parsed = "error: lza_pick: expected 1 to 1 arguments, got 0"}},
    {.name = "move toward a heading", .input = {.line = "lza_move 90 1500"}, .expectation = {.parsed = "heading 90, turn 0 walk 1500 ms"}},
    {.name = "move straight ahead", .input = {.line = "lza_move ahead 800"}, .expectation = {.parsed = "ahead, turn 0 walk 800 ms"}},
    {.name = "move heading out of range", .input = {.line = "lza_move 360 800"}, .expectation = {.parsed = "error: lza_move: heading 360 is not in [0, 360)"}},
    {.name = "move for no time", .input = {.line = "lza_move 90 0"}, .expectation = {.parsed = "error: lza_move: the walk time must be more than 0 ms, got 0"}},
    {.name = "face with the default tolerance", .input = {.line = "lza_face 270"}, .expectation = {.parsed = "heading 270 +/- 10"}},
    {.name = "face with a tolerance", .input = {.line = "lza_face 45 20"}, .expectation = {.parsed = "heading 45 +/- 20"}},
    {.name = "face with no tolerance", .input = {.line = "lza_face 45 0"}, .expectation = {.parsed = "error: lza_face: the tolerance must be in (0, 180), got 0"}},
    {.name = "fly in English by default", .input = {.line = "lza_fly centrico-plaza"}, .expectation = {.parsed = "centrico-plaza in eng"}},
    {.name = "fly in French", .input = {.line = "lza_fly hotel-z fra"}, .expectation = {.parsed = "hotel-z in fra"}},
    {.name = "fly to an unknown place", .input = {.line = "lza_fly paris"}, .expectation = {.parsed = "error: lza_fly: unknown place: paris"}},
    {.name = "fly in an unknown language", .input = {.line = "lza_fly hotel-z xx"}, .expectation = {.parsed = "error: lza_fly: unknown language code: xx"}},
    {.name = "battle that is starting", .input = {.line = "lza_battle"}, .expectation = {.parsed = "starting"}},
    {.name = "battle in progress", .input = {.line = "lza_battle now"}, .expectation = {.parsed = "in progress, 180 s"}},
    {.name = "battle in progress with a timeout", .input = {.line = "lza_battle now 60"}, .expectation = {.parsed = "in progress, 60 s"}},
    {.name = "battle with another word", .input = {.line = "lza_battle later"}, .expectation = {.parsed = "error: lza_battle: expected \"now\", got later"}},
    {.name = "battle with no timeout", .input = {.line = "lza_battle now 0"}, .expectation = {.parsed = "error: lza_battle: the timeout must be a whole number of seconds above 0, got 0"}},
    {.name = "save takes nothing", .input = {.line = "lza_save now"}, .expectation = {.parsed = "error: lza_save: expected 0 to 0 arguments, got 1"}},
};

UnitTestResult check_parse_case(const ParseCase& test){
    const std::string read = parse_line(test.input.line);
    if (read != test.expectation.parsed){
        return std::format("\"{}\": expected \"{}\", read \"{}\"", test.input.line, test.expectation.parsed, read);
    }
    return true;
}

class ParseTest : public UnitTest{
public:
    explicit ParseTest(const ParseCase& test)
        : UnitTest("PokemonLZA::AutoStory Commands - " + test.name)
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_parse_case(m_test);
    }

private:
    const ParseCase& m_test;
};


struct LookupCase{
    std::string name;
    struct Input{
        std::string command;
    };
    Input input;
    struct Expectation{
        CommandFileCommandKind kind = CommandFileCommandKind::UNKNOWN;
    };
    Expectation expectation;
};

const LookupCase LOOKUP_CASES[] = {
    {.name = "lookup talk", .input = {.command = "lza_talk"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup talk_to", .input = {.command = "lza_talk_to"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup pick", .input = {.command = "lza_pick"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup move", .input = {.command = "lza_move"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup face", .input = {.command = "lza_face"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup fly", .input = {.command = "lza_fly"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup battle", .input = {.command = "lza_battle"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup heal, which no action backs", .input = {.command = "lza_heal"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
    {.name = "lookup shop, which no action backs", .input = {.command = "lza_shop"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
    {.name = "lookup save", .input = {.command = "lza_save"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup the built-in type", .input = {.command = "type"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.name = "lookup the built-in detect", .input = {.command = "detect"}, .expectation = {.kind = CommandFileCommandKind::READS_STREAM}},
    {.name = "lookup an unknown command", .input = {.command = "lza_dance"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
};

UnitTestResult check_lookup_case(const LookupCase& test){
    register_lza_command_file_commands();
    const CommandFileCommandKind read = command_file_command_kind(test.input.command);
    if (read != test.expectation.kind){
        return std::format("{}: expected kind {}, read {}", test.input.command, (int)test.expectation.kind, (int)read);
    }
    return true;
}

class LookupTest : public UnitTest{
public:
    explicit LookupTest(const LookupCase& test)
        : UnitTest("PokemonLZA::AutoStory Commands - " + test.name)
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_lookup_case(m_test);
    }

private:
    const LookupCase& m_test;
};

} // namespace


void add_tests_AutoStory_Commands(UnitTestDatabase& database){
    for (const ParseCase& test : PARSE_CASES){
        database.add<ParseTest>(test);
    }
    for (const LookupCase& test : LOOKUP_CASES){
        database.add<LookupTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

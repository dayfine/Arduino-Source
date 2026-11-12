/*  PokemonLZA AutoStory - Command File Runner commands Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
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


struct ParseInput{
    std::string_view name;
    std::string line;
};

struct ParseExpectation{
    std::string parsed;
};

struct ParseCase{
    ParseInput input;
    ParseExpectation expectation;
};

const ParseCase PARSE_CASES[] = {
    {.input = {.name = "talk mashes B by default", .line = "lza_talk"}, .expectation = {.parsed = "button B"}},
    {.input = {.name = "talk with A", .line = "lza_talk A"}, .expectation = {.parsed = "button A"}},
    {.input = {.name = "talk with another button", .line = "lza_talk X"}, .expectation = {.parsed = "error: lza_talk: expected A or B, got X"}},
    {.input = {.name = "talk_to takes nothing", .line = "lza_talk_to now"}, .expectation = {.parsed = "error: lza_talk_to: expected 0 to 0 arguments, got 1"}},
    {.input = {.name = "pick the second choice", .line = "lza_pick 1"}, .expectation = {.parsed = "choice 1"}},
    {.input = {.name = "pick needs a whole number", .line = "lza_pick 1.5"}, .expectation = {.parsed = "error: lza_pick: the choice must be a whole number from 0, got 1.5"}},
    {.input = {.name = "pick needs a choice", .line = "lza_pick"}, .expectation = {.parsed = "error: lza_pick: expected 1 to 1 arguments, got 0"}},
    {.input = {.name = "move toward a heading", .line = "lza_move 90 1500"}, .expectation = {.parsed = "heading 90, turn 0 walk 1500 ms"}},
    {.input = {.name = "move straight ahead", .line = "lza_move ahead 800"}, .expectation = {.parsed = "ahead, turn 0 walk 800 ms"}},
    {.input = {.name = "move heading out of range", .line = "lza_move 360 800"}, .expectation = {.parsed = "error: lza_move: heading 360 is not in [0, 360)"}},
    {.input = {.name = "move for no time", .line = "lza_move 90 0"}, .expectation = {.parsed = "error: lza_move: the walk time must be more than 0 ms, got 0"}},
    {.input = {.name = "face with the default tolerance", .line = "lza_face 270"}, .expectation = {.parsed = "heading 270 +/- 10"}},
    {.input = {.name = "face with a tolerance", .line = "lza_face 45 20"}, .expectation = {.parsed = "heading 45 +/- 20"}},
    {.input = {.name = "face with no tolerance", .line = "lza_face 45 0"}, .expectation = {.parsed = "error: lza_face: the tolerance must be in (0, 180), got 0"}},
    {.input = {.name = "fly in English by default", .line = "lza_fly centrico-plaza"}, .expectation = {.parsed = "centrico-plaza in eng"}},
    {.input = {.name = "fly in French", .line = "lza_fly hotel-z fra"}, .expectation = {.parsed = "hotel-z in fra"}},
    {.input = {.name = "fly to an unknown place", .line = "lza_fly paris"}, .expectation = {.parsed = "error: lza_fly: unknown place: paris"}},
    {.input = {.name = "fly in an unknown language", .line = "lza_fly hotel-z xx"}, .expectation = {.parsed = "error: lza_fly: unknown language code: xx"}},
    {.input = {.name = "battle that is starting", .line = "lza_battle"}, .expectation = {.parsed = "starting"}},
    {.input = {.name = "battle in progress", .line = "lza_battle now"}, .expectation = {.parsed = "in progress, 180 s"}},
    {.input = {.name = "battle in progress with a timeout", .line = "lza_battle now 60"}, .expectation = {.parsed = "in progress, 60 s"}},
    {.input = {.name = "battle with another word", .line = "lza_battle later"}, .expectation = {.parsed = "error: lza_battle: expected \"now\", got later"}},
    {.input = {.name = "battle with no timeout", .line = "lza_battle now 0"}, .expectation = {.parsed = "error: lza_battle: the timeout must be a whole number of seconds above 0, got 0"}},
    {.input = {.name = "save takes nothing", .line = "lza_save now"}, .expectation = {.parsed = "error: lza_save: expected 0 to 0 arguments, got 1"}},
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
        : UnitTest("PokemonLZA::AutoStory Commands - " + std::string(test.input.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_parse_case(m_test);
    }

private:
    const ParseCase& m_test;
};


struct LookupInput{
    std::string_view name;
    std::string command;
};

struct LookupExpectation{
    CommandFileCommandKind kind = CommandFileCommandKind::UNKNOWN;
};

struct LookupCase{
    LookupInput input;
    LookupExpectation expectation;
};

const LookupCase LOOKUP_CASES[] = {
    {.input = {.name = "lookup talk", .command = "lza_talk"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup talk_to", .command = "lza_talk_to"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup pick", .command = "lza_pick"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup move", .command = "lza_move"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup face", .command = "lza_face"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup fly", .command = "lza_fly"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup battle", .command = "lza_battle"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup heal, which no action backs", .command = "lza_heal"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
    {.input = {.name = "lookup shop, which no action backs", .command = "lza_shop"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
    {.input = {.name = "lookup save", .command = "lza_save"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup the built-in type", .command = "type"}, .expectation = {.kind = CommandFileCommandKind::DRIVES_CONSOLE}},
    {.input = {.name = "lookup the built-in detect", .command = "detect"}, .expectation = {.kind = CommandFileCommandKind::READS_STREAM}},
    {.input = {.name = "lookup an unknown command", .command = "lza_dance"}, .expectation = {.kind = CommandFileCommandKind::UNKNOWN}},
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
        : UnitTest("PokemonLZA::AutoStory Commands - " + std::string(test.input.name))
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

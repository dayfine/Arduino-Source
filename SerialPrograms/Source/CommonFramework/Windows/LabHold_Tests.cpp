/*  Lab Hold - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The user hold with no app: the "Lab/hold" text, which commands run while
 *  it is on, and when the user's driving puts it on.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabHold.h"
#include "LabHold_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


std::string text_of(const std::optional<std::string>& reason){
    return reason ? *reason : "(none)";
}


//  hold.sh writes the file with `date -u`; the app reads it back.
UnitTestResult test_parse_hold(){
    struct Case{
        std::string name;
        std::string input;
        std::optional<LabHold> expectation;
    };
    const std::vector<Case> cases{
        {
            .name = "written by hold.sh",
            .input = "owner=user\nreason=hold_sh\nacquired_at=2026-11-15T21:04:05Z\n",
            .expectation = LabHold{.owner = "user", .reason = "hold_sh", .acquired_at = "2026-11-15T21:04:05Z"},
        },
        {
            .name = "CRLF and an unknown line",
            .input = "owner=user\r\nby=keyboard\r\nreason=user_input\r\n",
            .expectation = LabHold{.owner = "user", .reason = "user_input", .acquired_at = ""},
        },
        {.name = "empty file", .input = "", .expectation = std::nullopt},
        {.name = "no owner", .input = "reason=hold_sh\nacquired_at=2026-11-15T21:04:05Z\n", .expectation = std::nullopt},
        {.name = "empty owner", .input = "owner=\n", .expectation = std::nullopt},
    };
    for (const Case& c : cases){
        const std::optional<LabHold> hold = parse_lab_hold(c.input);
        if (hold != c.expectation){
            return std::format(
                "{}: got owner \"{}\", expected \"{}\"",
                c.name, hold ? hold->owner : "(no hold)", c.expectation ? c.expectation->owner : "(no hold)"
            );
        }
    }
    const LabHold written{.owner = "user", .reason = "ui_start", .acquired_at = "2026-11-15T21:04:05.120Z"};
    if (parse_lab_hold(lab_hold_text(written)) != written){
        return "The app's own Lab/hold text should read back as the hold it wrote.";
    }
    return true;
}


//  While held, only what reads runs.
UnitTestResult test_runs_while_held(){
    struct Input{
        std::vector<std::string> words;
        bool reads_stream = false;
    };
    struct Case{
        std::string name;
        Input input;
        bool expectation;
    };
    const std::vector<Case> cases{
        {.name = "press", .input = {.words = {"press", "A"}}, .expectation = false},
        {.name = "stick", .input = {.words = {"stick", "L", "0", "1"}}, .expectation = false},
        {.name = "close_game", .input = {.words = {"close_game"}}, .expectation = false},
        {.name = "start", .input = {.words = {"start"}}, .expectation = false},
        {.name = "stop", .input = {.words = {"stop"}}, .expectation = false},
        {.name = "set", .input = {.words = {"set", "KEY", "1"}}, .expectation = false},
        {.name = "select", .input = {.words = {"select", "PokemonBDSP:AutoStory"}}, .expectation = false},
        {.name = "preflight", .input = {.words = {"preflight", "{}"}}, .expectation = false},
        {.name = "keepawake on", .input = {.words = {"keepawake", "on"}}, .expectation = false},
        {.name = "keepawake off", .input = {.words = {"keepawake", "off"}}, .expectation = true},
        {.name = "shot", .input = {.words = {"shot", "x"}, .reads_stream = true}, .expectation = true},
        {.name = "snapshot", .input = {.words = {"snapshot", "frame"}}, .expectation = true},
        {.name = "state", .input = {.words = {"state"}}, .expectation = true},
        {.name = "help", .input = {.words = {"help"}}, .expectation = true},
        {.name = "describe_program", .input = {.words = {"describe_program"}}, .expectation = true},
        {.name = "owner", .input = {.words = {"owner", "release"}}, .expectation = true},
    };
    for (const Case& c : cases){
        const bool runs = lab_runs_while_held(c.input.words, c.input.reads_stream);
        if (runs != c.expectation){
            return std::format("{}: runs while held = {}, expected {}", c.name, runs, c.expectation);
        }
    }
    return true;
}


//  One check of the selected program, for LabHoldTrigger::check().
struct Observation{
    std::optional<ProgramTransition> transition;
    std::optional<WallClock> last_user_input;
};

const WallClock SELECTED = WallClock{} + 1000s;

ProgramTransition transition(ProgramAction action, ActionSource source, Seconds after_selection){
    return ProgramTransition{.action = action, .source = source, .request_id = std::nullopt, .at = SELECTED + after_selection};
}


//  The user's own Start, Stop or press puts the hold on; the lab's own
//  requests and the program's own end do not.
UnitTestResult test_trigger(){
    struct Case{
        std::string name;
        Observation input;
        std::optional<std::string> expectation;
    };
    const std::vector<Case> cases{
        {
            .name = "Start clicked",
            .input = {.transition = transition(ProgramAction::START, ActionSource::UI, 5s)},
            .expectation = "ui_start",
        },
        {
            .name = "Stop clicked",
            .input = {.transition = transition(ProgramAction::STOP, ActionSource::UI, 5s)},
            .expectation = "ui_stop",
        },
        {
            .name = "lab request started it",
            .input = {.transition = transition(ProgramAction::START, ActionSource::LAB, 5s)},
            .expectation = std::nullopt,
        },
        {
            .name = "program ended by itself",
            .input = {.transition = transition(ProgramAction::STOP, ActionSource::PROGRAM, 5s)},
            .expectation = std::nullopt,
        },
        {
            .name = "program ended with an error",
            .input = {.transition = transition(ProgramAction::STOP, ActionSource::PROGRAM_ERROR, 5s)},
            .expectation = std::nullopt,
        },
        {
            .name = "Start clicked before this panel was selected",
            .input = {.transition = transition(ProgramAction::START, ActionSource::UI, -5s)},
            .expectation = std::nullopt,
        },
        {
            .name = "button pressed in the window",
            .input = {.last_user_input = SELECTED + 5s},
            .expectation = "user_input",
        },
        {
            .name = "button pressed before this panel was selected",
            .input = {.last_user_input = SELECTED - 5s},
            .expectation = std::nullopt,
        },
        {.name = "nothing happened", .input = {}, .expectation = std::nullopt},
    };
    for (const Case& c : cases){
        LabHoldTrigger trigger;
        const std::optional<std::string> reason = trigger.check(c.input.transition, c.input.last_user_input, SELECTED);
        if (reason != c.expectation){
            return std::format("{}: got {}, expected {}", c.name, text_of(reason), text_of(c.expectation));
        }
    }
    return true;
}


//  Each Start, Stop or press is news once: after "hold.sh off" the same
//  click must not put the hold back; the next one does.
UnitTestResult test_trigger_reports_once(){
    LabHoldTrigger trigger;
    const ProgramTransition start = transition(ProgramAction::START, ActionSource::UI, 5s);
    if (trigger.check(start, SELECTED + 6s, SELECTED) != "ui_start"){
        return "The first check should report the Start.";
    }
    const std::optional<std::string> again = trigger.check(start, SELECTED + 6s, SELECTED);
    if (again){
        return std::format("The same Start and press again: got {}, expected (none).", *again);
    }
    if (trigger.check(start, SELECTED + 20s, SELECTED) != "user_input"){
        return "A later press should be reported.";
    }
    const ProgramTransition stop = transition(ProgramAction::STOP, ActionSource::UI, 30s);
    if (trigger.check(stop, SELECTED + 20s, SELECTED) != "ui_stop"){
        return "A later Stop should be reported.";
    }
    return true;
}


class Test_LabHold : public UnitTest{
public:
    Test_LabHold(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("CommonFramework::LabHold - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

}


void add_tests_LabHold(UnitTestDatabase& database){
    database.add<Test_LabHold>("parse Lab/hold", test_parse_hold);
    database.add<Test_LabHold>("commands while held", test_runs_while_held);
    database.add<Test_LabHold>("user driving puts it on", test_trigger);
    database.add<Test_LabHold>("each event once", test_trigger_reports_once);
}



}

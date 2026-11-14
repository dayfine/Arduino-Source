/*  AutoStory Step Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's table of interrupts between steps (step_interrupts()) on the
 *  shared executor, against a scripted screen: what each kind of step is
 *  stopped by and what it starts on.
 *
 */

#include <format>
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::StoryStepKind;

const WallClock T0 = WallClock() + std::chrono::hours(1);

//  What FrlgScreenObserver reports of each screen (no PLAYER_CAN_MOVE).
const ScreenEvents OVERWORLD{};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};

//  One frame of the scripted screen: its events, `ms` after T0.
struct Frame{
    int ms;
    ScreenEvents events;
};

//  FRLG's rows bound to a recorder, and its interrupts to a scripted look
//  and handlers that note what they cleared, all in `trace`.
class FakeGame{
public:
    explicit FakeGame(std::vector<Frame> screen)
        : m_screen(std::move(screen))
        , m_table(make_story_action_table([this](StoryActionId id){ return recorder(id); }))
        , m_executor(m_table, m_guard)
        , m_interrupts(step_interrupts([this](bool){ return look(); }, noting()))
    {
        m_executor.set_interrupts(&m_interrupts);
    }

    //  `kind`'s step through the table, from a known pose: its outcome.
    Pokemon::Outcome run(StoryStepKind kind){
        Pokemon::StoryStep step;
        step.kind = kind;
        step.name = "synthetic";
        return m_executor.run(step, state);
    }

    Pokemon::RunState state{.pose = to_pose(StoryPose{"Route1", TilePos{1, 2}, std::nullopt})};
    std::vector<std::string> trace;

private:
    //  The screen until its last frame, then quiet: a cleared interrupt is
    //  gone at the next look.
    Pokemon::ScreenLook look(){
        Pokemon::ScreenLook out;
        int until = 300;
        for (const Frame& frame : std::exchange(m_screen, {})){
            out.log.record(frame.events, T0 + Milliseconds(frame.ms));
            until = std::max(until, frame.ms + 300);
        }
        out.window = {T0, T0 + Milliseconds(until)};
        return out;
    }
    StepInterruptActions noting(){
        const auto note = [this](std::string_view kind){
            return [this, kind](const Pokemon::InterruptVerdict&, Pokemon::RunState&){
                trace.push_back(std::format("clear {}", kind));
                return Pokemon::outcome_ok();
            };
        };
        return {.wild = note("wild"), .trainer = note("trainer"), .dialog = note("dialog"), .warp = note("warp")};
    }
    Pokemon::StoryAction recorder(StoryActionId id){
        return {.act = [this, id](const Pokemon::StoryStep&, Pokemon::RunState&){
            trace.emplace_back(story_action_name(id));
            return Pokemon::outcome_ok();
        }};
    }

    std::vector<Frame> m_screen;
    Pokemon::StepGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
    Pokemon::StoryInterrupts m_interrupts;
};

//  Runs a `kind` step after `screen`; empty if what ran was `expected`.
std::string ran(std::vector<Frame> screen, StoryStepKind kind, const std::vector<std::string>& expected){
    FakeGame game(std::move(screen));
    const Pokemon::Outcome outcome = game.run(kind);
    if (outcome.succeeded() && game.trace == expected){
        return "";
    }
    std::string got = Pokemon::to_string(outcome) + ":";
    for (const std::string& item : game.trace){
        got += " " + item;
    }
    return std::string(Pokemon::story_step_kind_name(kind)) + ": got " + got;
}

UnitTestResult test_message(){
    const std::vector<Frame> message{{0, OVERWORLD}, {100, BOX}};
    for (const std::string& error : {
        ran(message, StoryStepKind::ROUTE, {"clear dialog", "route"}),
        ran(message, StoryStepKind::TALK, {"clear dialog", "talk"}),
        ran(message, StoryStepKind::MASH, {"mash"}),
        ran(message, StoryStepKind::ANSWER, {"answer"}),
        ran(message, StoryStepKind::BATTLE, {"battle"}),
    }){
        if (!error.empty()){
            return error;
        }
    }
    return true;
}

UnitTestResult test_battle(){
    const std::vector<Frame> wild{{0, OVERWORLD}, {100, WHITE}, {400, OVERWORLD}, {1500, BATTLE}};
    const std::vector<Frame> trainer{{0, OVERWORLD}, {100, BOX}, {2000, BLACK}, {2500, BATTLE}};
    for (const std::string& error : {
        ran(wild, StoryStepKind::ROUTE, {"clear wild", "route"}),
        ran(wild, StoryStepKind::BATTLE, {"battle"}),
        ran(trainer, StoryStepKind::LEGS, {"clear trainer", "legs"}),
        ran(trainer, StoryStepKind::BATTLE, {"battle"}),
        ran(trainer, StoryStepKind::GAME, {"game"}),
    }){
        if (!error.empty()){
            return error;
        }
    }
    return true;
}

UnitTestResult test_warp(){
    const std::vector<Frame> warp{{0, OVERWORLD}, {100, BLACK}, {600, OVERWORLD}, {9000, OVERWORLD}};
    FakeGame game(warp);
    const Pokemon::Outcome outcome = game.run(StoryStepKind::STEP);
    if (!outcome.succeeded() || game.trace != std::vector<std::string>{"clear warp", "step"}){
        return "Expected the warp cleared before the step, got " + Pokemon::to_string(outcome);
    }
    if (game.state.pose.where != nullptr){
        return "Expected the pose lost after a warp, got " + Pokemon::to_string(game.state.pose);
    }
    const std::string error = ran(warp, StoryStepKind::GAME, {"game"});
    if (!error.empty()){
        return error;
    }
    return true;
}


struct StepInterruptsInput{
    std::string name;
    UnitTestResult (*body)();
};
struct StepInterruptsExpectation{
    std::string_view description;
};
struct StepInterruptsTestCase{
    StepInterruptsInput input;
    StepInterruptsExpectation expectation;
};

class Test_StepInterrupts : public UnitTest{
public:
    explicit Test_StepInterrupts(StepInterruptsTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryStepInterrupts - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StepInterruptsTestCase m_scenario;
};

const StepInterruptsTestCase STEP_INTERRUPTS_CASES[] = {
    {
        .input = {.name = "a message before a step", .body = test_message},
        .expectation = {.description = "read before a walk or a talk; the step's own before one that presses through text"},
    },
    {
        .input = {.name = "a battle before a step", .body = test_battle},
        .expectation = {.description = "run from or fought before a walk; a BATTLE's own, its lines missed or not"},
    },
    {
        .input = {.name = "a warp before a step", .body = test_warp},
        .expectation = {.description = "pressed through and the pose lost; a GAME's own"},
    },
};

} // namespace


void add_tests_AutoStoryStepInterrupts(UnitTestDatabase& database){
    for (const auto& scenario : STEP_INTERRUPTS_CASES){
        database.add<Test_StepInterrupts>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

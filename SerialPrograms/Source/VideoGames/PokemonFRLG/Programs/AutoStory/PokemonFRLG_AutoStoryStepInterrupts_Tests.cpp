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
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

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
        , m_interrupts(step_interrupts([this](bool){ return look(); }, noting()))
    {}

    //  `action`'s step through the table, from a known pose: its failure, if any.
    std::optional<Pokemon::StoryFailure> run(const std::string& action){
        StoryStep step;
        step.action = action;
        step.name = "synthetic";
        const StoryActionId id = story_action_id_of(action);
        const bool check_interrupts = id == StoryActionId::ROUTE || id == StoryActionId::LEGS
            || id == StoryActionId::STEP || id == StoryActionId::TALK;
        if (check_interrupts){
            const Pokemon::InterruptCheck cleared = Pokemon::clear_story_interrupt(m_interrupts, step.name, step.action);
            if (cleared.pose_lost){
                state.pose = std::nullopt;
            }
            if (cleared.failure){
                return cleared.failure;
            }
        }
        const auto it = m_table.find(step.action);
        if (it == m_table.end() || !it->second){
            return frlg_unsupported_step(step);
        }
        return it->second(step, state);
    }

    FrlgRunState state{.pose = to_grid_pose(StoryPose{"Route1", TilePos{1, 2}, std::nullopt})};
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
        const auto note = [this](std::string_view kind, bool lose_pose){
            return [this, kind, lose_pose](const Pokemon::InterruptVerdict&) -> std::optional<Pokemon::StoryFailure>{
                trace.push_back(std::format("clear {}", kind));
                if (lose_pose){
                    state.pose = std::nullopt;
                }
                return std::nullopt;
            };
        };
        return {
            .wild = note("wild", false),
            .trainer = note("trainer", false),
            .dialog = note("dialog", false),
            .warp = note("warp", true),
        };
    }
    FrlgStepAction recorder(StoryActionId id){
        return [this, id](const StoryStep&, FrlgRunState&) -> std::optional<Pokemon::StoryFailure>{
            trace.emplace_back(story_action_name(id));
            return std::nullopt;
        };
    }

    std::vector<Frame> m_screen;
    FrlgActionTable m_table;
    Pokemon::StoryInterrupts m_interrupts;
};

//  Runs an `action` step after `screen`; empty if what ran was `expected`.
std::string ran(std::vector<Frame> screen, const std::string& action, const std::vector<std::string>& expected){
    FakeGame game(std::move(screen));
    const std::optional<Pokemon::StoryFailure> failure = game.run(action);
    if (!failure && game.trace == expected){
        return "";
    }
    std::string got = (failure ? failure->code : std::string("ok")) + ":";
    for (const std::string& item : game.trace){
        got += " " + item;
    }
    return action + ": got " + got;
}

UnitTestResult test_message(){
    const std::vector<Frame> message{{0, OVERWORLD}, {100, BOX}};
    for (const std::string& error : {
        ran(message, "route", {"clear dialog", "route"}),
        ran(message, "talk", {"clear dialog", "talk"}),
        ran(message, "scene", {"mash"}),
        ran(message, "answer", {"answer"}),
        ran(message, "battle", {"battle"}),
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
        ran(wild, "route", {"clear wild", "route"}),
        ran(wild, "battle", {"battle"}),
        ran(trainer, "legs", {"clear trainer", "legs"}),
        ran(trainer, "battle", {"battle"}),
        ran(trainer, "mart", {"game"}),
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
    const std::optional<Pokemon::StoryFailure> failure = game.run("step");
    if (failure || game.trace != std::vector<std::string>{"clear warp", "step"}){
        return "Expected the warp cleared before the step, got " + (failure ? failure->code : std::string("ok"));
    }
    if (game.state.pose){
        return "Expected the pose lost after a warp, got " + format_grid_pose(game.state.pose);
    }
    const std::string error = ran(warp, "mart", {"game"});
    if (!error.empty()){
        return error;
    }
    const std::string scene_error = ran(warp, "scene", {"mash"});
    if (!scene_error.empty()){
        return scene_error;
    }
    return true;
}


//  Console B, 2026-11-13 08:53 (12.03): "still surfing ... didn't land"
//  failed land_north_sand_bar, and its frame was saved nowhere. A failed
//  step's frame is saved to the failures folder, named for the step; one
//  that failed without a frame saves nothing.
UnitTestResult test_failure_frame(){
    const std::string folder = DEBUG_PATH() + "UnitTests/AutoStoryFailures/";
    InterruptLogGuard guard(global_logger_tagged(), folder);
    StoryStep step;
    step.name = "land_north_sand_bar";
    guard.on_failure(step, OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "no frame"));
    if (Filesystem::exists(Filesystem::Path(folder)) && !Filesystem::list_directory(Filesystem::Path(folder)).empty()){
        return "Expected nothing saved for a failure without a frame.";
    }
    guard.on_failure(step, OperationFailedExceptionWithScreenshot(
        ErrorReportMode::NO_ERROR_REPORT, "still surfing", nullptr, ImageRGB32(64, 48)
    ));
    const std::vector<Filesystem::Path> saved = Filesystem::exists(Filesystem::Path(folder))
        ? Filesystem::list_directory(Filesystem::Path(folder))
        : std::vector<Filesystem::Path>();
    for (const Filesystem::Path& path : saved){
        Filesystem::remove(path);
    }
    if (saved.size() != 1 || saved[0].string_slash_normalized().find("land_north_sand_bar.png") == std::string::npos){
        return std::format("Expected one frame named for the step, got {}.", saved.size());
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
    {
        .input = {.name = "a failed step's frame", .body = test_failure_frame},
        .expectation = {.description = "saved to the failures folder, named for the step"},
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

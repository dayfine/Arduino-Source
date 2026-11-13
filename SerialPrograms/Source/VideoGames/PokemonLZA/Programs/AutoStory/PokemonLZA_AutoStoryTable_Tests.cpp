/*  PokemonLZA AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/VisualDetectors/BlackBorderDetector.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryTable_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;

//  The table: ids unique and in order, each row but the new game starts
//  somewhere, and each lints against PLZA's actions in the engine.
UnitTestResult test_story_table_lints(Logger&){
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id <= previous){
            return std::format("{}: repeated or out of order after {}.", row.id, previous);
        }
        previous = row.id;
        if (!row.start && row.id != "01.00"){
            return std::format("{}: no start; every row after the new game starts from a save.", row.id);
        }
        const std::vector<std::string> problems = plza_story_lint(row);
        if (!problems.empty()){
            return problems[0];
        }
    }
    if (story_checkpoints().empty() || story_checkpoints().front().id != "01.00"){
        return "Expected the table to start at 01.00, the new game.";
    }
    return true;
}

//  Each action that is not implemented says what it waits for, is no move's
//  name, and is in a step.
UnitTestResult test_not_implemented_actions(Logger&){
    std::set<std::string> used;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::GAME){
                used.insert(step.action);
            }
        }
    }
    for (const auto& [name, todo] : story_actions_not_implemented()){
        if (todo.empty()){
            return std::format("{}: no TODO.", name);
        }
        if (name == "walk" || name == "approach_starter"){
            return std::format("{}: also a walk's action.", name);
        }
        if (!used.contains(name)){
            return std::format("{}: in no step.", name);
        }
    }
    return true;
}

//  Every autosave row is in the table, and 01.03 saves through the menu
//  (the menu can save from the start of the story).
UnitTestResult test_autosave_rows(Logger&){
    size_t seen = 0;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == "01.03" && !story_row_saves(builtin_story_data(), row)){
            return "Expected 01.03 to save through the menu.";
        }
        if (!story_row_saves(builtin_story_data(), row)){
            seen++;
        }
    }
    if (seen != story_autosave_rows().size()){
        return std::format("Expected {} autosave rows in the table, found {}.", story_autosave_rows().size(), seen);
    }
    return true;
}

//  A run can play every row: none names an action that is not implemented.
UnitTestResult test_every_row_ready(Logger&){
    const size_t ready = story_rows_ready(story_checkpoints());
    if (ready != story_checkpoints().size()){
        return std::format("Expected all {} rows to be ready, {} are.", story_checkpoints().size(), ready);
    }
    return true;
}

//  scene_mash_action() on each kind of frame.
UnitTestResult test_scene_mash_rule(Logger&){
    using Pokemon::ScreenEvent;
    using Pokemon::ScreenEvents;
    struct Case{
        std::string_view name;
        ScreenEvents events;
        SceneMashGoal goal;
        bool text_b;
        SceneMashAction expected;
    };
    const Case cases[] = {
        {"box, A mash", {ScreenEvent::DIALOG_BOX}, SceneMashGoal::OVERWORLD, false, SceneMashAction::PRESS_TEXT},
        {"box, B mash", {ScreenEvent::DIALOG_BOX}, SceneMashGoal::OVERWORLD, true, SceneMashAction::PRESS_TEXT},
        {"choice takes the top", {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, SceneMashGoal::OVERWORLD, true, SceneMashAction::PRESS_A},
        {"choice is the goal", {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, SceneMashGoal::CHOICE, true, SceneMashAction::DONE},
        {"party bar", {ScreenEvent::PLAYER_CAN_MOVE}, SceneMashGoal::OVERWORLD, false, SceneMashAction::DONE},
        {"party bar, not the goal", {ScreenEvent::PLAYER_CAN_MOVE}, SceneMashGoal::BATTLE, false, SceneMashAction::PRESS_A},
        {"black fade waits", {ScreenEvent::FADE_BLACK}, SceneMashGoal::BATTLE, false, SceneMashAction::WAIT},
        {"white fade waits", {ScreenEvent::FADE_WHITE}, SceneMashGoal::OVERWORLD, true, SceneMashAction::WAIT},
        {"battle HUD", {ScreenEvent::BATTLE_STARTED}, SceneMashGoal::BATTLE, false, SceneMashAction::DONE},
        {"card before a battle", {}, SceneMashGoal::BATTLE, false, SceneMashAction::PRESS_A},
        {"quiet, A mash", {}, SceneMashGoal::OVERWORLD, false, SceneMashAction::WAIT},
        {"quiet, B mash (map, Pokedex)", {}, SceneMashGoal::OVERWORLD, true, SceneMashAction::PRESS_B},
        {"prompt alone, A mash", {ScreenEvent::INTERACT_PROMPT}, SceneMashGoal::OVERWORLD, false, SceneMashAction::WAIT},
    };
    for (const Case& item : cases){
        const SceneMashAction got = scene_mash_action(item.events, item.goal, item.text_b);
        if (got != item.expected){
            return std::format("{}: expected action {}, got {}.", item.name, (int)item.expected, (int)got);
        }
    }
    return true;
}


struct AutoStoryTableInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryTableExpectation{
};

//  A black start frame (the previous run stopped in a dark scene) trips the
//  black border check: a run from 01.00, which launches the game, leaves the
//  check to HOME; a run from any later row still refuses on it.
UnitTestResult test_black_start_frame(Logger&){
    ImageRGB32 black(1920, 1080);
    black.fill(0xff000000);
    BlackBorderDetector border;
    if (!border.detect(black)){
        return "Expected the black border check to refuse an all-black frame.";
    }
    for (const StoryCheckpoint& row : story_checkpoints()){
        const bool launches = story_row_launches_game(row);
        if (launches != (row.id == "01.00")){
            return std::format("{}: launches the game: {}; only 01.00 should.", row.id, launches);
        }
    }
    return true;
}

struct AutoStoryTableCase{
    AutoStoryTableInput input;
    AutoStoryTableExpectation expectation;
};

class Test_AutoStoryTable : public UnitTest{
public:
    explicit Test_AutoStoryTable(AutoStoryTableCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryTableCase m_case;
};

} // namespace


const AutoStoryTableCase AUTO_STORY_TABLE_CASES[] = {
    {
        .input = {"Story table: rows lint against PLZA's engine actions", test_story_table_lints},
        .expectation = {},
    },
    {
        .input = {"Story table: actions not implemented say what they wait for", test_not_implemented_actions},
        .expectation = {},
    },
    {
        .input = {"Story table: the autosave rows, and 01.03 saves through the menu", test_autosave_rows},
        .expectation = {},
    },
    {
        .input = {"Story table: every row is ready", test_every_row_ready},
        .expectation = {},
    },
    {
        .input = {"Scene mash: what each frame presses", test_scene_mash_rule},
        .expectation = {},
    },
    {
        .input = {"Start checks: a black start frame refuses every row but 01.00", test_black_start_frame},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryTable(UnitTestDatabase& database){
    for (const AutoStoryTableCase& item : AUTO_STORY_TABLE_CASES){
        database.add<Test_AutoStoryTable>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

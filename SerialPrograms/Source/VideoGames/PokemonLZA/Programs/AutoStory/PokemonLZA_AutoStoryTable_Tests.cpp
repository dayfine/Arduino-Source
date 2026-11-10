/*  PokemonLZA AutoStory - Story table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
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

//  What is wrong with a walk, if anything: it needs legs, each turning at
//  most half a turn either way and walking no negative time, and it must
//  walk somewhere. Empty when there is none.
std::string move_problem(const std::string& name, const StoryMove& move){
    if (move.legs.empty()){
        return std::format("Move {} has no legs.", name);
    }
    bool goes = false;
    for (const StoryWalkLeg& leg : move.legs){
        if (leg.turn < -180 || leg.turn > 180 || leg.walk.count() < 0){
            return std::format("Move {}: leg turns {} deg, walks {} ms.", name, leg.turn, leg.walk.count());
        }
        goes = goes || leg.walk.count() > 0;
    }
    if (!goes){
        return std::format("Move {} walks nowhere.", name);
    }
    if (move.heading && (*move.heading < 0 || *move.heading >= 360)){
        return std::format("Move {} heads {} deg, expected [0, 360).", name, *move.heading);
    }
    return "";
}

//  Every move the table names is known, every move is in a step, and each
//  walks somewhere.
UnitTestResult test_story_moves_known(Logger&){
    std::set<std::string> used;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::GAME && story_moves().contains(step.action)){
                used.insert(step.action);
            }
        }
    }
    for (const auto& [name, move] : story_moves()){
        if (!used.contains(name)){
            return std::format("Move {} is in no step.", name);
        }
        const std::string problem = move_problem(name, move);
        if (!problem.empty()){
            return problem;
        }
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
        if (story_moves().contains(name)){
            return std::format("{}: also a move's name.", name);
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
        if (row.id == "01.03" && !story_row_saves(row)){
            return "Expected 01.03 to save through the menu.";
        }
        if (!story_row_saves(row)){
            seen++;
        }
    }
    if (seen != story_autosave_rows().size()){
        return std::format("Expected {} autosave rows in the table, found {}.", story_autosave_rows().size(), seen);
    }
    return true;
}

//  A run can play from the new game to Wild Zone 1's gate (03.05) with no
//  action that is not implemented.
UnitTestResult test_ready_to_wild_zone_1(Logger&){
    const size_t ready = story_rows_ready();
    if (ready == 0 || story_checkpoints()[ready - 1].id < "03.05"){
        return std::format(
            "Expected rows 01.00-03.05 to be ready, {} are ({}).",
            ready, ready == 0 ? std::string("none") : std::format("up to {}", story_checkpoints()[ready - 1].id)
        );
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

//  Chikorita's walk is recorded and walks somewhere.
UnitTestResult test_starter_moves(Logger&){
    const auto chikorita = starter_moves().find(StarterChoice::CHIKORITA);
    if (chikorita == starter_moves().end()){
        return "Expected the walk to Chikorita.";
    }
    const std::string problem = move_problem("approach_chikorita", chikorita->second);
    if (!problem.empty()){
        return problem;
    }
    return true;
}


struct AutoStoryTableInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryTableExpectation{
};

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
        .input = {"Story table: every move is known and runs", test_story_moves_known},
        .expectation = {},
    },
    {
        .input = {"Story table: actions not implemented say what they wait for", test_not_implemented_actions},
        .expectation = {},
    },
    {
        .input = {"Starters: the walk to Chikorita is recorded", test_starter_moves},
        .expectation = {},
    },
    {
        .input = {"Story table: the autosave rows, and 01.03 saves through the menu", test_autosave_rows},
        .expectation = {},
    },
    {
        .input = {"Story table: ready from the new game to Wild Zone 1", test_ready_to_wild_zone_1},
        .expectation = {},
    },
    {
        .input = {"Scene mash: what each frame presses", test_scene_mash_rule},
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

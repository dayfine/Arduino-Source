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

//  The table: ids unique and in order, each row but the new game's (00.00
//  and 01.00) starts somewhere, and each lints against PLZA's actions in
//  the engine.
UnitTestResult test_story_table_lints(Logger&){
    std::string previous;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id <= previous){
            return std::format("{}: repeated or out of order after {}.", row.id, previous);
        }
        previous = row.id;
        if (!row.start && !story_row_starts_without_save(row)){
            return std::format("{}: no start; every row after the new game starts from a save.", row.id);
        }
        const std::vector<std::string> problems = plza_story_lint(row);
        if (!problems.empty()){
            return problems[0];
        }
    }
    if (story_checkpoints().size() < 2 || story_checkpoints()[0].id != "00.00" || story_checkpoints()[1].id != "01.00"){
        return "Expected the table to start at 00.00 and 01.00, the new game.";
    }
    return true;
}

//  The step `name` of checkpoint `id` in `checkpoints`; null if none.
const StoryStep* find_step(const std::vector<StoryCheckpoint>& checkpoints, std::string_view id, std::string_view name){
    for (const StoryCheckpoint& row : checkpoints){
        for (const StoryStep& step : row.steps){
            if (row.id == id && step.name == name){
                return &step;
            }
        }
    }
    return nullptr;
}

//  `move`'s legs as [turn, ms] pairs, for messages and comparisons.
std::vector<std::pair<int, int64_t>> leg_pairs(const StoryMove& move){
    std::vector<std::pair<int, int64_t>> pairs;
    for (const StoryWalkLeg& leg : move.legs){
        pairs.emplace_back((int)leg.turn, leg.walk.count());
    }
    return pairs;
}

//  Every walk reads as a move with legs (its args passed the lint), and
//  platform_to_plaza reads as recorded, ending on the white fade.
UnitTestResult test_story_walks_read(Logger&){
    size_t walks = 0;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind != StoryStepKind::GAME || step.action != "walk"){
                continue;
            }
            walks++;
            if (story_walk_move(step.args, "legs").legs.empty()){
                return std::format("{} {}: no legs.", row.id, step.name);
            }
        }
    }
    if (walks != 26){
        return std::format("Expected the 26 walks, found {}.", walks);
    }
    const StoryStep* plaza = find_step(story_checkpoints(), "01.01", "platform_to_plaza");
    if (plaza == nullptr){
        return "Expected 01.01 platform_to_plaza.";
    }
    const std::vector<std::pair<int, int64_t>> expected{{0, 4500}, {90, 10000}};
    if (leg_pairs(story_walk_move(plaza->args, "legs")) != expected){
        return "Expected platform_to_plaza's legs [[0, 4500], [90, 10000]].";
    }
    const auto end = plaza->args.find("end");
    if (end == plaza->args.end() || std::get<std::string>(end->second) != "fade_white"){
        return "Expected platform_to_plaza to end on the white fade.";
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

//  03.05 heals at the Pokemon Center and ends there: talk to the nurse,
//  her scene, then the checkpoint's save. Healing writes a save, so the
//  title screen's backup save (Up + B + X) lands on 03.05's end for
//  Main Mission 03's retries (user, 2026-11-13).
UnitTestResult test_mm3_heal_row(Logger&){
    const StoryCheckpoint* heal = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == "03.05"){
            heal = &row;
        }
    }
    if (heal == nullptr || heal->steps.size() < 2){
        return "Expected 03.05, the heal, with at least two steps.";
    }
    const StoryStep& talk = heal->steps[heal->steps.size() - 2];
    const StoryStep& scene = heal->steps.back();
    if (talk.name != "talk_to_nurse" || talk.kind != StoryStepKind::TALK){
        return "Expected 03.05 to talk to the nurse before its last step.";
    }
    if (scene.name != "heal_party" || scene.kind != StoryStepKind::MASH){
        return "Expected 03.05 to end on the nurse's heal_party scene.";
    }
    if (!story_row_saves(builtin_story_data(), *heal)){
        return "Expected 03.05 to save after the heal.";
    }
    const StoryStep* leave = find_step(story_checkpoints(), "03.06", "out_of_pokemon_center");
    if (leave == nullptr || leave->kind != StoryStepKind::GAME || leave->action != "walk"){
        return "Expected 03.06 to walk out of the Pokemon Center.";
    }
    return true;
}

//  The built-in file of checkpoint `id` with its execution's save line
//  replaced by `execution` (the fields after "execution": {), as a dev file.
StoryDevFile dev_with_execution(std::string_view id, std::string_view execution){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name != std::format("plza-{}.plan", id)){
            continue;
        }
        std::string edited = text;
        const size_t at = edited.find("\"save\": \"autosave\"");
        if (at != std::string::npos){
            edited.replace(at, std::string_view("\"save\": \"autosave\"").size(), execution);
        }
        return StoryDevFile{.name = std::format("{}.json", name), .path = std::format("dev/{}.json", name), .text = edited};
    }
    return StoryDevFile{};
}

//  The ParseException load_story_data() throws for `dev`; empty when it
//  loads.
std::string load_problem(const StoryDevFile& dev){
    try{
        load_story_data(std::vector<StoryDevFile>{dev});
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

//  "reload_when_chained" in a checkpoint's execution: only true or "always", and only on
//  a row with a save to reload after a row that ends on the autosave.
UnitTestResult test_reload_when_chained_field(Logger&){
    struct Case{
        std::string_view execution;
        std::string_view problem;
    };
    const Case cases[] = {
        {R"("save": "autosave", "reload_when_chained": "yes")", "true, \"always\" or left out"},
        {R"("save": "autosave", "reload_when_chained": false)", "true, \"always\" or left out"},
        {R"("save": "autosave", "reload_when_chained": true)", "no save to reload"},
        {R"("save": "autosave", "reload_when_chained": "always")", "no save to reload"},
    };
    for (const Case& item : cases){
        const std::string problem = load_problem(dev_with_execution("01.00", item.execution));
        if (problem.find(item.problem) == std::string::npos){
            return std::format("01.00 with {}: expected a problem with \"{}\", got \"{}\".", item.execution, item.problem, problem);
        }
    }
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (story_row_starts_without_save(row) && builtin_story_data().reload_when_chained.contains(row.id)){
            return std::format("{} starts without a save but reloads when chained.", row.id);
        }
    }
    return true;
}

//  01.02 reloads when run after 01.01, whose autosave is 01.02's start;
//  with 01.01 saving through the menu instead, 01.02's reload is refused.
UnitTestResult test_mm1_reload_when_chained(Logger&){
    if (!builtin_story_data().reload_when_chained.contains("01.02")){
        return "Expected 01.02 to reload when chained.";
    }
    const std::string problem = load_problem(dev_with_execution("01.01", R"("save": "menu")"));
    if (problem.find("does not end on the game's autosave") == std::string::npos){
        return std::format("01.01 saving through the menu: expected 01.02's reload refused, got \"{}\".", problem);
    }
    return true;
}

//  03.04 reloads even in a run straight through: its walk to the nurse
//  starts from 03.03's autosave, not from where 03.03's scene leaves the
//  player (console B, 2026-11-13).
UnitTestResult test_0304_reloads_always(Logger&){
    const StoryData& data = builtin_story_data();
    if (!data.reload_always.contains("03.04") || !data.autosaves.contains("03.03")){
        return "Expected 03.04 to always reload, after 03.03's autosave.";
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
//  A scene step's start window: a box, a choice or a battle within it
//  starts the scene; fades and quiet frames alone (02.02's street missed
//  after the Brigitte battle, console B, 2026-11-13) fail at its end, not
//  before.
UnitTestResult test_scene_start_window(Logger&){
    const WallClock start = current_time();
    const auto at = [start](int64_t seconds){
        return start + Milliseconds(seconds * 1000);
    };
    SceneStart missed(start, SCENE_START_WINDOW);
    if (missed.never_started({ScreenEvent::FADE_BLACK}, at(10)) || missed.never_started({}, at(44))){
        return "Expected no failure inside the window.";
    }
    if (!missed.never_started({ScreenEvent::PLAYER_CAN_MOVE}, at(45))){
        return "Expected a scene with only a fade and quiet frames to fail at the window's end.";
    }
    for (const ScreenEvent event : {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX, ScreenEvent::BATTLE_STARTED}){
        SceneStart started(start, SCENE_START_WINDOW);
        if (started.never_started({event}, at(10)) || started.never_started({}, at(100))){
            return "Expected a box, a choice or a battle to start the scene.";
        }
    }
    return true;
}

UnitTestResult test_scene_mash_rule(Logger&){
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

//  Chikorita's walk is recorded in 01.03's approach_starter, and only hers.
UnitTestResult test_starter_moves(Logger&){
    const StoryStep* approach = find_step(story_checkpoints(), "01.03", "approach_starter");
    if (approach == nullptr || approach->action != "approach_starter"){
        return "Expected 01.03 approach_starter.";
    }
    if (approach->args.contains("tepig") || approach->args.contains("totodile")){
        return "Expected only Chikorita's walk.";
    }
    if (!approach->args.contains("chikorita")){
        return "Expected the walk to Chikorita.";
    }
    const std::vector<std::pair<int, int64_t>> expected{{-70, 750}};
    if (leg_pairs(story_walk_move(approach->args, "chikorita")) != expected){
        return "Expected Chikorita's walk [[-70, 750]].";
    }
    return true;
}

//  The built-in text of checkpoint file `name` ("plza-01.02.plan").
std::string builtin_text(std::string_view name){
    for (const auto& [file, text] : autostory_checkpoint_json()){
        if (file == name){
            return text;
        }
    }
    return "";
}

//  `text` with its first `from` replaced by `to`.
std::string replaced(std::string text, std::string_view from, std::string_view to){
    const size_t at = text.find(from);
    if (at != std::string::npos){
        text.replace(at, from.size(), to);
    }
    return text;
}

//  The ParseException message of load_story_data() on the one dev file
//  `text`; empty if it loads.
std::string dev_load_error(std::string_view name, std::string text){
    const StoryDevFile dev{.name = std::string(name), .path = std::format("/dev/{}", name), .text = std::move(text)};
    try{
        load_story_data(std::span<const StoryDevFile>(&dev, 1));
    }catch (ParseException& e){
        return e.message();
    }
    return "";
}

//  A dev file replaces the built-in checkpoint of its name and no other,
//  and is named; a bad one (not JSON, a leg of 3 numbers, a negative walk,
//  an unknown file, another id) fails naming its path.
UnitTestResult test_dev_override(Logger&){
    const std::string chase = builtin_text("plza-01.02.plan");
    const std::string_view legs = "[[-15, 4000], [-75, 3500]";
    if (chase.find(legs) == std::string::npos){
        return "Expected chase_pancham's legs in plza-01.02.plan.json.";
    }
    const StoryDevFile dev{
        .name = "plza-01.02.plan.json", .path = "/dev/plza-01.02.plan.json",
        .text = replaced(chase, legs, "[[-15, 4200], [-75, 3500]"),
    };
    const StoryData data = load_story_data(std::span<const StoryDevFile>(&dev, 1));
    if (data.dev_files.size() != 1 || data.dev_files[0].first != dev.name || data.dev_files[0].second != dev.path){
        return "Expected the dev file to be named with its path.";
    }
    if (data.checkpoints.size() != story_checkpoints().size() || data.autosaves != story_autosave_rows()){
        return "Expected the same rows and autosaves as the built-in story.";
    }
    const StoryStep* tuned = find_step(data.checkpoints, "01.02", "chase_pancham");
    if (tuned == nullptr || leg_pairs(story_walk_move(tuned->args, "legs")).front() != std::pair<int, int64_t>(-15, 4200)){
        return "Expected the dev file's first leg, [-15, 4200].";
    }
    const StoryStep* plaza = find_step(data.checkpoints, "01.01", "platform_to_plaza");
    if (plaza == nullptr || leg_pairs(story_walk_move(plaza->args, "legs")).front() != std::pair<int, int64_t>(0, 4500)){
        return "Expected 01.01 to stay built-in.";
    }
    const std::pair<std::string, std::string> bad[] = {
        {"plza-01.02.plan.json", "{ not json"},
        {"plza-01.02.plan.json", replaced(chase, legs, "[[-15, 4000, 1], [-75, 3500]")},
        {"plza-01.02.plan.json", replaced(chase, legs, "[[-15, -4000], [-75, 3500]")},
        {"plza-09.99.plan.json", chase},
        {"plza-01.02.plan.json", replaced(chase, "\"01.02\"", "\"01.09\"")},
    };
    for (const auto& [name, text] : bad){
        const std::string error = dev_load_error(name, text);
        if (error.find(std::format("/dev/{}", name)) == std::string::npos){
            return std::format("Expected {} to fail naming its path, got \"{}\".", name, error);
        }
    }
    return true;
}


//  A black start frame (the previous run stopped in a dark scene) trips the
//  black border check: a run from 00.00, which launches the game, leaves the
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
        if (launches != (row.id == "00.00")){
            return std::format("{}: launches the game: {}; only 00.00 should.", row.id, launches);
        }
    }
    return true;
}

//  The new game's two starts: 00.00 launches from HOME and plays every
//  choice, 01.00 starts on "Confirm Choices" after the choices made by hand.
//  Neither loads a save, every later row does, and a run from 00.00 skips
//  01.00, which it has played.
UnitTestResult test_new_game_rows(Logger&){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    for (const StoryCheckpoint& row : rows){
        const bool confirms = story_row_confirms_new_game(row);
        if (confirms != (row.id == "01.00")){
            return std::format("{}: starts on Confirm Choices: {}; only 01.00 should.", row.id, confirms);
        }
        const bool no_save = story_row_starts_without_save(row);
        if (no_save != (row.id == "00.00" || row.id == "01.00")){
            return std::format("{}: starts without a save: {}; only 00.00 and 01.00 should.", row.id, no_save);
        }
    }
    const size_t last = rows.size() - 1;
    const auto ids = [](const std::vector<StoryCheckpoint>& run){
        std::string text;
        for (const StoryCheckpoint& row : run){
            text += (text.empty() ? "" : " ") + row.id;
        }
        return text;
    };
    const std::vector<StoryCheckpoint> from_setup = story_run_rows(rows, 0, last);
    if (from_setup.size() != rows.size() - 1 || from_setup[0].id != "00.00" || (from_setup.size() > 1 && from_setup[1].id == "01.00")){
        return std::format("A run from 00.00: expected every row but 01.00, got {}.", ids(from_setup));
    }
    const std::vector<StoryCheckpoint> from_confirm = story_run_rows(rows, 1, last);
    if (from_confirm.size() != rows.size() - 1 || from_confirm[0].id != "01.00"){
        return std::format("A run from 01.00: expected every row from 01.00, got {}.", ids(from_confirm));
    }
    if (!story_run_rows(rows, 1, 0).empty() || !story_run_rows(rows, 0, rows.size()).empty()){
        return "Expected no rows for a start after the end or an end past the table.";
    }
    return true;
}

//  The dev-only rows: 03.07d alone, between 03.07 and 03.08. Developer Mode
//  lists and plays it; a user's run neither lists nor plays it, so 03.07
//  and 03.08 sit next to each other there.
UnitTestResult test_dev_only_rows(Logger&){
    const StoryData& story = builtin_story_data();
    if (story.dev_only != std::set<std::string, std::less<>>{"03.07d"}){
        return "Expected 03.07d to be the only dev-only row.";
    }
    const auto index_of = [](const std::vector<StoryCheckpoint>& rows, std::string_view id) -> size_t{
        for (size_t i = 0; i < rows.size(); i++){
            if (rows[i].id == id){
                return i;
            }
        }
        return rows.size();
    };
    const std::vector<StoryCheckpoint> dev = story_rows_for_mode(story, true);
    const size_t dev_index = index_of(dev, "03.07d");
    if (dev.size() != story.checkpoints.size() || dev_index == dev.size()
        || dev[dev_index - 1].id != "03.07" || dev[dev_index + 1].id != "03.08"
    ){
        return "Developer Mode: expected every row, 03.07d between 03.07 and 03.08.";
    }
    const std::vector<StoryCheckpoint> user = story_rows_for_mode(story, false);
    const size_t user_index = index_of(user, "03.07");
    if (user.size() != story.checkpoints.size() - 1 || index_of(user, "03.07d") != user.size()
        || user_index + 1 >= user.size() || user[user_index + 1].id != "03.08"
    ){
        return "A user's run: expected every row but 03.07d, 03.08 right after 03.07.";
    }
    const std::string text = builtin_text("plza-03.07d.plan");
    const std::string_view field = "\"dev_only\": true";
    if (text.find(field) == std::string::npos){
        return "Expected \"dev_only\": true in plza-03.07d.plan.json.";
    }
    for (const std::string_view bad : {"\"dev_only\": false", "\"dev_only\": \"yes\"", "\"dev_mode\": true"}){
        const std::string error = dev_load_error("plza-03.07d.plan.json", replaced(text, field, bad));
        if (error.find("/dev/plza-03.07d.plan.json") == std::string::npos){
            return std::format("Expected {} to fail naming its file, got \"{}\".", bad, error);
        }
    }
    return true;
}

struct AutoStoryTableCase{
    std::string name;
    struct Input{
        std::function<UnitTestResult(Logger&)> body;
    };
    Input input;
    //  What each body checks is in its own code: these cases are functions.
    struct Expectation{
    };
    Expectation expectation;
};

class Test_AutoStoryTable : public UnitTest{
public:
    explicit Test_AutoStoryTable(AutoStoryTableCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}", test_case.name))
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
        .name = "Story table: rows lint against PLZA's engine actions", .input = {test_story_table_lints},
        .expectation = {},
    },
    {
        .name = "Story table: every walk reads its legs", .input = {test_story_walks_read},
        .expectation = {},
    },
    {
        .name = "Story table: actions not implemented say what they wait for", .input = {test_not_implemented_actions},
        .expectation = {},
    },
    {
        .name = "Starters: the walk to Chikorita is recorded", .input = {test_starter_moves},
        .expectation = {},
    },
    {
        .name = "Dev checkpoints: replace the built-in file of their name, fail naming the file", .input = {test_dev_override},
        .expectation = {},
    },
    {
        .name = "Story table: the autosave rows, and 01.03 saves through the menu", .input = {test_autosave_rows},
        .expectation = {},
    },
    {
        .name = "Story table: 03.05 heals at the Pokemon Center and saves there", .input = {test_mm3_heal_row},
        .expectation = {},
    },
    {
        .name = "Story table: every row is ready", .input = {test_every_row_ready},
        .expectation = {},
    },
    {
        .name = "reload_when_chained: only true, only after an autosave row, never on a new game row", .input = {test_reload_when_chained_field},
        .expectation = {},
    },
    {
        .name = "reload_when_chained: 01.02 after 01.01's autosave", .input = {test_mm1_reload_when_chained},
        .expectation = {},
    },
    {
        .name = "reload_when_chained: \"always\" on 03.04, after 03.03's autosave", .input = {test_0304_reloads_always},
        .expectation = {},
    },
    {
        .name = "Scene mash: a scene that never starts fails at its start window", .input = {test_scene_start_window},
        .expectation = {},
    },
    {
        .name = "Scene mash: what each frame presses", .input = {test_scene_mash_rule},
        .expectation = {},
    },
    {
        .name = "Start checks: a black start frame refuses every row but 00.00", .input = {test_black_start_frame},
        .expectation = {},
    },
    {
        .name = "Start checks: 00.00 and 01.00 start the new game without a save; a run from 00.00 skips 01.00", .input = {test_new_game_rows},
        .expectation = {},
    },
    {
        .name = "Dev-only rows: 03.07d only in Developer Mode, between 03.07 and 03.08", .input = {test_dev_only_rows},
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

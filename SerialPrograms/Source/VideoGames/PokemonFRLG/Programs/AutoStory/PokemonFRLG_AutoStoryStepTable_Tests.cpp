/*  AutoStory Step Table Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's rows run with a recorder for each action: every step of the
 *  story, and every kind with each variant the rows name, must reach the
 *  action the per-kind dispatch they replaced reached.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoveSteps.h"
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryBattleBag.h"
#include "PokemonFRLG_AutoStoryBattleTrack.h"
#include "PokemonFRLG_AutoStoryLeagueParity_Tests.h"
#include "PokemonFRLG_AutoStoryPlannedBattle.h"
#include "PokemonFRLG_AutoStoryPlannedPolicy.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryItemsPocketChange.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRepelStep.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The action StoryExecutor ran for `step` by S11 action name.
StoryActionId expected_dispatch(const StoryStep& step){
    return story_action_id_of(step.action);
}

//  FRLG's rows bound to a recorder: each action adds its name to `trace`.
class FakeGame{
public:
    FakeGame()
        : m_table(make_story_action_table([this](StoryActionId id){ return recorder(id); }))
    {}

    const FrlgActionTable& table() const{
        return m_table;
    }
    std::optional<Pokemon::StoryFailure> run(const StoryStep& step, std::vector<std::string>& trace){
        m_trace = &trace;
        FrlgRunState state;
        const auto it = m_table.find(step.action);
        if (it == m_table.end() || !it->second){
            return frlg_unsupported_step(step);
        }
        return it->second(step, state);
    }

private:
    FrlgStepAction recorder(StoryActionId id){
        return [this, id](const StoryStep&, FrlgRunState&) -> std::optional<Pokemon::StoryFailure>{
            m_trace->emplace_back(story_action_name(id));
            return std::nullopt;
        };
    }

    std::vector<std::string>* m_trace = nullptr;
    FrlgActionTable m_table;
};

//  Why `step` didn't run what expected_dispatch() did, or empty.
std::string parity_error(FakeGame& game, const StoryStep& step){
    std::vector<std::string> trace;
    const std::optional<Pokemon::StoryFailure> failure = game.run(step, trace);
    const StoryActionId expected = expected_dispatch(step);
    const std::string label = std::format("{} ({})", step.action, step.name);
    if (trace != std::vector<std::string>{std::string(story_action_name(expected))} || failure){
        return std::format("{}: expected {}, ran {} action(s){}", label, story_action_name(expected),
            trace.size(), trace.empty() ? "" : ", first " + trace[0]);
    }
    return "";
}

UnitTestResult test_parity_every_kind(){
    FakeGame game;
    for (const std::string& action : frlg_action_names()){
        StoryStep step;
        step.name = "synthetic";
        step.action = action;
        const std::string error = parity_error(game, step);
        if (!error.empty()){
            return error;
        }
    }
    StoryStep unknown;
    unknown.name = "synthetic";
    unknown.action = "no_such_action";
    std::vector<std::string> trace;
    const std::optional<Pokemon::StoryFailure> failure = game.run(unknown, trace);
    if (!trace.empty() || !failure || failure->code != "unsupported_step"){
        return "unknown action: expected unsupported_step.";
    }
    return true;
}

UnitTestResult test_parity_story_steps(){
    FakeGame game;
    for (const StoryCheckpoint& data : story_checkpoints()){
        for (const StoryStep& step : data.steps){
            const std::string error = parity_error(game, step);
            if (!error.empty()){
                return data.id + " " + error;
            }
        }
    }
    return true;
}

UnitTestResult test_every_step_bound(){
    FakeGame game;
    std::string problems;
    for (const StoryCheckpoint& data : story_checkpoints()){
        for (const StoryStep& step : data.steps){
            if (!game.table().contains(step.action)){
                problems += std::format("\n{}/{}: unbound action '{}'", data.id, step.name, step.action);
            }
        }
    }
    if (!problems.empty()){
        return "Expected every step to have an action:" + problems;
    }
    return true;
}


//  Every action a step can name says what it does to the Items pocket: a
//  new one without a row would leave a battle on rows it changed.
UnitTestResult test_every_action_declares_pocket_change(){
    for (const std::string& action : frlg_action_names()){
        if (!items_pocket_change(action)){
            return std::format("\"{}\" has no Items pocket change.", action);
        }
    }
    return true;
}


struct StepTableInput{
    std::string name;
    UnitTestResult (*body)();
};

struct StepTableExpectation{
    std::string_view description;
};

struct StepTableTestCase{
    StepTableInput input;
    StepTableExpectation expectation;
};

class Test_StepTable : public UnitTest{
public:
    explicit Test_StepTable(StepTableTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryStepTable - " + scenario.input.name)
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
    StepTableTestCase m_scenario;
};

const StepTableTestCase STEP_TABLE_CASES[] = {
    {
        .input = {.name = "every kind and variant runs what the per-kind dispatch ran", .body = test_parity_every_kind},
        .expectation = {.description = "table rows match the replaced dispatch for each kind and variant"},
    },
    {
        .input = {.name = "every story step runs what the per-kind dispatch ran", .body = test_parity_story_steps},
        .expectation = {.description = "table rows match the replaced dispatch for each authored step"},
    },
    {
        .input = {.name = "every checkpoint's steps have an action", .body = test_every_step_bound},
        .expectation = {.description = "lint_story_checkpoint finds no unbound step"},
    },
    {
        .input = {.name = "every action declares its Items pocket change", .body = test_every_action_declares_pocket_change},
        .expectation = {.description = "items_pocket_change() has a row for each action spec"},
    },
};

struct PocketChangeInput{
    std::string action;
};
struct PocketChangeExpectation{
    ItemsPocketChange change;
};
struct PocketChangeCase{
    std::string name;
    PocketChangeInput input;
    PocketChangeExpectation expectation;
};

class Test_ItemsPocketChange : public UnitTest{
public:
    explicit Test_ItemsPocketChange(PocketChangeCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryStepTable - Items pocket change: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<ItemsPocketChange> change = items_pocket_change(m_case.input.action);
        if (change != m_case.expectation.change){
            return std::format("Got {}; expected {}.", change ? static_cast<int>(*change) : -1, static_cast<int>(m_case.expectation.change));
        }
        return true;
    }

private:
    PocketChangeCase m_case;
};

//  The places the story changes the Items pocket, each as it is kept.
const PocketChangeCase POCKET_CHANGE_CASES[] = {
    {.name = "a Mart buys and sells", .input = {MART_ACTION}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "Viridian's shop", .input = {"shop"}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a use_item step", .input = {USE_ITEM_VARIANT}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a use_repel step", .input = {USE_REPEL_VARIANT}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a talk: a gift or an item ball", .input = {"talk"}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a battle counts its items", .input = {"battle"}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "a ready step counts its items", .input = {"ready"}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "heal_to_full counts its items", .input = {HEAL_TO_FULL_ACTION}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "count_items reads the pocket", .input = {COUNT_ITEMS_VARIANT}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "a TM taught: the TM Case", .input = {TEACH_ACTION}, .expectation = {ItemsPocketChange::NONE}},
    {.name = "a walk", .input = {"route"}, .expectation = {ItemsPocketChange::NONE}},
};

} // namespace


void add_tests_AutoStoryStepTable(UnitTestDatabase& database){
    for (const auto& scenario : STEP_TABLE_CASES){
        database.add<Test_StepTable>(scenario);
    }
    for (const PocketChangeCase& test_case : POCKET_CHANGE_CASES){
        database.add<Test_ItemsPocketChange>(test_case);
    }
    add_tests_AutoStoryStepInterrupts(database);
    //  The GAME variants with logic of their own.
    add_tests_AutoStoryHideout(database);
    add_tests_AutoStoryFieldMoves(database);
    add_tests_AutoStoryFlyLanding(database);
    add_tests_AutoStoryRooms(database);
    add_tests_AutoStoryItemUse(database);
    add_tests_AutoStoryBattleBag(database);
    add_tests_AutoStoryPlannedPolicy(database);
    add_tests_AutoStoryPlannedBattle(database);
    add_tests_AutoStoryPlannedBattleFake(database);
    add_tests_AutoStoryLeagueParity(database);
    add_tests_AutoStoryBattleTrack(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

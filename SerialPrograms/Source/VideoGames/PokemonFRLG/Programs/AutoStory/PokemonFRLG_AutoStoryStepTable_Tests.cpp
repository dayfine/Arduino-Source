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
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoveSteps.h"
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryBattleBag.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryItemsPocketChange.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::StoryStepKind;

const StoryStepKind ALL_KINDS[] = {
    StoryStepKind::NEW_GAME, StoryStepKind::OPTIONS, StoryStepKind::ROUTE, StoryStepKind::LEGS,
    StoryStepKind::STEP, StoryStepKind::TALK, StoryStepKind::ANSWER, StoryStepKind::MASH,
    StoryStepKind::BATTLE, StoryStepKind::TRAIN, StoryStepKind::HEAL, StoryStepKind::SHOP,
    StoryStepKind::SAVE, StoryStepKind::READY, StoryStepKind::GAME,
};

//  The action StoryExecutor::game() ran for a GAME `step` before the table.
StoryActionId old_game_dispatch(const StoryStep& step){
    if (step.action == CLOSE_TRAINER_CARD){
        return StoryActionId::CLOSE_TRAINER_CARD;
    }
    if (step.action == LEAVE_GIFT_SUMMARY){
        return StoryActionId::LEAVE_GIFT_SUMMARY;
    }
    if (step.action == CUT_TREE_VARIANT){
        return StoryActionId::CUT_TREE;
    }
    if (step.action == PUSH_BOULDER_VARIANT){
        return StoryActionId::PUSH_BOULDER;
    }
    if (step.action == TRASH_CANS_VARIANT){
        return StoryActionId::TRASH_CANS;
    }
    return StoryActionId::GAME;
}

//  The action StoryExecutor ran for `step` before the table: the shared
//  per-kind dispatch (StoryStepExecutor::run()) into the kinds it
//  overrode. Nothing for the kinds it left to the shared executor.
std::optional<StoryActionId> old_dispatch(const StoryStep& step){
    switch (step.kind){
    case StoryStepKind::NEW_GAME:   return StoryActionId::NEW_GAME;
    case StoryStepKind::OPTIONS:    return StoryActionId::OPTIONS;
    case StoryStepKind::ROUTE:      return StoryActionId::ROUTE;
    case StoryStepKind::LEGS:       return StoryActionId::LEGS;
    case StoryStepKind::STEP:       return StoryActionId::STEP;
    case StoryStepKind::TALK:       return StoryActionId::TALK;
    case StoryStepKind::ANSWER:     return StoryActionId::ANSWER;
    case StoryStepKind::MASH:       return StoryActionId::MASH;
    case StoryStepKind::BATTLE:     return StoryActionId::BATTLE;
    case StoryStepKind::HEAL:       return StoryActionId::HEAL;
    case StoryStepKind::SHOP:       return StoryActionId::SHOP;
    case StoryStepKind::READY:      return StoryActionId::READY;
    case StoryStepKind::GAME:       return old_game_dispatch(step);
    case StoryStepKind::TRAIN:
    case StoryStepKind::SAVE:
        return std::nullopt;
    }
    return std::nullopt;
}

//  FRLG's rows bound to a recorder: each action adds its name to `trace`.
class FakeGame{
public:
    FakeGame()
        : m_table(make_story_action_table([this](StoryActionId id){ return recorder(id); }))
        , m_executor(m_table, m_guard)
    {}

    const Pokemon::StoryActionTable& table() const{
        return m_table;
    }
    //  `step` through the table: the outcome, and the actions it ran.
    Pokemon::Outcome run(const StoryStep& step, std::vector<std::string>& trace){
        m_trace = &trace;
        Pokemon::RunState state;
        return m_executor.run(step, state);
    }

private:
    Pokemon::StoryAction recorder(StoryActionId id){
        return {.act = [this, id](const StoryStep&, Pokemon::RunState&){
            m_trace->emplace_back(story_action_name(id));
            return Pokemon::outcome_ok();
        }};
    }

    std::vector<std::string>* m_trace = nullptr;
    Pokemon::StepGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
};

//  Why `step` didn't run what old_dispatch() did, or empty.
std::string parity_error(FakeGame& game, const StoryStep& step){
    std::vector<std::string> trace;
    const Pokemon::Outcome outcome = game.run(step, trace);
    const std::optional<StoryActionId> expected = old_dispatch(step);
    const std::string label = std::format("{} '{}' ({})", Pokemon::story_step_kind_name(step.kind), step.action, step.name);
    if (!expected){
        if (!trace.empty() || outcome.failure != "unsupported_step"){
            return label + ": expected unsupported, ran " + (trace.empty() ? outcome.failure : trace[0]);
        }
        return "";
    }
    if (trace != std::vector<std::string>{std::string(story_action_name(*expected))} || !outcome.succeeded()){
        return std::format("{}: expected {}, ran {} action(s){}", label, story_action_name(*expected),
            trace.size(), trace.empty() ? "" : ", first " + trace[0]);
    }
    return "";
}

//  Every kind, with no variant, an unknown one, and each one a row names.
//  The table finds a row by the variant's first word, where game() compared
//  the whole variant; the rows' variants are one word, as every authored
//  step's is (test_parity_story_steps()).
std::vector<StoryStep> every_kind_and_variant(){
    std::vector<std::string> variants{"", "no_such_variant", "no_such_variant key=1"};
    for (const StoryActionRow& row : story_action_rows()){
        if (!row.variant.empty()){
            variants.push_back(row.variant);
        }
    }
    std::vector<StoryStep> steps;
    for (StoryStepKind kind : ALL_KINDS){
        for (const std::string& variant : variants){
            StoryStep step;
            step.kind = kind;
            step.name = "synthetic";
            step.action = variant;
            steps.push_back(std::move(step));
        }
    }
    return steps;
}

UnitTestResult test_parity_every_kind(){
    FakeGame game;
    for (const StoryStep& step : every_kind_and_variant()){
        const std::string error = parity_error(game, step);
        if (!error.empty()){
            return error;
        }
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
        for (const std::string& problem : Pokemon::lint_story_checkpoint(data, game.table())){
            problems += "\n" + problem;
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
    for (const Pokemon::StoryActionSpec& spec : story_action_specs()){
        if (!items_pocket_change(spec.kind, spec.name)){
            return std::format("{} \"{}\" has no Items pocket change.", Pokemon::story_step_kind_name(spec.kind), spec.name);
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
    StoryStepKind kind;
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
        const std::optional<ItemsPocketChange> change = items_pocket_change(m_case.input.kind, m_case.input.action);
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
    {.name = "a Mart buys and sells", .input = {StoryStepKind::GAME, MART_ACTION}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "Viridian's shop", .input = {StoryStepKind::SHOP, ""}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a use_item step", .input = {StoryStepKind::GAME, USE_ITEM_VARIANT}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a talk: a gift or an item ball", .input = {StoryStepKind::TALK, ""}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a wanderer's talk", .input = {StoryStepKind::TALK, TALK_WANDERER_VARIANT}, .expectation = {ItemsPocketChange::STALE}},
    {.name = "a battle counts its items", .input = {StoryStepKind::BATTLE, "fight"}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "a ready step counts its items", .input = {StoryStepKind::READY, ""}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "heal_to_full counts its items", .input = {StoryStepKind::GAME, HEAL_TO_FULL_ACTION}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "count_items reads the pocket", .input = {StoryStepKind::GAME, COUNT_ITEMS_VARIANT}, .expectation = {ItemsPocketChange::COUNTED}},
    {.name = "a TM taught: the TM Case", .input = {StoryStepKind::GAME, TEACH_ACTION}, .expectation = {ItemsPocketChange::NONE}},
    {.name = "a walk", .input = {StoryStepKind::ROUTE, RUN_VARIANT}, .expectation = {ItemsPocketChange::NONE}},
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
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

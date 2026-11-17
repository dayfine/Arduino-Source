/*  Story Verify Rows Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  run_segment() on a row with a verify block, against a fake game: one
 *  fight from a save, whose tries are scripted (won or lost, the potions
 *  spent). The game's pass predicate is a cap on the potions a try spends;
 *  the row's start check is the potions the save must hold.
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StorySegments.h"
#include "Pokemon_StoryVerifyRows_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr int POTION_CAP = 1;
constexpr int POTIONS_NEEDED = 2;

//  One try's fight.
struct ScriptedTry{
    bool won = true;
    int potions_spent = 0;
};

//  A try won on no potion: a pass.
ScriptedTry win(){
    return ScriptedTry{};
}
ScriptedTry lose(){
    return ScriptedTry{.won = false};
}
//  Won on two potions, over the cap.
ScriptedTry win_over_cap(){
    return ScriptedTry{.potions_spent = 2};
}

std::vector<ScriptedTry> repeated(size_t count, const ScriptedTry& one){
    return std::vector<ScriptedTry>(count, one);
}
std::vector<ScriptedTry> joined(std::vector<ScriptedTry> first, const std::vector<ScriptedTry>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

//  The fake game: the save's potions, the potions now, the tries in order
//  (the last repeats).
struct FakeFight{
    int saved_potions = 0;
    int potions = 0;
    std::vector<ScriptedTry> tries;
    //  The game's postcondition check holds; its pass check reads the game.
    bool postconditions_hold = true;
    bool pass_readable = true;
    int fights = 0;
    int saves = 0;
    int reloads = 0;
    int spent = 0;

    CheckpointSpec spec(StoryVerify verify){
        CheckpointSpec spec;
        spec.id = "20.01";
        spec.title = "Lorelei";
        spec.verify = verify;
        spec.build = [this]{
            Checkpoint checkpoint;
            checkpoint.id = "20.01";
            checkpoint.start.push_back(condition("the potions the fight needs", [this](RunState&){
                return potions >= POTIONS_NEEDED;
            }));
            Event fight;
            fight.name = "fight";
            fight.act = [this](RunState&){
                const ScriptedTry& one = tries[std::min<size_t>(fights, tries.size() - 1)];
                fights++;
                potions -= one.potions_spent;
                spent = one.potions_spent;
                return one.won ? outcome_ok() : outcome_failed("blackout");
            };
            checkpoint.events.push_back(std::move(fight));
            checkpoint.save = [this](RunState&){
                saves++;
                saved_potions = potions;
                return outcome_ok();
            };
            return checkpoint;
        };
        return spec;
    }

    SegmentHooks hooks(std::vector<std::string>& lines){
        SegmentHooks hooks;
        hooks.reload = [this](RunState&){
            reloads++;
            potions = saved_potions;
            return outcome_ok();
        };
        hooks.check_postconditions = [this](const std::string&, RunState&){
            return postconditions_hold ? outcome_ok() : outcome_failed("postconditions", "the bag is short of the next fight's entry");
        };
        hooks.verify_pass = [this](const std::string&, RunState&){
            if (!pass_readable){
                return outcome_failed(std::string(VERIFY_UNREAD), "the bag wasn't read");
            }
            return spent <= POTION_CAP ? outcome_ok() : outcome_failed("over_cap", std::format("{} potions", spent));
        };
        hooks.log = [&lines](const std::string& line){ lines.push_back(line); };
        return hooks;
    }
};


struct VerifyRowCase{
    struct Input{
        StoryVerify verify = {};
        int potions = POTIONS_NEEDED;
        std::vector<ScriptedTry> tries = {};
        bool postconditions_hold = true;
        bool pass_readable = true;
    };
    struct Expectation{
        bool completed = false;
        //  The segment's failure; empty when it completed.
        std::string failure = {};
        int fights = 0;
        int saves = 0;
        int reloads = 0;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_StoryVerifyRow : public UnitTest{
public:
    explicit Test_StoryVerifyRow(VerifyRowCase test_case)
        : UnitTest(std::format("Pokemon::StoryVerifyRows - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeFight game{
            .saved_potions = m_case.input.potions, .potions = m_case.input.potions, .tries = m_case.input.tries,
            .postconditions_hold = m_case.input.postconditions_hold, .pass_readable = m_case.input.pass_readable,
        };
        Segment segment;
        segment.id = "20";
        segment.checkpoints.push_back(game.spec(m_case.input.verify));
        std::vector<std::string> lines;
        SegmentHooks hooks = game.hooks(lines);
        RunState state;
        const SegmentReport report = run_segment({}, SegmentPolicy{}, segment, hooks, state);
        const VerifyRowCase::Expectation& want = m_case.expectation;
        const std::string failure = report.completed ? "" : report.failure.failure;
        if (report.completed != want.completed || failure != want.failure
            || game.fights != want.fights || game.saves != want.saves || game.reloads != want.reloads
        ){
            std::string log;
            for (const std::string& line : lines){
                log += std::format("\n    {}", line);
            }
            return std::format(
                "{} \"{}\", fights {}, saves {}, reloads {}; expected {} \"{}\", {}, {}, {}:{}",
                report.completed ? "completed" : "failed", failure, game.fights, game.saves, game.reloads,
                want.completed ? "completed" : "failed", want.failure, want.fights, want.saves, want.reloads, log
            );
        }
        return true;
    }

private:
    VerifyRowCase m_case;
};

const VerifyRowCase VERIFY_ROW_CASES[] = {
    {
        .name = "ten passes: saved on the first phase 2 try",
        .input = {.tries = {win()}},
        .expectation = {.completed = true, .fights = 11, .saves = 1, .reloads = 10},
    },
    {
        .name = "four passes in phase 1, under min_pass 5: stop, no save",
        .input = {.tries = joined(repeated(4, win()), {lose()})},
        .expectation = {.failure = "verify_below_min_pass", .fights = 10, .saves = 0, .reloads = 9},
    },
    {
        .name = "phase 2 passes on its third try: saved once",
        .input = {.tries = joined(joined(repeated(5, win()), repeated(7, lose())), {win()})},
        .expectation = {.completed = true, .fights = 13, .saves = 1, .reloads = 12},
    },
    {
        .name = "no pass in the reset budget of 5: stop, no save",
        .input = {.tries = joined(repeated(5, win()), {lose()})},
        .expectation = {.failure = "verify_out_of_budget", .fights = 15, .saves = 0, .reloads = 14},
    },
    {
        .name = "wins over the game's cap don't pass: never saved",
        .input = {.tries = joined(repeated(5, win()), {win_over_cap()})},
        .expectation = {.failure = "verify_out_of_budget", .fights = 15, .saves = 0, .reloads = 14},
    },
    {
        .name = "the save is short of the start check: stop before fighting",
        .input = {.potions = POTIONS_NEEDED - 1, .tries = {win()}},
        .expectation = {.failure = "start_failed", .fights = 0, .saves = 0, .reloads = 0},
    },
    {
        .name = "a won try whose postcondition check fails right before the save: never saved",
        .input = {.verify = {.tries = 2, .min_pass = 0, .reset_budget = 2}, .tries = {win()}, .postconditions_hold = false},
        .expectation = {.failure = "verify_out_of_budget", .fights = 4, .saves = 0, .reloads = 3},
    },
    {
        .name = "the pass check can't read the game: no save, the loop stops at once",
        .input = {.tries = {win()}, .pass_readable = false},
        .expectation = {.failure = "postcondition_unread", .fights = 1, .saves = 0, .reloads = 0},
    },
    {
        .name = "min_pass 0 of 2: straight to phase 2",
        .input = {.verify = {.tries = 2, .min_pass = 0, .reset_budget = 1}, .tries = {lose(), lose(), win()}},
        .expectation = {.completed = true, .fights = 3, .saves = 1, .reloads = 2},
    },
};

} // namespace


void add_tests_StoryVerifyRows(UnitTestDatabase& database){
    for (const VerifyRowCase& test_case : VERIFY_ROW_CASES){
        database.add<Test_StoryVerifyRow>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

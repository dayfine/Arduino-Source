/*  AutoStory Evolution Wait Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle loop after its last turn, on scripted readings
 *  (Pokemon_ScriptedReadings.h): EvolutionHold::wait_out() for each hold, B
 *  otherwise, and START once the battle is gone, as between_turns(),
 *  battle_gone() and no_battle_screen() in PokemonFRLG_AutoStoryBattle.cpp
 *  run them. Regression for 16.05 (console B, 2026-11-15): black at Silph
 *  Co.'s map edge read as the text box, three holds, then a stop.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_ScriptedReadings.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBattleTurns.h"
#include "PokemonFRLG_AutoStoryEvolutionWait_Tests.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

using namespace std::chrono_literals;
using Pokemon::ScriptedReading;
using Pokemon::ScriptedReadings;

const WallClock T0 = WallClock() + std::chrono::hours(1);
//  As the battle loop's.
constexpr Milliseconds POLL = 300ms;
//  How long a run may take before it counts as stuck.
constexpr Milliseconds RUN_LIMIT = 120000ms;

//  "<foe> fainted!" with its HP box.
const BattleTextScreen DEFEATED{.text_box = true, .hp_box = true, .arrow = true};
//  The screen blacked out between the battle and the map, or the black at
//  a map's edge as the text box's frame check reads it: nothing.
const BattleTextScreen BLACK{};
const BattleTextScreen OVERWORLD{};
//  "What? SQUIRTLE is evolving!"
const BattleTextScreen EVOLVING{.text_box = true};
//  "Congratulations! ..." with its arrow.
const BattleTextScreen CONGRATULATIONS{.text_box = true, .arrow = true};
//  The black at a map's edge as the blue-only check read it before the
//  frame check: the text box with nothing else.
const BattleTextScreen EDGE_AS_BOX{.text_box = true};

//  What the loop did after the last turn.
struct ExitRun{
    std::vector<Milliseconds> holds;
    size_t b_presses = 0;
    //  When START opened the menu, from the start; none if it never did.
    std::optional<Milliseconds> left = std::nullopt;
};

//  The loop from the last turn (one that attacked) on `script`, START
//  opening the menu from `overworld_from` on.
ExitRun run_exit(std::vector<ScriptedReading<BattleTextScreen>> script, Milliseconds overworld_from, Supplies& supplies){
    ScriptedReadings<BattleTextScreen> source(std::move(script), T0);
    EvolutionHold hold(supplies.lead);
    hold.turn_taken(TurnChoice::MOVE);
    BattleEndWatch watch;
    ExitRun run;
    while (source.elapsed() < RUN_LIMIT){
        const std::optional<BattleTextScreen> screen = source.read();
        if (screen && hold.holds(*screen)){
            const Milliseconds held = hold.wait_out(POLL, source);
            hold.held(held, false);
            run.holds.push_back(held);
            continue;
        }
        const bool battle = screen && (screen->hp_box || screen->yes_no);
        watch.seen(battle ? BattleSeen::BATTLE : BattleSeen::NONE, source.now());
        if (watch.check_overworld(source.now()) && source.elapsed() >= overworld_from){
            run.left = source.elapsed();
            return run;
        }
        run.b_presses++;
        source.wait(POLL);
    }
    return run;
}


struct EvolutionWaitInput{
    std::vector<ScriptedReading<BattleTextScreen>> script;
    //  From when START opens the menu.
    Milliseconds overworld_from = Milliseconds(0);
};

//  A hold's length, within a poll or two.
struct HoldRange{
    Milliseconds min = Milliseconds(0);
    Milliseconds max = Milliseconds(0);
};

struct EvolutionWaitExpectation{
    std::vector<HoldRange> holds;
    //  START opens the menu by then, from the start.
    Milliseconds left_by = Milliseconds(0);
    //  Whether the loop took the lead for evolved; none: not checked.
    std::optional<bool> evolved = std::nullopt;
};

struct EvolutionWaitCase{
    std::string name;
    EvolutionWaitInput input;
    EvolutionWaitExpectation expectation;
};

UnitTestResult check_run(const ExitRun& run, const EvolutionWaitExpectation& expected, const Supplies& supplies){
    if (run.holds.size() != expected.holds.size()){
        return std::format("expected {} holds, got {}", expected.holds.size(), run.holds.size());
    }
    for (size_t c = 0; c < run.holds.size(); c++){
        if (run.holds[c] < expected.holds[c].min || run.holds[c] > expected.holds[c].max){
            return std::format(
                "hold {}: {} ms, expected {} to {} ms",
                c + 1, run.holds[c].count(), expected.holds[c].min.count(), expected.holds[c].max.count()
            );
        }
    }
    if (!run.left){
        return std::format("START never opened the menu in {} s ({} B presses)", RUN_LIMIT.count() / 1000, run.b_presses);
    }
    if (*run.left > expected.left_by){
        return std::format("START opened the menu at {} ms, expected by {} ms", run.left->count(), expected.left_by.count());
    }
    const bool evolved = supplies.lead.species != "squirtle";
    if (expected.evolved && evolved != *expected.evolved){
        return std::format("expected the lead {}evolved, it is {}", *expected.evolved ? "" : "not ", supplies.lead.species);
    }
    return true;
}

class Test_EvolutionWait : public UnitTest{
public:
    explicit Test_EvolutionWait(EvolutionWaitCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryEvolutionWait - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        Supplies supplies;
        const ExitRun run = run_exit(m_case.input.script, m_case.input.overworld_from, supplies);
        return check_run(run, m_case.expectation, supplies);
    }

private:
    EvolutionWaitCase m_case;
};

const EvolutionWaitCase EVOLUTION_WAIT_CASES[] = {
    {
        .name = "text, black, overworld: B, then START",
        .input = {
            .script = {{1500ms, DEFEATED}, {1000ms, BLACK}, {60000ms, OVERWORLD}},
            .overworld_from = 2500ms,
        },
        .expectation = {.holds = {}, .left_by = 5000ms, .evolved = false},
    },
    {
        .name = "text, black, evolution: held through it, then START",
        .input = {
            .script = {
                {1500ms, DEFEATED}, {600ms, BLACK}, {9000ms, EVOLVING},
                {1500ms, CONGRATULATIONS}, {600ms, BLACK}, {60000ms, OVERWORLD},
            },
            .overworld_from = 13200ms,
        },
        .expectation = {.holds = {{8700ms, 9600ms}}, .left_by = 16500ms, .evolved = true},
    },
    {
        .name = "a look with no frame holds on through an evolution",
        .input = {
            .script = {
                {1500ms, DEFEATED}, {3000ms, EVOLVING}, {2000ms, std::nullopt}, {3000ms, EVOLVING},
                {1500ms, CONGRATULATIONS}, {60000ms, OVERWORLD},
            },
            .overworld_from = 11000ms,
        },
        .expectation = {.holds = {{7700ms, 8600ms}}, .left_by = 14500ms, .evolved = true},
    },
    {
        //  16.05: the map was there all along, black at its edge.
        .name = "black at a map's edge read as the text box: one full hold, then START",
        .input = {
            .script = {{1500ms, DEFEATED}, {120000ms, EDGE_AS_BOX}},
            .overworld_from = 1500ms,
        },
        .expectation = {
            .holds = {{EvolutionHold::MAX_HOLD, EvolutionHold::MAX_HOLD + POLL}},
            .left_by = 1500ms + EvolutionHold::MAX_HOLD + 5000ms,
            //  A full hold counts as an evolution; not this test's point.
            .evolved = std::nullopt,
        },
    },
    {
        .name = "black at a map's edge as the frame check reads it: no hold",
        .input = {
            .script = {{1500ms, DEFEATED}, {120000ms, BLACK}},
            .overworld_from = 1500ms,
        },
        .expectation = {.holds = {}, .left_by = 5000ms, .evolved = false},
    },
};

} // namespace


void add_tests_AutoStoryEvolutionWait(UnitTestDatabase& database){
    for (const EvolutionWaitCase& test_case : EVOLUTION_WAIT_CASES){
        database.add<Test_EvolutionWait>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

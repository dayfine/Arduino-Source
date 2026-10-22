/*  Pokemon SV BBQ Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <stdexcept>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQTests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


namespace{

struct TeraEncounterScenario{
    std::string name;
    bool shiny = false;
    int encounter_watcher_result;       //  0: battle menu, 1: audio, -1: route finished first
    bool battle_menu_detected = false;  //  The route's own wait saw the battle menu.
    TeraEncounterAction expected;
};

class Test_TeraEncounterAction : public UnitTest{
public:
    Test_TeraEncounterAction(TeraEncounterScenario scenario)
        : UnitTest("PokemonSV::TeraEncounterAction - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TeraEncounterAction actual = tera_encounter_action(
            m_scenario.shiny, m_scenario.encounter_watcher_result, m_scenario.battle_menu_detected
        );
        if (actual != m_scenario.expected){
            return "Expected action " + std::to_string((int)m_scenario.expected)
                + ", got " + std::to_string((int)actual) + ".";
        }
        return true;
    }

private:
    const TeraEncounterScenario m_scenario;
};

}

static void add_tests_TeraEncounterAction(UnitTestDatabase& database){
    using enum TeraEncounterAction;
    const std::vector<TeraEncounterScenario> scenarios{
        {.name = "timeout recovers without battle", .encounter_watcher_result = -1, .expected = Recover},
        {.name = "audio alone is not entry", .encounter_watcher_result = 1, .expected = Recover},
        {.name = "unknown callback is not entry", .encounter_watcher_result = 2, .expected = Recover},
        {.name = "encounter watcher menu battles", .encounter_watcher_result = 0, .expected = Battle},
        {.name = "route wait menu battles", .encounter_watcher_result = -1, .battle_menu_detected = true, .expected = Battle},
        {.name = "audio then route wait menu battles", .encounter_watcher_result = 1, .battle_menu_detected = true, .expected = Battle},
        {.name = "both menus battle", .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = Battle},
        {.name = "shiny stops without entry", .shiny = true, .encounter_watcher_result = -1, .expected = StopForShiny},
        {.name = "shiny stops before battle", .shiny = true, .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = StopForShiny},
    };
    for (const TeraEncounterScenario& scenario : scenarios){
        database.add<Test_TeraEncounterAction>(scenario);
    }
}


namespace{

// Keeps every line logged, so a test can check the pass log.
class RecordingLogger : public Logger{
public:
    std::vector<std::string> lines;

    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
};

std::string join(const std::vector<std::string>& items, const std::string& separator){
    std::string ret;
    for (const std::string& item : items){
        if (!ret.empty()){
            ret += separator;
        }
        ret += item;
    }
    return ret;
}

// Auto Battle steps that report scripted kills, and charge each step the
// seconds measured on one live pass (bbq-m5 log, 2026-10-21).
class ScriptedAutoBattle : public BBQAutoBattleActions{
public:
    static constexpr int FLY_SECONDS = 20;      // Fly to Chargestone, wait 3 s, walk in (~19.5 s).
    static constexpr int CLEAR_SECONDS = 26;    // Battling, then 6 s with no kill.
    static constexpr int CLEAR_TAIL_SECONDS = 6;    // Skipped when the clear stops at `stop_at_kills`.
    static constexpr int RETURN_SECONDS = 28;   // Fly back to Central Plaza.
    static constexpr int HEAL_SECONDS = 28;     // Walk to the nurse and heal (14 s), then re-fly (14 s).

    // Kills heard in each clear, in turn. Running past the end throws, which
    // also stops a loop that would never end.
    std::vector<uint64_t> kills_per_clear;

    std::vector<std::string> steps;
    int seconds = 0;

    virtual uint64_t kills() const override{
        return m_kills;
    }
    virtual WallClock now() const override{
        return WallClock{} + std::chrono::seconds(seconds);
    }
    virtual void fly_to_battle_area() override{
        steps.push_back("fly");
        seconds += FLY_SECONDS;
    }
    virtual std::optional<double> clear(uint64_t stop_at_kills) override{
        if (m_clears >= kills_per_clear.size()){
            throw std::runtime_error("Cleared more often than scripted.");
        }
        steps.push_back("clear");
        seconds += CLEAR_SECONDS;
        m_kills += kills_per_clear[m_clears++];
        if (m_kills >= stop_at_kills){
            seconds -= CLEAR_TAIL_SECONDS;
        }
        return std::nullopt;
    }
    virtual void return_to_plaza() override{
        steps.push_back("return");
        seconds += RETURN_SECONDS;
    }
    virtual void heal() override{
        steps.push_back("heal");
        seconds += HEAL_SECONDS;
    }

private:
    uint64_t m_kills = 0;
    size_t m_clears = 0;
};

// One pass as it shows in the expected steps.
const std::string PASS = "fly clear return heal";

std::string passes(int count){
    std::vector<std::string> ret(count, PASS);
    return join(ret, " ");
}

struct BBQAutoBattleScenario{
    std::string name;
    uint64_t kill_target = 10;
    std::vector<uint64_t> kills_per_clear;
    bool expected_endless = false;  // Runs past the script without stopping.
    std::string expected_steps;
    int expected_seconds = 0;
    std::vector<std::string> expected_log = {}; // Checked when not empty.
};

class Test_BBQAutoBattle : public UnitTest{
public:
    Test_BBQAutoBattle(BBQAutoBattleScenario scenario)
        : UnitTest("PokemonSV::BBQAutoBattle - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedAutoBattle actions;
        actions.kills_per_clear = m_scenario.kills_per_clear;

        RecordingLogger logger;
        BBQAutoBattlePolicy policy;
        policy.kill_target = m_scenario.kill_target;
        bool endless = false;
        try{
            run_auto_battle(logger, actions, policy);
        }catch (const std::runtime_error&){
            endless = true;
        }
        if (endless != m_scenario.expected_endless){
            return endless
                ? "Expected the loop to stop, but it cleared more often than scripted."
                : "Expected the loop to run past the script, but it stopped.";
        }
        const std::string steps = join(actions.steps, " ");
        if (steps != m_scenario.expected_steps){
            return "Expected steps: " + m_scenario.expected_steps + "\nActual steps:   " + steps;
        }
        if (actions.seconds != m_scenario.expected_seconds){
            return "Expected " + std::to_string(m_scenario.expected_seconds) + " seconds, got "
                + std::to_string(actions.seconds);
        }
        if (!m_scenario.expected_log.empty() && logger.lines != m_scenario.expected_log){
            return "Expected log:\n" + join(m_scenario.expected_log, "\n")
                + "\nActual log:\n" + join(logger.lines, "\n");
        }
        return true;
    }

private:
    const BBQAutoBattleScenario m_scenario;
};

}

static void add_tests_BBQAutoBattle(UnitTestDatabase& database){
    constexpr int PASS_SECONDS = ScriptedAutoBattle::FLY_SECONDS + ScriptedAutoBattle::CLEAR_SECONDS
        + ScriptedAutoBattle::RETURN_SECONDS + ScriptedAutoBattle::HEAL_SECONDS;
    const BBQAutoBattleScenario scenarios[] = {
        {
            // The final pass still heals and re-flies, though the quest is done.
            .name = "auto battle 10 at 2 kills per pass",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .expected_steps = passes(5),
            .expected_seconds = 5 * PASS_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP ?, 46 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP ?, 148 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP ?, 250 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP ?, 352 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP ?, 454 s elapsed. Target reached: return and heal.",
            },
        },
        {
            .name = "auto battle 30 at 3 kills per pass",
            .kill_target = 30,
            .kills_per_clear = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
            .expected_steps = passes(10),
            .expected_seconds = 10 * PASS_SECONDS,
        },
        {
            // A clear keeps going while kills are heard, past the target.
            .name = "kills past the target",
            .kills_per_clear = {4, 4, 4},
            .expected_steps = passes(3),
            .expected_seconds = 3 * PASS_SECONDS,
        },
        {
            .name = "passes with no kill go again",
            .kills_per_clear = {0, 0, 5, 0, 5},
            .expected_steps = passes(5),
            .expected_seconds = 5 * PASS_SECONDS,
        },
        {
            // Nothing bounds the loop: with no kills it never ends.
            .name = "no kills never stops",
            .kills_per_clear = {0, 0, 0, 0, 0, 0},
            .expected_endless = true,
            .expected_steps = passes(6) + " fly",
            .expected_seconds = 6 * PASS_SECONDS + ScriptedAutoBattle::FLY_SECONDS,
        },
    };
    for (const BBQAutoBattleScenario& scenario : scenarios){
        database.add<Test_BBQAutoBattle>(scenario);
    }
}


void add_tests_BBQ(UnitTestDatabase& database){
    add_tests_TeraEncounterAction(database);
    add_tests_BBQAutoBattle(database);
}



}
}
}

/*  Pokemon SV BBQ Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <stdexcept>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryPointsReader.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQuests.h"
#include "PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQTests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


namespace{

class Test_BBQuestsSlugs : public UnitTest{
public:
    Test_BBQuestsSlugs()
        : UnitTest("PokemonSV::BBQuests - slug round trip")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (int c = 0; c < (int)BBQuests::UnableToDetect; c++){
            const BBQuests quest = (BBQuests)c;
            const std::string& slug = BBQuests_enum_to_string(quest);
            if (slug.empty()){
                return "Quest " + std::to_string(c) + " has no slug.";
            }
            if (BBQuests_string_to_enum(slug) != quest){
                return "Slug " + slug + " does not map back to quest " + std::to_string(c) + ".";
            }
        }
        if (!BBQuests_enum_to_string(BBQuests::UnableToDetect).empty()){
            return "UnableToDetect must map to the empty slug.";
        }
        if (BBQuests_string_to_enum("not-a-quest") != BBQuests::UnableToDetect){
            return "An unrecognized slug must map to UnableToDetect.";
        }
        return true;
    }
};

class Test_BlueberryPoints : public UnitTest{
public:
    Test_BlueberryPoints(const std::string& image, int expected, size_t width, size_t height)
        : UnitTest("PokemonSV::BlueberryPoints - " + image + " - " + std::to_string(width))
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_expected(expected), m_width(width), m_height(height)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        ImageRGB32 reference(m_image);
        ImageRGB32 image = reference.scale_to(m_width, m_height);
        const int actual = read_BP_from_image(logger, image);
        if (actual != m_expected){
            return "Expected " + std::to_string(m_expected) + " BP, read " + std::to_string(actual);
        }
        return true;
    }

private:
    const std::string m_image;
    const int m_expected;
    const size_t m_width, m_height;
};

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
// A pass that returns without healing.
const std::string LAST_PASS = "fly clear return";

std::string passes(int count){
    std::vector<std::string> ret(count, PASS);
    return join(ret, " ");
}

std::string auto_battle_result_to_string(BBQAutoBattleResult result){
    switch (result){
    case BBQAutoBattleResult::TargetReached:    return "TargetReached";
    case BBQAutoBattleResult::NoKills:          return "NoKills";
    case BBQAutoBattleResult::TimeLimit:        return "TimeLimit";
    }
    return "?";
}

struct BBQAutoBattleScenario{
    std::string name;
    uint64_t kill_target = 10;
    int time_limit_minutes = 35;
    std::vector<uint64_t> kills_per_clear;
    BBQAutoBattleResult expected_result = BBQAutoBattleResult::TargetReached;
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
        policy.time_limit = std::chrono::minutes(m_scenario.time_limit_minutes);
        BBQAutoBattleResult result;
        try{
            result = run_auto_battle(logger, actions, policy);
        }catch (const std::runtime_error&){
            return "Expected the loop to stop, but it cleared more often than scripted.";
        }
        if (result != m_scenario.expected_result){
            return "Expected " + auto_battle_result_to_string(m_scenario.expected_result)
                + ", got " + auto_battle_result_to_string(result);
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
    constexpr int LAST_PASS_SAVED_SECONDS = ScriptedAutoBattle::CLEAR_TAIL_SECONDS + ScriptedAutoBattle::HEAL_SECONDS;
    const BBQAutoBattleScenario scenarios[] = {
        {
            // The final pass stops clearing at the target, and skips the heal and re-fly.
            .name = "auto battle 10 at 2 kills per pass",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .expected_steps = passes(4) + " " + LAST_PASS,
            .expected_seconds = 5 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP ?, 46 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP ?, 148 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP ?, 250 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP ?, 352 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP ?, 448 s elapsed. Target reached: return without healing.",
            },
        },
        {
            .name = "auto battle 30 at 3 kills per pass",
            .kill_target = 30,
            .kills_per_clear = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
            .expected_steps = passes(9) + " " + LAST_PASS,
            .expected_seconds = 10 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
        },
        {
            // Kills heard before the clear can stop still count.
            .name = "kills past the target",
            .kills_per_clear = {4, 4, 4},
            .expected_steps = passes(2) + " " + LAST_PASS,
            .expected_seconds = 3 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
        },
        {
            .name = "passes with no kill go again",
            .kills_per_clear = {0, 0, 5, 0, 5},
            .expected_steps = passes(4) + " " + LAST_PASS,
            .expected_seconds = 5 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
        },
        {
            .name = "gives up after 3 passes with no kill",
            .kills_per_clear = {0, 0, 0},
            .expected_result = BBQAutoBattleResult::NoKills,
            .expected_steps = passes(2) + " " + LAST_PASS,
            .expected_seconds = 3 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 0 kills, 0/10 in total, HP ?, 46 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 2: 0 kills, 0/10 in total, HP ?, 148 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 3: 0 kills, 0/10 in total, HP ?, 250 s elapsed. No kill in 3 passes in a row: return and give up.",
            },
        },
        {
            // Only passes in a row count.
            .name = "a kill resets the empty passes",
            .kills_per_clear = {0, 0, 1, 0, 0, 0},
            .expected_result = BBQAutoBattleResult::NoKills,
            .expected_steps = passes(5) + " " + LAST_PASS,
            .expected_seconds = 6 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            // The pass that ends past the limit is the last.
            .name = "gives up at the time limit",
            .time_limit_minutes = 5,
            .kills_per_clear = {1, 1, 1, 1},
            .expected_result = BBQAutoBattleResult::TimeLimit,
            .expected_steps = passes(3) + " " + LAST_PASS,
            .expected_seconds = 4 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 1 kills, 1/10 in total, HP ?, 46 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 2: 1 kills, 2/10 in total, HP ?, 148 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 3: 1 kills, 3/10 in total, HP ?, 250 s elapsed. Below the target: return, heal and go again.",
                "Auto Battle pass 4: 1 kills, 4/10 in total, HP ?, 352 s elapsed. Time limit of 5 min reached: return and give up.",
            },
        },
        {
            // Reaching the target counts as reached, even past the time limit.
            .name = "target reached past the time limit",
            .time_limit_minutes = 5,
            .kills_per_clear = {3, 3, 3, 3},
            .expected_steps = passes(3) + " " + LAST_PASS,
            .expected_seconds = 4 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
        },
    };
    for (const BBQAutoBattleScenario& scenario : scenarios){
        database.add<Test_BBQAutoBattle>(scenario);
    }
}


void add_tests_BBQ(UnitTestDatabase& database){
    database.add<Test_BBQuestsSlugs>();

    // Full-frame references, in the CommandLineTests repo: they exercise the production
    // crop, filter and OCR together.
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1280, 720);

    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1280, 720);

    add_tests_TeraEncounterAction(database);
    add_tests_BBQAutoBattle(database);
}



}
}
}

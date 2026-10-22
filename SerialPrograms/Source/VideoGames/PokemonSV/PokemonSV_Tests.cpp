/*  Pokemon SV Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <set>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/PokemonSV_WhiteButtonDetector.h"
#include "Inference/Boxes/PokemonSV_BoxDetection.h"
#include "Inference/Boxes/PokemonSV_BoxEggDetector.h"
#include "Inference/Map/PokemonSV_MapDetector.h"
#include "Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "Inference/Picnics/PokemonSV_PicnicDetector.h"
#include "Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "Inference/Tera/PokemonSV_TeraSilhouetteReader.h"
#include "Inference/Tera/PokemonSV_TeraTypeReader.h"
#include "Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "Inference/Map/PokemonSV_FastTravelDetector.h"
#include "Inference/Map/PokemonSV_MapMenuDetector.h"
#include "Inference/Map/PokemonSV_MapPokeCenterIconDetector.h"
#include "Inference/PokemonSV_ESPEmotionDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichIngredientDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichPlateDetector.h"
#include "Inference/Overworld/PokemonSV_LetsGoKillDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichRecipeDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichHandDetector.h"
#include "Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "Programs/Farming/PokemonSV_BBQSoloFarmer.h"
#include "Programs/Farming/PokemonSV_BlueberryCatchPhoto.h"
#include "PokemonSV_Tests.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Inference/PokemonSV_BlueberryPointsReader.h"
#include "Programs/Farming/PokemonSV_BBQPointsLedger.h"

#include <optional>
#include <stdexcept>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

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
    static constexpr int CLEAR_TAIL_SECONDS = 6;    // Skipped when the clear stops at the target.
    static constexpr int RETURN_SECONDS = 28;   // Fly back to Central Plaza.
    static constexpr int HEAL_SECONDS = 28;     // Walk to the nurse and heal (14 s), then re-fly (14 s).

    // Kills heard in each clear, in turn. Running past the end throws, which
    // also stops a loop that would never end.
    std::vector<uint64_t> kills_per_clear;
    // The lead's HP read in each clear, in turn. Past the end, it is unknown.
    std::vector<std::optional<double>> hp_per_clear;

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
        const std::optional<double> hp = m_clears < hp_per_clear.size() ? hp_per_clear[m_clears] : std::nullopt;
        m_kills += kills_per_clear[m_clears++];
        if (m_kills >= stop_at_kills){
            seconds -= CLEAR_TAIL_SECONDS;
        }
        return hp;
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
// The last pass, or one with enough HP: it returns without healing.
const std::string LAST_PASS = "fly clear return";

std::string passes(int count, const std::string& pass = PASS){
    std::vector<std::string> ret(count, pass);
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
    std::vector<std::optional<double>> hp_per_clear;    // Unknown when not given.
    int heal_below_hp_percent = 50;
    BBQAutoBattleResult expected_result = BBQAutoBattleResult::TargetReached;
    std::string expected_steps;
    int expected_seconds = 0;
    std::vector<std::string> expected_log;  // Checked when not empty.
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
        actions.hp_per_clear = m_scenario.hp_per_clear;

        RecordingLogger logger;
        BBQAutoBattlePolicy policy;
        policy.kill_target = m_scenario.kill_target;
        policy.time_limit = std::chrono::minutes(m_scenario.time_limit_minutes);
        policy.heal_below_hp_percent = (uint8_t)m_scenario.heal_below_hp_percent;
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

void add_tests_BBQAutoBattle(UnitTestDatabase& database){
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
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP ?, 250 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP ?, 352 s elapsed. Below the target and HP unknown: return, heal and go again.",
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
                "Auto Battle pass 1: 0 kills, 0/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 0 kills, 0/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
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
                "Auto Battle pass 1: 1 kills, 1/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 1 kills, 2/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 3: 1 kills, 3/10 in total, HP ?, 250 s elapsed. Below the target and HP unknown: return, heal and go again.",
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
        {
            // Same kills as "auto battle 10 at 2 kills per pass", 4 heals (112 s) faster.
            .name = "high HP skips the heal",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .hp_per_clear = {0.8, 0.8, 0.8, 0.8, 0.8},
            .expected_steps = passes(5, LAST_PASS),
            .expected_seconds = 5 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - 4 * ScriptedAutoBattle::HEAL_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP 80%, 46 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP 80%, 120 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP 80%, 194 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP 80%, 268 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP 80%, 336 s elapsed. Target reached: return without healing.",
            },
        },
        {
            // Same kills as "auto battle 30 at 3 kills per pass", 9 heals (252 s) faster.
            .name = "high HP skips the heal in auto battle 30",
            .kill_target = 30,
            .kills_per_clear = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
            .hp_per_clear = {0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9},
            .expected_steps = passes(10, LAST_PASS),
            .expected_seconds = 10 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - 9 * ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            .name = "low HP heals",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .hp_per_clear = {0.3, 0.3, 0.3, 0.3, 0.3},
            .expected_steps = passes(4) + " " + LAST_PASS,
            .expected_seconds = 5 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP 30%, 46 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP 30%, 148 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP 30%, 250 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP 30%, 352 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP 30%, 448 s elapsed. Target reached: return without healing.",
            },
        },
        {
            // Unknown HP heals, as every pass did before the threshold.
            .name = "unknown HP heals",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .hp_per_clear = {0.8, std::nullopt, 0.8, std::nullopt, 0.8},
            .expected_steps = LAST_PASS + " " + PASS + " " + LAST_PASS + " " + PASS + " " + LAST_PASS,
            .expected_seconds = 5 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - 2 * ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            // HP at the threshold skips the heal; just below heals.
            .name = "heal threshold boundary",
            .kills_per_clear = {3, 3, 3, 3},
            .hp_per_clear = {0.5, 0.49, 0.5, 0.5},
            .expected_steps = LAST_PASS + " " + PASS + " " + LAST_PASS + " " + LAST_PASS,
            .expected_seconds = 4 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - 2 * ScriptedAutoBattle::HEAL_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 3 kills, 3/10 in total, HP 50%, 46 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 2: 3 kills, 6/10 in total, HP 49%, 120 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 3: 3 kills, 9/10 in total, HP 50%, 222 s elapsed. Below the target and HP at or above 50%: return and go again without healing.",
                "Auto Battle pass 4: 3 kills, 12/10 in total, HP 50%, 290 s elapsed. Target reached: return without healing.",
            },
        },
        {
            .name = "a threshold of 100% heals unless HP is full",
            .kills_per_clear = {4, 4, 4},
            .hp_per_clear = {1.0, 0.99, 1.0},
            .heal_below_hp_percent = 100,
            .expected_steps = LAST_PASS + " " + PASS + " " + LAST_PASS,
            .expected_seconds = 3 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            .name = "a threshold of 0% heals only when HP is unknown",
            .kills_per_clear = {4, 4, 4},
            .hp_per_clear = {0.01, std::nullopt, 0.01},
            .heal_below_hp_percent = 0,
            .expected_steps = LAST_PASS + " " + PASS + " " + LAST_PASS,
            .expected_seconds = 3 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            // The limits still end the attempt when no pass heals.
            .name = "gives up after 3 passes with no kill and high HP",
            .kills_per_clear = {0, 0, 0},
            .hp_per_clear = {1.0, 1.0, 1.0},
            .expected_result = BBQAutoBattleResult::NoKills,
            .expected_steps = passes(3, LAST_PASS),
            .expected_seconds = 3 * (PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS),
        },
    };
    for (const BBQAutoBattleScenario& scenario : scenarios){
        database.add<Test_BBQAutoBattle>(scenario);
    }
}

}

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

void add_tests_TeraEncounterAction(UnitTestDatabase& database){
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

class NullLogger : public Logger{
public:
    virtual void log(const std::string&, Color) override{}
};

// The quests read from the panel, the options that matter, and the plan
// plan_quests() must make. The default exclusions table has one row:
// auto-10, skip.
struct BBQuestPlanScenario{
    std::string name;
    std::vector<BBQuests> quests;
    std::vector<std::pair<BBQuests, BBQAction>> extra_exclusions;
    uint8_t num_eggs = 1;
    uint8_t eggs_hatched = 0;
    BBQOption::OOEggs out_of_eggs = BBQOption::OOEggs::Reroll;
    std::vector<BBQuests> expected_to_do;
    std::vector<size_t> expected_reroll_slots;
    bool expected_reroll_all = false;
};

std::string quests_to_string(const std::vector<BBQuests>& quests){
    std::string ret;
    for (BBQuests quest : quests){
        ret += std::to_string((int)quest) + " ";
    }
    return ret;
}
std::string slots_to_string(const std::vector<size_t>& slots){
    std::string ret;
    for (size_t slot : slots){
        ret += std::to_string(slot) + " ";
    }
    return ret;
}

class Test_BBQuestPlan : public UnitTest{
public:
    Test_BBQuestPlan(BBQuestPlanScenario scenario)
        : UnitTest("PokemonSV::BBQuestPlan - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        BBQOption options;
        options.NUM_EGGS.set(m_scenario.num_eggs);
        options.OUT_OF_EGGS.set(m_scenario.out_of_eggs);
        for (const auto& [quest, action] : m_scenario.extra_exclusions){
            auto row = std::make_unique<BBQuestTableRow>(options.QUEST_EXCLUSIONS);
            row->quest.set(quest);
            row->action.set(action);
            options.QUEST_EXCLUSIONS.append_row(std::move(row));
        }

        NullLogger logger;
        const BBQuestPlan plan = plan_quests(logger, options, m_scenario.quests, m_scenario.eggs_hatched);
        if (plan.to_do != m_scenario.expected_to_do){
            return "Expected quests to do: " + quests_to_string(m_scenario.expected_to_do)
                + "got: " + quests_to_string(plan.to_do);
        }
        if (plan.reroll_slots != m_scenario.expected_reroll_slots){
            return "Expected reroll slots: " + slots_to_string(m_scenario.expected_reroll_slots)
                + "got: " + slots_to_string(plan.reroll_slots);
        }
        if (plan.reroll_all != m_scenario.expected_reroll_all){
            return std::string("Expected reroll_all = ") + (m_scenario.expected_reroll_all ? "true" : "false");
        }
        return true;
    }

private:
    const BBQuestPlanScenario m_scenario;
};


// A quest panel with a cursor. A reroll replaces the quest under the cursor
// with the next of `replacements`, unless out of BP.
class SimulatedQuestPanel : public BBQRerollActions{
public:
    std::vector<BBQuests> quests;
    std::vector<BBQuests> replacements;
    size_t cursor = 0;
    bool panel_opens = true;
    bool menu_seen = true;
    bool out_of_bp = false;
    int missed_reads = 0;   // Reads that fail OCR on every slot.
    std::optional<std::vector<BBQuests>> read_override;

    bool panel_open = false;
    bool menu_open = false;
    std::vector<size_t> rerolled_slots;

    virtual bool open_panel() override{
        panel_open = panel_opens;
        return panel_open;
    }
    virtual std::optional<size_t> open_reroll_menu() override{
        menu_open = true;
        if (!menu_seen){
            return std::nullopt;
        }
        return cursor;
    }
    virtual void close_reroll_menu() override{
        menu_open = false;
    }
    virtual void move_cursor(int slots) override{
        cursor = (size_t)((int)cursor + slots);
    }
    virtual void confirm_reroll() override{
        menu_open = false;
        if (out_of_bp){
            return;
        }
        rerolled_slots.push_back(cursor);
        quests[cursor] = replacements[rerolled_slots.size() - 1];
    }
    virtual void close_panel() override{
        panel_open = false;
        menu_open = false;
    }
    virtual std::vector<BBQuests> read_quests() override{
        if (missed_reads > 0){
            missed_reads--;
            return std::vector<BBQuests>(quests.size(), BBQuests::UnableToDetect);
        }
        return read_override.value_or(quests);
    }
};

struct BBQRerollScenario{
    std::string name;
    std::vector<BBQuests> quests;
    std::vector<BBQuests> replacements;
    size_t slot;
    size_t cursor = 0;
    bool panel_opens = true;
    bool menu_seen = true;
    bool out_of_bp = false;
    int missed_reads = 0;
    std::optional<std::vector<BBQuests>> read_override;
    BBQRerollResult expected;
    std::vector<size_t> expected_rerolled_slots;
    std::vector<BBQuests> expected_quests;
    std::optional<std::vector<BBQuests>> expected_observed_panel;
};

std::string reroll_result_to_string(BBQRerollResult result){
    switch (result){
    case BBQRerollResult::Rerolled:         return "Rerolled";
    case BBQRerollResult::PanelNotOpened:   return "PanelNotOpened";
    case BBQRerollResult::MenuNotFound:     return "MenuNotFound";
    case BBQRerollResult::NotReplaced:      return "NotReplaced";
    }
    return "?";
}

class Test_BBQReroll : public UnitTest{
public:
    Test_BBQReroll(BBQRerollScenario scenario)
        : UnitTest("PokemonSV::BBQReroll - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        SimulatedQuestPanel panel;
        panel.quests = m_scenario.quests;
        panel.replacements = m_scenario.replacements;
        panel.cursor = m_scenario.cursor;
        panel.panel_opens = m_scenario.panel_opens;
        panel.menu_seen = m_scenario.menu_seen;
        panel.out_of_bp = m_scenario.out_of_bp;
        panel.missed_reads = m_scenario.missed_reads;
        panel.read_override = m_scenario.read_override;

        NullLogger logger;
        std::vector<BBQuests> quests = m_scenario.quests;
        std::optional<std::vector<BBQuests>> observed_panel;
        const BBQRerollResult result = reroll_quest(logger, panel, quests, m_scenario.slot, observed_panel);
        if (result != m_scenario.expected){
            return "Expected " + reroll_result_to_string(m_scenario.expected)
                + ", got " + reroll_result_to_string(result);
        }
        if (panel.rerolled_slots != m_scenario.expected_rerolled_slots){
            return "Expected rerolled slots: " + slots_to_string(m_scenario.expected_rerolled_slots)
                + "got: " + slots_to_string(panel.rerolled_slots);
        }
        if (quests != m_scenario.expected_quests){
            return "Expected quests: " + quests_to_string(m_scenario.expected_quests)
                + "got: " + quests_to_string(quests);
        }
        if (observed_panel != m_scenario.expected_observed_panel){
            return "Expected observed panel: " + quests_to_string(m_scenario.expected_observed_panel.value_or(std::vector<BBQuests>{}))
                + ", got: " + quests_to_string(observed_panel.value_or(std::vector<BBQuests>{}));
        }
        if (panel.panel_open || panel.menu_open){
            return "Panel left open.";
        }
        return true;
    }

private:
    const BBQRerollScenario m_scenario;
};

}

void add_tests_BBQuestPlan(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQuestPlanScenario scenarios[] = {
        {
            .name = "default table skips auto-10",
            .quests = {make_tm, auto_10, sneak_up},
            .expected_to_do = {make_tm, sneak_up},
        },
        {
            .name = "quests not possible are left out",
            .quests = {pickup_10, make_tm, UnableToDetect},
            .expected_to_do = {make_tm},
        },
        {
            // One quest costs less BP to reroll than all of them.
            .name = "nothing doable rerolls the first quest not possible",
            .quests = {auto_10, pickup_10, catch_flying},
            .expected_reroll_slots = {1},
        },
        {
            .name = "nothing doable and nothing not possible rerolls all",
            .quests = {auto_10, sneak_up},
            .extra_exclusions = {{sneak_up, BBQAction::skip}},
            .expected_reroll_all = true,
        },
        {
            // An undetected quest is not known to be impossible.
            .name = "an undetected quest is not rerolled alone",
            .quests = {UnableToDetect, auto_10},
            .expected_reroll_all = true,
        },
        {
            .name = "a skipped quest not possible is not rerolled",
            .quests = {pickup_10, auto_10, catch_flying},
            .extra_exclusions = {{pickup_10, BBQAction::skip}},
            .expected_reroll_slots = {2},
        },
        {
            .name = "a reroll row rerolls a quest not possible",
            .quests = {make_tm, sneak_up, pickup_10},
            .extra_exclusions = {{pickup_10, BBQAction::reroll}},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {2},
        },
        {
            .name = "a run row does not run a quest not possible",
            .quests = {make_tm, pickup_10},
            .extra_exclusions = {{pickup_10, BBQAction::run}},
            .expected_to_do = {make_tm},
        },
        {
            // The reroll replaces a quest, so the next read has something new.
            .name = "a reroll row rerolls that slot",
            .quests = {make_tm, sneak_up, travel_500},
            .extra_exclusions = {{travel_500, BBQAction::reroll}},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {2},
        },
        {
            // A reroll is already a change, so nothing more is rerolled.
            .name = "a rerolled slot with nothing doable does not reroll all",
            .quests = {pickup_10, sneak_up, auto_10},
            .extra_exclusions = {{sneak_up, BBQAction::reroll}},
            .expected_reroll_slots = {1},
        },
        {
            // Each row for a quest applies, in table order.
            .name = "every table row for a quest applies",
            .quests = {travel_500, auto_10},
            .extra_exclusions = {
                {travel_500, BBQAction::run},
                {auto_10, BBQAction::run},
                {travel_500, BBQAction::run},
            },
            .expected_to_do = {travel_500, travel_500, auto_10},
        },
        {
            .name = "egg quest is done while eggs remain",
            .quests = {make_tm, hatch_egg, sneak_up},
            .expected_to_do = {make_tm, hatch_egg, sneak_up},
        },
        {
            // The egg quest is rerolled where it is on the panel, not always
            // in the top slot.
            .name = "out of eggs rerolls the egg quest slot",
            .quests = {make_tm, hatch_egg, sneak_up},
            .eggs_hatched = 1,
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {1},
        },
        {
            .name = "out of eggs with keep going leaves the egg quest",
            .quests = {make_tm, hatch_egg, sneak_up},
            .eggs_hatched = 1,
            .out_of_eggs = BBQOption::OOEggs::KeepGoing,
            .expected_to_do = {make_tm, sneak_up},
        },
    };
    for (const BBQuestPlanScenario& scenario : scenarios){
        database.add<Test_BBQuestPlan>(scenario);
    }
}

void add_tests_BBQReroll(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQRerollScenario scenarios[] = {
        {
            .name = "unknown fourth slot is not reusable",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{make_tm, travel_500, catch_any, UnableToDetect},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, travel_500, catch_any, hatch_egg},
        },
        {
            .name = "missing fourth slot is not reusable",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{make_tm, travel_500, catch_any},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, travel_500, catch_any, hatch_egg},
        },
        {
            .name = "another changed quest is not reusable",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{photo_canyon, travel_500, catch_any, hatch_egg},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, travel_500, catch_any, hatch_egg},
        },
        {
            // The panel used for the reroll had 3 slots, so a 3-slot read is
            // not a missed fourth quest.
            .name = "three-slot read of a three-slot panel is reusable",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, sneak_up, catch_any},
        },
        {
            .name = "three-slot read with an unknown slot is not reusable",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{make_tm, UnableToDetect, catch_any},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
        },
        {
            .name = "three-slot read with another changed quest is not reusable",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{photo_canyon, sneak_up, catch_any},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
        },
        {
            // The panel used for the reroll missed this fourth quest.
            .name = "fourth quest not on the three-slot panel is not reusable",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .read_override = std::vector<BBQuests>{make_tm, sneak_up, catch_any, hatch_egg},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
        },
        {
            .name = "complete observation after missed reads is reusable",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 2,
            .missed_reads = 2,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, travel_500, catch_any, hatch_egg},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, travel_500, catch_any, hatch_egg},
        },
        {
            .name = "cursor on the top quest",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, sneak_up, catch_any},
        },
        {
            // Seen on a Switch: the panel can open with the cursor where it
            // was left, so moving down from the top rerolls the wrong quest.
            .name = "cursor left below the quest moves up",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 1,
            .cursor = 3,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {1},
            .expected_quests = {make_tm, catch_any, sneak_up, hatch_egg},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, catch_any, sneak_up, hatch_egg},
        },
        {
            .name = "fourth quest",
            .quests = {make_tm, travel_500, sneak_up, hatch_egg},
            .replacements = {catch_any},
            .slot = 3,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {3},
            .expected_quests = {make_tm, travel_500, sneak_up, catch_any},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, travel_500, sneak_up, catch_any},
        },
        {
            .name = "panel not opened spends nothing",
            .quests = {make_tm, sneak_up, travel_500},
            .slot = 2,
            .panel_opens = false,
            .expected = BBQRerollResult::PanelNotOpened,
            .expected_quests = {make_tm, sneak_up, travel_500},
        },
        {
            .name = "menu not seen spends nothing",
            .quests = {make_tm, sneak_up, travel_500},
            .slot = 2,
            .menu_seen = false,
            .expected = BBQRerollResult::MenuNotFound,
            .expected_quests = {make_tm, sneak_up, travel_500},
        },
        {
            .name = "out of BP is not replaced",
            .quests = {make_tm, sneak_up, travel_500},
            .slot = 2,
            .out_of_bp = true,
            .expected = BBQRerollResult::NotReplaced,
            .expected_quests = {make_tm, sneak_up, travel_500},
        },
        {
            .name = "a missed read is read again",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .missed_reads = 2,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, sneak_up, catch_any},
        },
        {
            .name = "three missed reads give up",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {catch_any},
            .slot = 2,
            .missed_reads = 3,
            .expected = BBQRerollResult::NotReplaced,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, travel_500},
        },
        {
            // Cannot be told apart from out of BP.
            .name = "replaced by the same quest is not replaced",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {travel_500},
            .slot = 2,
            .expected = BBQRerollResult::NotReplaced,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, travel_500},
        },
    };
    for (const BBQRerollScenario& scenario : scenarios){
        database.add<Test_BBQReroll>(scenario);
    }
}


namespace{

// A singleplayer quest panel that behaves like the game's: 3 blue quests,
// and a completed quest is replaced by a new one. process_quest_list()
// follows the rules of the real one that the round loop depends on: quests
// in not_possible_quests are skipped, quests in table_rerolls are rerolled
// in their slot as an exclusions table reroll row does, and if nothing is
// doable all quests are rerolled. After any reroll it returns nothing to do.
// Red quests are not modeled. Records the game actions taken.
// With out_of_bp, rerolls do nothing, as in the game when BP runs out.
class SimulatedBBQGameActions : public BBQGameActions{
public:
    std::vector<BBQuests> panel;
    // Each attempt returns the next outcome; once exhausted, attempts succeed.
    std::vector<bool> outcomes;
    // Completing the first quest also completes the second, e.g. walking
    // 500 meters while navigating to a photo target.
    std::vector<std::pair<BBQuests, BBQuests>> also_completes;
    // Attempts at these always fail, e.g. a target that never spawns.
    std::set<BBQuests> never_completes;
    // New quests are drawn from this list in turn, skipping any that are
    // already on the panel.
    std::vector<BBQuests> new_quests = {
        BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar,
        BBQuests::photo_savanna, BBQuests::catch_any, BBQuests::catch_normal,
        BBQuests::catch_fire, BBQuests::catch_water, BBQuests::catch_grass,
    };
    bool out_of_bp = false;
    // Quests that process_quest_list() rerolls in their slot.
    std::set<BBQuests> table_rerolls;
    // Rerolls return the panel they read to verify the reroll, as
    // reroll_quest() does when that read is reusable.
    bool reuse_rerolled_panel = false;

    std::vector<std::string> events;
    std::vector<BBQuests> attempted_quests;

    virtual std::vector<BBQuests> read_quests() override{
        events.push_back("read");
        if (++m_reads > 1000){
            throw std::runtime_error("Too many quest panel reads.");
        }
        return panel;
    }
    virtual BBQuestBatch process_quest_list(
        const std::vector<BBQuests>& quests, uint8_t
    ) override{
        events.push_back("process");
        bool rerolled = false;
        for (BBQuests& slot : panel){
            if (!out_of_bp && table_rerolls.contains(slot)){
                slot = next_new_quest();
                rerolled = true;
            }
        }
        std::vector<BBQuests> doable;
        for (BBQuests quest : quests){
            if (!not_possible_quests.contains(quest)){
                doable.push_back(quest);
            }
        }
        if (!rerolled && doable.empty() && !out_of_bp){
            for (BBQuests& slot : panel){
                slot = next_new_quest();
            }
            rerolled = true;
        }
        if (rerolled){
            return {{}, reuse_rerolled_panel ? std::optional(panel) : std::nullopt};
        }
        return {std::move(doable), std::nullopt};
    }
    virtual bool do_quest(BBQuests quest, uint8_t&) override{
        events.push_back("do");
        attempted_quests.push_back(quest);
        const bool success = !never_completes.contains(quest)
            && (m_next_outcome < outcomes.size() ? outcomes[m_next_outcome++] : true);
        if (success){
            complete(quest);
            for (const auto& [done, extra] : also_completes){
                if (done == quest){
                    complete(extra);
                }
            }
        }
        return success;
    }
    virtual std::optional<std::vector<BBQuests>> reroll_quest(BBQuests quest) override{
        events.push_back("reroll");
        auto iter = std::find(panel.begin(), panel.end(), quest);
        if (iter == panel.end()){
            return std::nullopt;
        }
        *iter = next_new_quest();
        return reuse_rerolled_panel ? std::optional(panel) : std::nullopt;
    }
    virtual void quest_completed() override{
        events.push_back("completed");
    }
    virtual void end_round(bool save) override{
        events.push_back(save ? "end(save)" : "end");
    }

private:
    void complete(BBQuests quest){
        auto iter = std::find(panel.begin(), panel.end(), quest);
        if (iter != panel.end()){
            *iter = next_new_quest();
        }
    }

    BBQuests next_new_quest(){
        while (true){
            BBQuests quest = new_quests[m_next_new_quest++ % new_quests.size()];
            if (std::find(panel.begin(), panel.end(), quest) == panel.end()){
                return quest;
            }
        }
    }

    size_t m_reads = 0;
    size_t m_next_outcome = 0;
    size_t m_next_new_quest = 0;
};

std::string join_events(const std::vector<std::string>& events){
    std::string ret;
    for (const std::string& event : events){
        if (!ret.empty()) ret += ' ';
        ret += event;
    }
    return ret;
}

// One simulated farmer run: option values, the panel it starts with and how
// quests turn out, and the game actions it must take.
struct BBQRoundsScenario{
    std::string name;
    uint64_t num_quests;
    uint64_t save_num_quests = 0;
    bool stop_when_out_of_eggs = false;
    std::vector<BBQuests> panel = {BBQuests::make_tm, BBQuests::auto_10, BBQuests::sneak_up};
    std::vector<bool> outcomes;
    std::vector<std::pair<BBQuests, BBQuests>> also_completes;
    std::set<BBQuests> never_completes;
    std::vector<BBQuests> new_quests;
    BBQOption::OutOfRetries out_of_retries = BBQOption::OutOfRetries::Skip;
    std::vector<std::pair<BBQuests, BBQAction>> exclusions;
    bool out_of_bp = false;
    std::set<BBQuests> table_rerolls;
    bool reuse_rerolled_panel = false;
    uint64_t expected_completed = 0;
    std::vector<BBQuests> expected_attempted;
    std::optional<size_t> expected_reads;
    std::string expected;
};

class Test_BBQFarmerRounds : public UnitTest{
public:
    Test_BBQFarmerRounds(BBQRoundsScenario scenario)
        : UnitTest("PokemonSV::BBQFarmerRounds - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        BBQOption options;
        options.NUM_QUESTS.set(m_scenario.num_quests);
        options.SAVE_NUM_QUESTS.set(m_scenario.save_num_quests);
        if (m_scenario.stop_when_out_of_eggs){
            options.NUM_EGGS.set(0);
            options.OUT_OF_EGGS.set(BBQOption::OOEggs::Stop);
        }
        options.OUT_OF_RETRIES.set(m_scenario.out_of_retries);
        for (const auto& [quest, action] : m_scenario.exclusions){
            auto row = std::make_unique<BBQuestTableRow>(options.QUEST_EXCLUSIONS);
            row->quest.set(quest);
            row->action.set(action);
            options.QUEST_EXCLUSIONS.append_row(std::move(row));
        }

        SimulatedBBQGameActions game;
        game.panel = m_scenario.panel;
        game.outcomes = m_scenario.outcomes;
        game.also_completes = m_scenario.also_completes;
        game.never_completes = m_scenario.never_completes;
        if (!m_scenario.new_quests.empty()){
            game.new_quests = m_scenario.new_quests;
        }
        game.out_of_bp = m_scenario.out_of_bp;
        game.table_rerolls = m_scenario.table_rerolls;
        game.reuse_rerolled_panel = m_scenario.reuse_rerolled_panel;

        NullLogger logger;
        try{
            run_bbq_rounds(logger, options, game);
        }catch (UserSetupError&){
            game.events.push_back("error");
        }
        const uint64_t completed = (uint64_t)std::count(game.events.begin(), game.events.end(), "completed");
        if (completed != m_scenario.expected_completed){
            return "Expected " + std::to_string(m_scenario.expected_completed)
                + " completed quests, counted " + std::to_string(completed);
        }
        if (!m_scenario.expected_attempted.empty() && game.attempted_quests != m_scenario.expected_attempted){
            std::vector<std::string> expected_quests;
            std::vector<std::string> actual_quests;
            for (BBQuests quest : m_scenario.expected_attempted){
                expected_quests.push_back(std::to_string((int)quest));
            }
            for (BBQuests quest : game.attempted_quests){
                actual_quests.push_back(std::to_string((int)quest));
            }
            return "Expected quest order: " + join_events(expected_quests)
                + "\nActual quest order:   " + join_events(actual_quests);
        }
        const size_t reads = (size_t)std::count(game.events.begin(), game.events.end(), "read");
        if (m_scenario.expected_reads && reads != *m_scenario.expected_reads){
            return "Expected " + std::to_string(*m_scenario.expected_reads)
                + " quest panel reads, counted " + std::to_string(reads);
        }
        const std::string actual = join_events(game.events);
        if (actual != m_scenario.expected){
            return "Expected: " + m_scenario.expected + "\nActual:   " + actual;
        }
        return true;
    }

private:
    const BBQRoundsScenario m_scenario;
};

// Every blue quest the program can do, each with the given action.
std::vector<std::pair<BBQuests, BBQAction>> every_doable_blue_quest(BBQAction action){
    std::vector<std::pair<BBQuests, BBQAction>> ret;
    for (BBQuests quest : blue_quests){
        if (!not_possible_quests.contains(quest)){
            ret.emplace_back(quest, action);
        }
    }
    return ret;
}

void add_tests_BBQFarmerRounds(UnitTestDatabase& database){
    const std::string round_of_3 = "read process do completed read do completed read do completed";
    std::string read_30_times;
    for (int c = 0; c < 30; c++){
        read_30_times += "read process ";
    }
    std::vector<std::pair<BBQuests, BBQAction>> all_skipped_but_make_tm = every_doable_blue_quest(BBQAction::skip);
    all_skipped_but_make_tm.emplace_back(BBQuests::make_tm, BBQAction::run);
    const BBQRoundsScenario scenarios[] = {
        {
            .name = "failed reroll replans before queued photo and TM",
            .num_quests = 1,
            .panel = {BBQuests::catch_grass, BBQuests::photo_savanna, BBQuests::make_tm},
            .outcomes = {false},
            .new_quests = {BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .expected_completed = 3,
            .expected_attempted = {BBQuests::catch_grass, BBQuests::travel_500, BBQuests::photo_savanna, BBQuests::make_tm},
            .expected = "read process do reroll end " + round_of_3 + " end",
        },
        {
            .name = "failed reroll on full board reuses its verified read after round actions",
            .num_quests = 1,
            .panel = {BBQuests::catch_grass, BBQuests::photo_savanna, BBQuests::make_tm, BBQuests::wash_pokemon},
            .outcomes = {false},
            .new_quests = {BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .reuse_rerolled_panel = true,
            .expected_completed = 4,
            .expected_attempted = {BBQuests::catch_grass, BBQuests::travel_500, BBQuests::photo_savanna, BBQuests::make_tm, BBQuests::wash_pokemon},
            .expected = "read process do reroll end process do completed read do completed read do completed read do completed end",
        },
        {
            .name = "replanning after failure preserves a crossed save milestone",
            .num_quests = 2,
            .save_num_quests = 1,
            .panel = {BBQuests::make_tm, BBQuests::catch_grass, BBQuests::photo_savanna},
            .outcomes = {true, false},
            .new_quests = {BBQuests::photo_canyon, BBQuests::travel_500, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .expected_completed = 4,
            .expected_attempted = {BBQuests::make_tm, BBQuests::catch_grass, BBQuests::photo_canyon, BBQuests::travel_500, BBQuests::photo_savanna},
            .expected = "read process do completed read do reroll end(save) " + round_of_3 + " end(save)",
        },
        {
            .name = "reuses reroll observation for selection and first dispatch only",
            .num_quests = 1,
            .panel = {BBQuests::pickup_10, BBQuests::catch_flying, BBQuests::UnableToDetect, BBQuests::photo_flying},
            .reuse_rerolled_panel = true,
            .expected_completed = 4,
            .expected = "read process process do completed read do completed read do completed read do completed end",
        },
        {
            // reroll_quest() decides whether a 3-slot read is reusable.
            .name = "reusable three-slot reroll observation is not read again",
            .num_quests = 1,
            .panel = {BBQuests::pickup_10, BBQuests::catch_flying, BBQuests::UnableToDetect},
            .reuse_rerolled_panel = true,
            .expected_completed = 3,
            .expected = "read process process do completed read do completed read do completed end",
        },
        {
            // Seen in a live run: catch-fighting fails and is rerolled into
            // pickup-10, which the exclusions table rerolls into travel-500.
            // Each reroll's verified read is reused, and so is the last one
            // for the first dispatch. The round loop used to read the panel
            // 3 times between the failed reroll and the next attempt.
            .name = "failed reroll then table reroll reuse verified reads",
            .num_quests = 1,
            .panel = {BBQuests::catch_fighting, BBQuests::make_tm, BBQuests::photo_savanna},
            .outcomes = {false},
            .new_quests = {BBQuests::pickup_10, BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .table_rerolls = {BBQuests::pickup_10},
            .reuse_rerolled_panel = true,
            .expected_completed = 3,
            .expected_attempted = {BBQuests::catch_fighting, BBQuests::travel_500, BBQuests::make_tm, BBQuests::photo_savanna},
            .expected_reads = 3,
            .expected = "read process do reroll end process process do completed read do completed read do completed end",
        },
        {
            // As above, but neither reroll's read is reusable. Only the
            // first dispatch skips a read: 2 reads before the next attempt
            // instead of 3.
            .name = "failed reroll then table reroll without reusable reads",
            .num_quests = 1,
            .panel = {BBQuests::catch_fighting, BBQuests::make_tm, BBQuests::photo_savanna},
            .outcomes = {false},
            .new_quests = {BBQuests::pickup_10, BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .table_rerolls = {BBQuests::pickup_10},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::catch_fighting, BBQuests::travel_500, BBQuests::make_tm, BBQuests::photo_savanna},
            .expected_reads = 5,
            .expected = "read process do reroll end read process read process do completed read do completed read do completed end",
        },
        {
            // The save resets the game, so the reroll's read is not reused.
            .name = "failed reroll before a save reads the panel after it",
            .num_quests = 2,
            .save_num_quests = 1,
            .panel = {BBQuests::make_tm, BBQuests::catch_grass, BBQuests::photo_savanna},
            .outcomes = {true, false},
            .new_quests = {BBQuests::photo_canyon, BBQuests::travel_500, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .reuse_rerolled_panel = true,
            .expected_completed = 4,
            .expected = "read process do completed read do reroll end(save) " + round_of_3 + " end(save)",
        },
        {
            // The goal (4) is reached in the middle of round 2. The loop only
            // checks the goal between rounds, so round 2 finishes and 6 quests
            // complete.
            .name = "finishes the round that reaches the quest goal",
            .num_quests = 4,
            .expected_completed = 6,
            .expected = round_of_3 + " end " + round_of_3 + " end",
        },
        {
            // Doing photo_swim also completes travel_500, so travel_500 is
            // gone when the loop reads the panel before attempting it.
            .name = "quest already gone counts as completed",
            .num_quests = 3,
            .panel = {BBQuests::photo_swim, BBQuests::travel_500, BBQuests::make_tm},
            .also_completes = {{BBQuests::photo_swim, BBQuests::travel_500}},
            .expected_completed = 3,
            .expected = "read process do completed read completed read do completed end",
        },
        {
            // The first attempt fails: 3 attempts, 2 completions.
            .name = "failed quest does not count",
            .num_quests = 2,
            .outcomes = {false},
            .expected_completed = 2,
            .expected = "read process do read do completed read do completed end",
        },
        {
            // Nothing on the panel is doable, so process_quest_list() rerolls
            // all of it and returns nothing; the loop reads the panel again.
            .name = "panel with nothing doable is rerolled",
            .num_quests = 3,
            .panel = {BBQuests::pickup_10, BBQuests::catch_flying, BBQuests::UnableToDetect},
            .expected_completed = 3,
            .expected = "read process " + round_of_3 + " end",
        },
        {
            .name = "saves at each milestone",
            .num_quests = 6,
            .save_num_quests = 3,
            .expected_completed = 6,
            .expected = round_of_3 + " end(save) " + round_of_3 + " end(save)",
        },
        {
            // 0 -> 3 crosses the milestone at 2 without landing on it.
            .name = "saves when a round skips past a milestone",
            .num_quests = 6,
            .save_num_quests = 2,
            .expected_completed = 6,
            .expected = round_of_3 + " end(save) " + round_of_3 + " end(save)",
        },
        {
            // Three partial rounds each fail and reroll their first quest.
            // None may save while the completed count is still zero.
            .name = "round without completions does not save",
            .num_quests = 3,
            .save_num_quests = 2,
            .outcomes = {false, false, false},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .expected_completed = 3,
            .expected =
                "read process do reroll end read process do reroll end "
                "read process do reroll end " + round_of_3 + " end(save)",
        },
        {
            .name = "stops when out of eggs",
            .num_quests = 5,
            .stop_when_out_of_eggs = true,
            .expected_completed = 0,
            .expected = "",
        },
        {
            // sneak-up fails in round 1 and is still on the panel in round 2,
            // where only the two new quests are attempted.
            .name = "failed quest is not attempted again",
            .num_quests = 4,
            .never_completes = {BBQuests::sneak_up},
            .expected_completed = 4,
            .expected =
                "read process do completed read do completed read do end "
                "read process do completed read do completed end",
        },
        {
            .name = "stops when only failed quests are doable",
            .num_quests = 3,
            .panel = {BBQuests::sneak_up, BBQuests::pickup_10, BBQuests::catch_flying},
            .never_completes = {BBQuests::sneak_up},
            .expected_completed = 0,
            .expected = "read process do end read process",
        },
        {
            // sneak-up fails, then leaves the panel along with make-tm. Round 2
            // reads a full panel without it, and its replacement for
            // photo-canyon brings it back. Round 3 attempts it again.
            .name = "failed quest that leaves a full panel is attempted again",
            .num_quests = 5,
            .panel = {BBQuests::sneak_up, BBQuests::make_tm, BBQuests::pickup_10, BBQuests::catch_flying},
            .outcomes = {false},
            .also_completes = {{BBQuests::make_tm, BBQuests::sneak_up}},
            .new_quests = {BBQuests::auto_10, BBQuests::photo_canyon, BBQuests::sneak_up},
            .expected_completed = 5,
            .expected =
                "read process do read do completed end "
                "read process do completed read do completed end "
                "read process do completed read do completed end",
        },
        {
            // As above with 3 slots. A 4th quest could have been missed by
            // OCR, so the read does not prove sneak-up left: it stays failed.
            .name = "failed quest missing from a 3-slot panel stays failed",
            .num_quests = 4,
            .panel = {BBQuests::sneak_up, BBQuests::make_tm, BBQuests::pickup_10},
            .outcomes = {false},
            .also_completes = {{BBQuests::make_tm, BBQuests::sneak_up}},
            .new_quests = {BBQuests::auto_10, BBQuests::photo_canyon, BBQuests::sneak_up},
            .expected_completed = 4,
            .expected =
                "read process do read do completed end "
                "read process do completed read do completed end "
                "read process do completed end",
        },
        {
            // sneak-up is rerolled to photo-polar, which round 2 attempts.
            .name = "out of retries with reroll replaces the failed quest",
            .num_quests = 3,
            .never_completes = {BBQuests::sneak_up},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .expected_completed = 5,
            .expected = "read process do completed read do completed read do reroll end " + round_of_3 + " end",
        },
        {
            .name = "out of retries with stop stops at once",
            .num_quests = 3,
            .panel = {BBQuests::sneak_up, BBQuests::make_tm, BBQuests::auto_10},
            .never_completes = {BBQuests::sneak_up},
            .out_of_retries = BBQOption::OutOfRetries::Stop,
            .expected_completed = 0,
            .expected = "read process do",
        },
        {
            // Rerolling could never produce a doable panel, so the run
            // fails before reading the panel.
            .name = "fails at start when every blue quest is excluded",
            .num_quests = 3,
            .exclusions = every_doable_blue_quest(BBQAction::reroll),
            .expected_completed = 0,
            .expected = "error",
        },
        {
            // A run row keeps the quest doable even when another row skips it.
            .name = "a quest with a run row is not excluded",
            .num_quests = 3,
            .exclusions = all_skipped_but_make_tm,
            .expected_completed = 3,
            .expected = round_of_3 + " end",
        },
        {
            // Rerolls change nothing, so the panel never becomes doable.
            .name = "gives up when rerolling does not help",
            .num_quests = 3,
            .panel = {BBQuests::pickup_10, BBQuests::catch_flying, BBQuests::UnableToDetect},
            .out_of_bp = true,
            .expected_completed = 0,
            .expected = read_30_times + "error",
        },
    };
    for (const BBQRoundsScenario& scenario : scenarios){
        database.add<Test_BBQFarmerRounds>(scenario);
    }
}

}

namespace{

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

// Balance reads in order, each at a minute offset from the first, and the
// ledger they must produce. A negative read is an OCR failure.
struct BBQPointsLedgerScenario{
    struct Read{
        int balance;
        int minute;
    };
    std::string name;
    std::vector<Read> reads;
    int expected_start = -1;
    uint64_t expected_earned = 0;
    uint64_t expected_spent = 0;
    int64_t expected_net = 0;
    int64_t expected_per_hour = 0;
};

class Test_BBQPointsLedger : public UnitTest{
public:
    Test_BBQPointsLedger(BBQPointsLedgerScenario scenario)
        : UnitTest("PokemonSV::BBQPointsLedger - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const WallClock start = current_time();
        BBQPointsLedger ledger;
        for (const BBQPointsLedgerScenario::Read& read : m_scenario.reads){
            ledger.record(read.balance, start + std::chrono::minutes(read.minute));
        }
        const std::pair<const char*, std::pair<int64_t, int64_t>> checks[] = {
            {"starting balance", {m_scenario.expected_start, ledger.starting_balance}},
            {"earned", {(int64_t)m_scenario.expected_earned, (int64_t)ledger.earned}},
            {"spent", {(int64_t)m_scenario.expected_spent, (int64_t)ledger.spent}},
            {"net", {m_scenario.expected_net, ledger.net()}},
            {"net per hour", {m_scenario.expected_per_hour, ledger.net_per_hour()}},
        };
        for (const auto& [field, values] : checks){
            if (values.first != values.second){
                return std::string("Expected ") + field + " " + std::to_string(values.first)
                    + ", got " + std::to_string(values.second);
            }
        }
        return true;
    }

private:
    const BBQPointsLedgerScenario m_scenario;
};

}

void add_tests_BBQPointsLedger(UnitTestDatabase& database){
    const BBQPointsLedgerScenario scenarios[] = {
        {
            .name = "no reads",
        },
        {
            .name = "one read sets the start",
            .reads = {{6750, 0}},
            .expected_start = 6750,
        },
        {
            // A quest reward, then a reroll.
            .name = "rises are earned and drops are spent",
            .reads = {{6750, 0}, {6780, 10}, {6730, 20}},
            .expected_start = 6750,
            .expected_earned = 30,
            .expected_spent = 50,
            .expected_net = -20,
            .expected_per_hour = -60,
        },
        {
            .name = "an unchanged balance adds nothing",
            .reads = {{6750, 0}, {6750, 30}},
            .expected_start = 6750,
        },
        {
            .name = "OCR failures are skipped",
            .reads = {{6750, 0}, {-1, 5}, {6800, 30}},
            .expected_start = 6750,
            .expected_earned = 50,
            .expected_net = 50,
            .expected_per_hour = 100,
        },
        {
            // The hour starts at the first successful read.
            .name = "a failed first read does not set the start",
            .reads = {{-1, 0}, {6750, 10}, {6760, 70}},
            .expected_start = 6750,
            .expected_earned = 10,
            .expected_net = 10,
            .expected_per_hour = 10,
        },
    };
    for (const BBQPointsLedgerScenario& scenario : scenarios){
        database.add<Test_BBQPointsLedger>(scenario);
    }
}

namespace{

// A catch quest, whether this attempt already battled, and whether a wild
// battle on the way back to Central Plaza should be caught instead of fled.
struct BBQCatchOnReturnScenario{
    std::string name;
    BBQuests quest;
    bool battled_this_attempt = false;
    bool expected_catch = false;
};

class Test_BBQCatchOnReturn : public UnitTest{
public:
    Test_BBQCatchOnReturn(BBQCatchOnReturnScenario scenario)
        : UnitTest("PokemonSV::BBQCatchOnReturn - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool got = catch_quest_catches_on_return(m_scenario.quest, m_scenario.battled_this_attempt);
        if (got != m_scenario.expected_catch){
            return std::string("Expected ") + (m_scenario.expected_catch ? "catch" : "flee")
                + ", got " + (got ? "catch" : "flee");
        }
        return true;
    }

private:
    const BBQCatchOnReturnScenario m_scenario;
};

void add_tests_BBQCatchOnReturn(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQCatchOnReturnScenario scenarios[] = {
        {
            // The target spawned late or not at all: any wild Pokemon completes the quest.
            .name = "catch any without a battle catches",
            .quest = catch_any,
            .expected_catch = true,
        },
        {
            // Already caught this attempt: fleeing is faster than a second catch.
            .name = "catch any after a battle flees",
            .quest = catch_any,
            .battled_this_attempt = true,
        },
        {
            // The type of a wild Pokemon is not read in battle.
            .name = "catch normal without a battle flees",
            .quest = catch_normal,
        },
        {
            .name = "catch fire without a battle flees",
            .quest = catch_fire,
        },
        {
            .name = "catch psychic without a battle flees",
            .quest = catch_psychic,
        },
        {
            .name = "catch fighting after a battle flees",
            .quest = catch_fighting,
            .battled_this_attempt = true,
        },
    };
    for (const BBQCatchOnReturnScenario& scenario : scenarios){
        database.add<Test_BBQCatchOnReturn>(scenario);
    }
}

}

void add_tests(UnitTestDatabase& database){
    add_tests_BBQuestPlan(database);
    add_tests_BBQReroll(database);
    add_tests_BBQFarmerRounds(database);
    add_tests_BBQAutoBattle(database);
    // Full-frame references exercise the production crop, filter and OCR together.
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6750.png", 6750, 1280, 720);

    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1920, 1080);
    database.add<Test_BlueberryPoints>("PokemonSV/BlueberryPoints/6740.png", 6740, 1280, 720);

    add_tests_BBQPointsLedger(database);

    add_tests_BBQCatchOnReturn(database);
    add_tests_WhiteButtonDetector(database);
    add_tests_BoxInfoDetector(database);
    add_tests_BoxEggDetector(database);
    add_tests_DialogDetector(database);
    add_tests_ESPEmotionDetector(database);
    add_tests_FastTravelDetector(database);
    add_tests_MapDetector(database);
    add_tests_MapMenuDetector(database);
    add_tests_MapPokeCenterIconDetector(database);
    add_tests_OverworldDetector(database);
    add_tests_PicnicDetector(database);
    add_tests_SandwichIngredientDetector(database);
    add_tests_SandwichPlateDetector(database);
    add_tests_SandwichRecipeDetector(database);
    add_tests_SandwichHandDetector(database);
    add_tests_TeraCardDetector(database);
    add_tests_TeraSilhouetteReader(database);
    add_tests_TeraTypeReader(database);
    add_tests_NormalBattleMenus(database);
    add_tests_LetsGoKillDetector(database);
    add_tests_TeraEncounterAction(database);
}



}
}
}

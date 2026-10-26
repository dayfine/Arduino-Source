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
#include "Programs/PokemonSV_TerariumPlazaReturn.h"
#include "Programs/Farming/PokemonSV_BBQLabConsole.h"
#include "Programs/Farming/PokemonSV_BBQRoutineBench.h"
#include "PokemonSV_Tests.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Inference/PokemonSV_BlueberryPointsReader.h"
#include "Programs/Farming/PokemonSV_BBQPointsLedger.h"
#include "Programs/Farming/PokemonSV_BBQLeaveSpot.h"
#include "Inference/Map/PokemonSV_MapDetector.h"
#include "Inference/Map/PokemonSV_FastTravelDetector.h"
#include "Inference/Map/PokemonSV_MapPokeCenterIconDetector.h"
#include "Programs/PokemonSV_WorldNavigation.h"

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
    static constexpr int BOARD_SECONDS = 5;     // Open the quest panel, read it, close it.
    static constexpr int REFLY_SECONDS = 20;    // Fly to the closest fly point, wait 3 s, walk in.
    static constexpr int LEAVE_SECONDS = 28;    // Fly back to Central Plaza at the end.

    // Kills heard in each clear, in turn. Running past the end throws, which
    // also stops a loop that would never end.
    std::vector<uint64_t> kills_per_clear;
    // The lead's HP read in each clear, in turn. Past the end, it is unknown.
    std::vector<std::optional<double>> hp_per_clear;
    // Kills heard on each flight back, in turn: the lead finishing a kill
    // after the clear ends. Past the end, none.
    std::vector<uint64_t> kills_per_return;
    // Whether each board read finds the quest, in turn. Past the end, it does.
    std::vector<bool> quest_on_board_per_read;

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
    virtual void refly_to_battle_area() override{
        steps.push_back("refly");
        seconds += REFLY_SECONDS;
    }
    virtual void leave() override{
        steps.push_back("leave");
        seconds += LEAVE_SECONDS;
    }
    virtual void return_to_plaza() override{
        steps.push_back("return");
        seconds += RETURN_SECONDS;
        if (m_returns < kills_per_return.size()){
            m_kills += kills_per_return[m_returns];
        }
        m_returns++;
    }
    virtual bool quest_on_board() override{
        steps.push_back("board");
        seconds += BOARD_SECONDS;
        const bool on_board = m_reads >= quest_on_board_per_read.size() || quest_on_board_per_read[m_reads];
        m_reads++;
        return on_board;
    }
    virtual void heal() override{
        steps.push_back("heal");
        seconds += HEAL_SECONDS;
    }

private:
    uint64_t m_kills = 0;
    size_t m_clears = 0;
    size_t m_returns = 0;
    size_t m_reads = 0;
};

// One pass as it shows in the expected steps.
const std::string PASS = "fly clear return heal";
// The last pass: it leaves without healing.
const std::string LAST_PASS = "fly clear leave";

std::string passes(int count, const std::string& pass = PASS){
    std::vector<std::string> ret(count, pass);
    return join(ret, " ");
}

std::string auto_battle_result_to_string(BBQAutoBattleResult result){
    switch (result){
    case BBQAutoBattleResult::TargetReached:    return "TargetReached";
    case BBQAutoBattleResult::NoKills:          return "NoKills";
    case BBQAutoBattleResult::TimeLimit:        return "TimeLimit";
    case BBQAutoBattleResult::QuestDone:        return "QuestDone";
    }
    return "?";
}

struct BBQAutoBattleScenario{
    std::string name;
    uint64_t kill_target = 10;
    int time_limit_minutes = 35;
    std::vector<uint64_t> kills_per_clear;
    std::vector<std::optional<double>> hp_per_clear = {}; // Unknown when not given.
    std::vector<uint64_t> kills_per_return = {};        // None when not given.
    uint64_t check_board_within_kills = 0;              // Off unless a scenario tests it.
    std::vector<bool> quest_on_board_per_read = {};     // On the board when not given.
    int heal_below_hp_percent = 50;
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
        actions.hp_per_clear = m_scenario.hp_per_clear;
        actions.kills_per_return = m_scenario.kills_per_return;
        actions.quest_on_board_per_read = m_scenario.quest_on_board_per_read;

        RecordingLogger logger;
        BBQAutoBattlePolicy policy;
        policy.kill_target = m_scenario.kill_target;
        policy.time_limit = std::chrono::minutes(m_scenario.time_limit_minutes);
        policy.heal_below_hp_percent = (uint8_t)m_scenario.heal_below_hp_percent;
        policy.check_board_within_kills = m_scenario.check_board_within_kills;
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
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP ?, 448 s elapsed. Target reached: leave without healing.",
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
                "Auto Battle pass 3: 0 kills, 0/10 in total, HP ?, 250 s elapsed. No kill in 3 passes in a row: leave and give up.",
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
                "Auto Battle pass 4: 1 kills, 4/10 in total, HP ?, 352 s elapsed. Time limit of 5 min reached: leave and give up.",
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
            .expected_steps = "fly clear refly clear refly clear refly clear refly clear leave",
            .expected_seconds = 252,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP 80%, 46 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP 80%, 92 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 3: 2 kills, 6/10 in total, HP 80%, 138 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 4: 2 kills, 8/10 in total, HP 80%, 184 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP 80%, 224 s elapsed. Target reached: leave without healing.",
            },
        },
        {
            // Same kills as "auto battle 30 at 3 kills per pass", 9 heals (252 s) faster.
            .name = "high HP skips the heal in auto battle 30",
            .kill_target = 30,
            .kills_per_clear = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
            .hp_per_clear = {0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9, 0.9},
            .expected_steps = "fly clear " + passes(9, "refly clear") + " leave",
            .expected_seconds = 482,
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
                "Auto Battle pass 5: 2 kills, 10/10 in total, HP 30%, 448 s elapsed. Target reached: leave without healing.",
            },
        },
        {
            // Unknown HP heals, as every pass did before the threshold.
            .name = "unknown HP heals",
            .kills_per_clear = {2, 2, 2, 2, 2},
            .hp_per_clear = {0.8, std::nullopt, 0.8, std::nullopt, 0.8},
            .expected_steps = "fly clear refly clear return heal fly clear refly clear return heal fly clear leave",
            .expected_seconds = 364,
        },
        {
            // HP at the threshold skips the heal; just below heals.
            .name = "heal threshold boundary",
            .kills_per_clear = {3, 3, 3, 3},
            .hp_per_clear = {0.5, 0.49, 0.5, 0.5},
            .expected_steps = "fly clear refly clear return heal fly clear refly clear leave",
            .expected_seconds = 262,
            .expected_log = {
                "Auto Battle pass 1: 3 kills, 3/10 in total, HP 50%, 46 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 2: 3 kills, 6/10 in total, HP 49%, 92 s elapsed. Below the target and HP below 50%: return, heal and go again.",
                "Auto Battle pass 3: 3 kills, 9/10 in total, HP 50%, 194 s elapsed. Below the target and HP at or above 50%: fly back in without healing.",
                "Auto Battle pass 4: 3 kills, 12/10 in total, HP 50%, 234 s elapsed. Target reached: leave without healing.",
            },
        },
        {
            .name = "a threshold of 100% heals unless HP is full",
            .kills_per_clear = {4, 4, 4},
            .hp_per_clear = {1.0, 0.99, 1.0},
            .heal_below_hp_percent = 100,
            .expected_steps = "fly clear refly clear return heal fly clear leave",
            .expected_seconds = 216,
        },
        {
            .name = "a threshold of 0% heals only when HP is unknown",
            .kills_per_clear = {4, 4, 4},
            .hp_per_clear = {0.01, std::nullopt, 0.01},
            .heal_below_hp_percent = 0,
            .expected_steps = "fly clear refly clear return heal fly clear leave",
            .expected_seconds = 216,
        },
        {
            // The limits still end the attempt when no pass heals.
            .name = "gives up after 3 passes with no kill and high HP",
            .kills_per_clear = {0, 0, 0},
            .hp_per_clear = {1.0, 1.0, 1.0},
            .expected_result = BBQAutoBattleResult::NoKills,
            .expected_steps = "fly clear refly clear refly clear leave",
            .expected_seconds = 166,
        },
        {
            // R3b: "pass 2: 1 kills, 4/10 in total" after "2/10". A kill heard after a pass's
            // line counts in the next one, so the passes add up to the total.
            .name = "a kill heard on the way back counts in the next pass",
            .kills_per_clear = {2, 1, 3, 4},
            .kills_per_return = {1},
            .expected_steps = passes(3) + " " + LAST_PASS,
            .expected_seconds = 4 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 2 kills, 2/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 2 kills, 4/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 3: 3 kills, 7/10 in total, HP ?, 250 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 4: 4 kills, 11/10 in total, HP ?, 346 s elapsed. Target reached: leave without healing.",
            },
        },
        {
            // A pass whose only kill was heard on the previous flight back is not empty.
            .name = "a kill heard on the way back resets the empty passes",
            .kills_per_clear = {0, 0, 0, 0, 0, 0},
            .kills_per_return = {0, 1},
            .expected_result = BBQAutoBattleResult::NoKills,
            .expected_steps = passes(5) + " " + LAST_PASS,
            .expected_seconds = 6 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
        },
        {
            .name = "a kill heard on the way back that reaches the target skips the heal",
            .kills_per_clear = {3, 3, 3},
            .kills_per_return = {0, 0, 1},
            .expected_steps = passes(2) + " fly clear return",
            .expected_seconds = 3 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 3 kills, 3/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 3 kills, 6/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 3: 3 kills, 9/10 in total, HP ?, 250 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle: kills heard on the way back reached the target, 10/10. Stop without healing.",
            },
        },
        {
            // R3b: 9 kills heard, but the game had completed the quest. The board read
            // replaces the 5th pass and its heal.
            .name = "near the target, a board without the quest ends it",
            .kills_per_clear = {3, 3, 3},
            .check_board_within_kills = 2,
            .quest_on_board_per_read = {false},
            .expected_result = BBQAutoBattleResult::QuestDone,
            .expected_steps = passes(2) + " fly clear board leave",
            .expected_seconds = 3 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS + ScriptedAutoBattle::BOARD_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 3 kills, 3/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 3 kills, 6/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 3: 3 kills, 9/10 in total, HP ?, 250 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle: the quest is no longer on the board at 9/10 kills heard. Stop: the game completed it.",
            },
        },
        {
            // Each pass near the target reads the board until the quest is gone.
            .name = "near the target, a board with the quest goes again",
            .kills_per_clear = {4, 4, 1, 1},
            .check_board_within_kills = 2,
            .quest_on_board_per_read = {true, false},
            .expected_result = BBQAutoBattleResult::QuestDone,
            .expected_steps = PASS + " fly clear board return heal fly clear board leave",
            .expected_seconds = 3 * PASS_SECONDS - ScriptedAutoBattle::HEAL_SECONDS + 2 * ScriptedAutoBattle::BOARD_SECONDS,
            .expected_log = {
                "Auto Battle pass 1: 4 kills, 4/10 in total, HP ?, 46 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle pass 2: 4 kills, 8/10 in total, HP ?, 148 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle: the quest is still on the board at 8/10 kills heard. Go again.",
                "Auto Battle pass 3: 1 kills, 9/10 in total, HP ?, 255 s elapsed. Below the target and HP unknown: return, heal and go again.",
                "Auto Battle: the quest is no longer on the board at 9/10 kills heard. Stop: the game completed it.",
            },
        },
        {
            // Far from the target the board is never read, and reaching the target
            // needs no read.
            .name = "far from the target, no board read",
            .kills_per_clear = {5, 5},
            .check_board_within_kills = 2,
            .quest_on_board_per_read = {false},
            .expected_steps = PASS + " " + LAST_PASS,
            .expected_seconds = 2 * PASS_SECONDS - LAST_PASS_SAVED_SECONDS,
        },
        {
            // High HP skips the heal but not the read.
            .name = "near the target with high HP, read the board without healing",
            .kill_target = 30,
            .kills_per_clear = {10, 10, 9, 1},
            .hp_per_clear = {0.9, 0.9, 0.9, 0.9},
            .check_board_within_kills = 2,
            .expected_steps = "fly clear refly clear refly clear board refly clear leave",
            .expected_seconds = 211,
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

// The quests the BBQ scenarios treat as not possible. The program can do them
// now, but the planner tests need quests it cannot do.
const std::set<BBQuests> TEST_NOT_POSSIBLE_QUESTS = {
    BBQuests::UnableToDetect, BBQuests::pickup_10, BBQuests::photo_flying, BBQuests::catch_flying
};

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
    std::vector<std::pair<BBQuests, BBQAction>> extra_exclusions = {};
    std::optional<int> bp = 1000;
    uint8_t num_eggs = 1;
    uint8_t eggs_hatched = 0;
    BBQOption::OOEggs out_of_eggs = BBQOption::OOEggs::Reroll;
    std::vector<BBQuests> expected_to_do = {};
    std::vector<size_t> expected_reroll_slots = {};
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
        options.NOT_POSSIBLE_QUESTS = TEST_NOT_POSSIBLE_QUESTS;
        options.NUM_EGGS.set(m_scenario.num_eggs);
        options.OUT_OF_EGGS.set(m_scenario.out_of_eggs);
        for (const auto& [quest, action] : m_scenario.extra_exclusions){
            auto row = std::make_unique<BBQuestTableRow>(options.QUEST_EXCLUSIONS);
            row->quest.set(quest);
            row->action.set(action);
            options.QUEST_EXCLUSIONS.append_row(std::move(row));
        }

        NullLogger logger;
        const BBQuestPlan plan = plan_quests(logger, options, m_scenario.quests, m_scenario.eggs_hatched, m_scenario.bp);
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
    bool slot_reads = false;    // Off, reads in the open panel miss and the whole panel is read.

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
    virtual BBQuests read_slot_in_panel(size_t slot) override{
        if (!slot_reads || !panel_open){
            return BBQuests::UnableToDetect;
        }
        return quests[slot];
    }
};

struct BBQRerollScenario{
    std::string name;
    std::vector<BBQuests> quests;
    std::vector<BBQuests> replacements = {};
    size_t slot;
    size_t cursor = 0;
    bool panel_opens = true;
    bool menu_seen = true;
    bool out_of_bp = false;
    int missed_reads = 0;
    std::optional<std::vector<BBQuests>> read_override = {};
    bool slot_reads = false;
    std::vector<BBQuests> reroll_again = {};
    BBQRerollResult expected;
    std::vector<size_t> expected_rerolled_slots = {};
    std::vector<BBQuests> expected_quests;
    std::optional<std::vector<BBQuests>> expected_observed_panel = {};
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
        panel.slot_reads = m_scenario.slot_reads;
        const std::vector<BBQuests>& again = m_scenario.reroll_again;

        RecordingLogger logger;
        std::vector<BBQuests> quests = m_scenario.quests;
        std::optional<std::vector<BBQuests>> observed_panel;
        const BBQRerollResult result = reroll_quest(
            logger, panel, quests, m_scenario.slot, observed_panel,
            [&](BBQuests quest){ return std::find(again.begin(), again.end(), quest) != again.end(); }
        );
        //Each reroll logs one line naming its slot and quest, whatever the result.
        const std::string reroll_line = "Rerolling quest " + std::to_string(m_scenario.slot + 1)
            + " (" + BBQuests_enum_to_string(m_scenario.quests[m_scenario.slot]) + ").";
        const size_t reroll_lines = std::count_if(logger.lines.begin(), logger.lines.end(), [](const std::string& line){
            return line.starts_with("Rerolling quest ") && line.find(" again ") == std::string::npos;
        });
        if (reroll_lines != 1 || std::find(logger.lines.begin(), logger.lines.end(), reroll_line) == logger.lines.end()){
            return "Expected one log line \"" + reroll_line + "\", got " + std::to_string(reroll_lines) + " reroll lines.";
        }
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
            // Left alone, it would hold its slot until something else is
            // rerolled. A blue reroll costs 10 BP.
            .name = "a blue quest not possible is rerolled while others are doable",
            .quests = {make_tm, catch_flying, sneak_up},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {1},
        },
        {
            // An undetected quest is neither run nor rerolled.
            .name = "a quest not possible is rerolled, an undetected one is not",
            .quests = {pickup_10, make_tm, UnableToDetect},
            .expected_to_do = {make_tm},
            .expected_reroll_slots = {0},
        },
        {
            // A red quest costs 50 BP to reroll, but pays 100-150 BP, and
            // the next red quest cannot appear while it stays.
            .name = "a red quest not possible is rerolled while the blues are doable",
            .quests = {photo_flying, make_tm, sneak_up, travel_500},
            .expected_to_do = {make_tm, sneak_up, travel_500},
            .expected_reroll_slots = {0},
        },
        {
            // The board that held 2 of 4 slots for 20 minutes.
            .name = "a red and a blue quest not possible are both rerolled",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {0, 2},
        },
        {
            // There is no longer a single slot to fall back on: every quest
            // not possible was rerolled already, so not every quest is.
            .name = "nothing doable rerolls each quest not possible, not all",
            .quests = {photo_flying, auto_10, pickup_10, catch_flying},
            .expected_reroll_slots = {0, 2, 3},
        },
        {
            // The table doesn't apply to quests not possible: they are always rerolled.
            .name = "a skip row doesn't keep a red quest not possible",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .extra_exclusions = {{photo_flying, BBQAction::skip}},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {0, 2},
        },
        {
            .name = "a run row doesn't keep a blue quest not possible",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .extra_exclusions = {{catch_flying, BBQAction::run}},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {0, 2},
        },
        {
            // A red reroll costs 50 BP, a blue one 10.
            .name = "BP for a blue reroll but not a red one skips the red quest not possible",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .bp = 49,
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {2},
        },
        {
            .name = "BP for exactly a red reroll rerolls it and skips the blue one",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .bp = 55,
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {0},
        },
        {
            // Before, the reroll did not happen and the program stopped.
            .name = "BP under a blue reroll skips quests not possible and runs the doable ones",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .bp = 9,
            .expected_to_do = {make_tm, sneak_up},
        },
        {
            .name = "unknown BP skips quests not possible and runs the doable ones",
            .quests = {photo_flying, make_tm, catch_flying, sneak_up},
            .bp = std::nullopt,
            .expected_to_do = {make_tm, sneak_up},
        },
        {
            // A reroll row is paid for first.
            .name = "BP left after a reroll row decides the quests not possible",
            .quests = {make_tm, catch_flying, sneak_up, pickup_10},
            .extra_exclusions = {{sneak_up, BBQAction::reroll}},
            .bp = 25,
            .expected_to_do = {make_tm},
            .expected_reroll_slots = {1, 2},
        },
        {
            // Nothing else changes the panel, so BP does not hold it back.
            .name = "nothing doable rerolls each quest not possible whatever the BP",
            .quests = {photo_flying, auto_10, catch_flying},
            .bp = 0,
            .expected_reroll_slots = {0, 2},
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
            .name = "a skip row doesn't stop a quest not possible being rerolled",
            .quests = {pickup_10, auto_10, catch_flying},
            .extra_exclusions = {{pickup_10, BBQAction::skip}},
            .expected_reroll_slots = {0, 2},
        },
        {
            .name = "skip rows on every quest not possible still reroll each of them",
            .quests = {photo_flying, auto_10, catch_flying},
            .extra_exclusions = {
                {photo_flying, BBQAction::skip},
                {catch_flying, BBQAction::skip},
            },
            .expected_reroll_slots = {0, 2},
        },
        {
            .name = "a reroll row rerolls a quest not possible",
            .quests = {make_tm, sneak_up, pickup_10},
            .extra_exclusions = {{pickup_10, BBQAction::reroll}},
            .expected_to_do = {make_tm, sneak_up},
            .expected_reroll_slots = {2},
        },
        {
            .name = "a run row rerolls a quest not possible instead",
            .quests = {make_tm, pickup_10},
            .extra_exclusions = {{pickup_10, BBQAction::run}},
            .expected_to_do = {make_tm},
            .expected_reroll_slots = {1},
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
            .expected_reroll_slots = {0, 1},
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
            .name = "the new quest is read in the open panel",
            .quests = {make_tm, sneak_up, travel_500, hatch_egg},
            .replacements = {catch_any},
            .slot = 2,
            .slot_reads = true,
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {2},
            .expected_quests = {make_tm, sneak_up, catch_any, hatch_egg},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, sneak_up, catch_any, hatch_egg},
        },
        {
            .name = "a new quest to reroll is rerolled again in the open panel",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {auto_10, catch_any, catch_bug},
            .slot = 1,
            .slot_reads = true,
            .reroll_again = {auto_10, catch_any},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {1, 1, 1},
            .expected_quests = {make_tm, catch_bug, travel_500},
            .expected_observed_panel = std::vector<BBQuests>{make_tm, catch_bug, travel_500},
        },
        {
            // The second reroll may not have happened, so the panel is read again.
            .name = "out of BP while rerolling again leaves the panel to be read",
            .quests = {make_tm, sneak_up, travel_500},
            .replacements = {auto_10, auto_10},
            .slot = 1,
            .slot_reads = true,
            .reroll_again = {auto_10},
            .expected = BBQRerollResult::Rerolled,
            .expected_rerolled_slots = {1, 1},
            .expected_quests = {make_tm, auto_10, travel_500},
        },
        {
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
// in TEST_NOT_POSSIBLE_QUESTS are skipped, quests in table_rerolls are rerolled
// in their slot as an exclusions table reroll row does, and if nothing is
// doable all quests are rerolled. After any reroll it returns nothing to do.
// Red quests are not modeled. Records the game actions taken.
// With out_of_bp, rerolls do nothing, as in the game when BP runs out.
// With plan_options, process_quest_list() decides with plan_quests() instead.
class SimulatedBBQGameActions : public BBQGameActions{
public:
    // Thrown where reroll_quest_slot() stops the program because a reroll
    // did not replace the quest, which is what running out of BP looks like.
    struct RerollNotReplaced{};

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
    // When set, process_quest_list() plans with plan_quests(), as the console
    // does: each slot it rerolls is replaced, or out of BP, the program stops.
    const BBQOption* plan_options = nullptr;

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
        if (plan_options){
            return process_with_plan(quests);
        }
        bool rerolled = false;
        for (BBQuests& slot : panel){
            if (!out_of_bp && table_rerolls.contains(slot)){
                slot = next_new_quest();
                rerolled = true;
            }
        }
        std::vector<BBQuests> doable;
        for (BBQuests quest : quests){
            if (!TEST_NOT_POSSIBLE_QUESTS.contains(quest)){
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
    virtual bool do_quest(BBQuests quest, uint8_t&, std::vector<BBQuests>& panel_after) override{
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
        panel_after = panel;
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
    BBQuestBatch process_with_plan(const std::vector<BBQuests>& quests){
        NullLogger logger;
        //Out of BP, plan_quests() skips a quest not possible next to doable ones.
        BBQuestPlan plan = plan_quests(logger, *plan_options, quests, 0, out_of_bp ? 0 : 1000);
        for (size_t slot : plan.reroll_slots){
            events.push_back("reroll(slot)");
            if (out_of_bp){
                throw RerollNotReplaced();
            }
            panel[slot] = next_new_quest();
        }
        if (plan.reroll_all){
            events.push_back("reroll(all)");
            if (!out_of_bp){
                for (BBQuests& slot : panel){
                    slot = next_new_quest();
                }
            }
        }
        if (!plan.reroll_slots.empty() || plan.reroll_all){
            return {{}, reuse_rerolled_panel ? std::optional(panel) : std::nullopt};
        }
        return {std::move(plan.to_do), std::nullopt};
    }

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
    std::vector<bool> outcomes = {};
    std::vector<std::pair<BBQuests, BBQuests>> also_completes = {};
    std::set<BBQuests> never_completes = {};
    std::vector<BBQuests> new_quests = {};
    BBQOption::OutOfRetries out_of_retries = BBQOption::OutOfRetries::Skip;
    bool retry_later = false;
    std::vector<std::pair<BBQuests, BBQAction>> exclusions = {};
    bool out_of_bp = false;
    std::set<BBQuests> table_rerolls = {};
    bool reuse_rerolled_panel = false;
    bool plan_with_plan_quests = false;
    uint64_t expected_completed = 0;
    std::vector<BBQuests> expected_attempted = {};
    std::optional<size_t> expected_reads = {};
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
        options.NOT_POSSIBLE_QUESTS = TEST_NOT_POSSIBLE_QUESTS;
        options.NUM_QUESTS.set(m_scenario.num_quests);
        options.SAVE_NUM_QUESTS.set(m_scenario.save_num_quests);
        if (m_scenario.stop_when_out_of_eggs){
            options.NUM_EGGS.set(0);
            options.OUT_OF_EGGS.set(BBQOption::OOEggs::Stop);
        }
        options.OUT_OF_RETRIES.set(m_scenario.out_of_retries);
        options.RETRY_LATER = m_scenario.retry_later;
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
        if (m_scenario.plan_with_plan_quests){
            game.plan_options = &options;
        }

        NullLogger logger;
        try{
            run_bbq_rounds(logger, options, game);
        }catch (UserSetupError&){
            game.events.push_back("error");
        }catch (SimulatedBBQGameActions::RerollNotReplaced&){
            game.events.push_back("stopped");
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
        if (!TEST_NOT_POSSIBLE_QUESTS.contains(quest)){
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
    std::string read_and_reroll_30_times;
    for (int c = 0; c < 30; c++){
        read_and_reroll_30_times += "read process reroll(slot) ";
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
            .expected_attempted = {BBQuests::make_tm, BBQuests::travel_500, BBQuests::photo_savanna, BBQuests::catch_grass},
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
            .expected_attempted = {BBQuests::make_tm, BBQuests::travel_500, BBQuests::photo_savanna, BBQuests::catch_grass, BBQuests::wash_pokemon},
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
            // Lab R7: sneak-up (no cost measured) replaces make-tm, so #28's
            // ordering keeps catch-fighting first.
            .name = "failed reroll then table reroll reuse verified reads",
            .num_quests = 1,
            .panel = {BBQuests::catch_fighting, BBQuests::sneak_up, BBQuests::photo_savanna},
            .outcomes = {false},
            .new_quests = {BBQuests::pickup_10, BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .table_rerolls = {BBQuests::pickup_10},
            .reuse_rerolled_panel = true,
            .expected_completed = 3,
            .expected_attempted = {BBQuests::catch_fighting, BBQuests::travel_500, BBQuests::sneak_up, BBQuests::photo_savanna},
            .expected_reads = 3,
            .expected = "read process do reroll end process process do completed read do completed read do completed end",
        },
        {
            // As above, but neither reroll's read is reusable. Only the
            // first dispatch skips a read: 2 reads before the next attempt
            // instead of 3.
            .name = "failed reroll then table reroll without reusable reads",
            .num_quests = 1,
            .panel = {BBQuests::catch_fighting, BBQuests::sneak_up, BBQuests::photo_savanna},
            .outcomes = {false},
            .new_quests = {BBQuests::pickup_10, BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .out_of_retries = BBQOption::OutOfRetries::Reroll,
            .table_rerolls = {BBQuests::pickup_10},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::catch_fighting, BBQuests::travel_500, BBQuests::sneak_up, BBQuests::photo_savanna},
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
            // photo_swim runs first, as the most BP per second.
            .name = "quest already gone counts as completed",
            .num_quests = 3,
            .panel = {BBQuests::photo_swim, BBQuests::travel_500, BBQuests::make_tm},
            .also_completes = {{BBQuests::photo_swim, BBQuests::travel_500}},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::photo_swim, BBQuests::make_tm},
            //make-tm's read after it shows travel-500 gone (no-trip reuse).
            .expected = "read process do completed read do completed completed end",
        },
        {
            // Expected seconds per completion: make-tm 79, travel-500 89,
            // auto-10 280.
            .name = "doable quests run cheapest first",
            .num_quests = 3,
            .panel = {BBQuests::auto_10, BBQuests::travel_500, BBQuests::make_tm},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::make_tm, BBQuests::travel_500, BBQuests::auto_10},
            //travel-500 starts from make-tm's read (no-trip chaining).
            .expected = "read process do completed do completed read do completed end",
        },
        {
            // photo-swim 63 s at 71% (89 s), typed catch 137 s at 75%
            // (183 s), tera-self 153 s at 59% (259 s).
            .name = "success rate counts in the order",
            .num_quests = 3,
            .panel = {BBQuests::tera_self_defeat, BBQuests::catch_fire, BBQuests::photo_swim},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::photo_swim, BBQuests::catch_fire, BBQuests::tera_self_defeat},
            .expected = round_of_3 + " end",
        },
        {
            // sneak_up has no measurement, so it stays in slot 2.
            .name = "unmeasured quests keep their place",
            .num_quests = 3,
            .panel = {BBQuests::auto_10, BBQuests::sneak_up, BBQuests::make_tm},
            .expected_completed = 3,
            .expected_attempted = {BBQuests::make_tm, BBQuests::sneak_up, BBQuests::auto_10},
            .expected = round_of_3 + " end",
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
            // With retry later, make-tm fails, the other two run, and make-tm
            // is tried once more at the end of the batch.
            .name = "retry later tries a failed quest after the rest of the batch",
            .num_quests = 3,
            .outcomes = {false},
            .retry_later = true,
            .expected_completed = 3,
            .expected_attempted = {BBQuests::make_tm, BBQuests::auto_10, BBQuests::sneak_up, BBQuests::make_tm},
            .expected = "read process do read do completed read do completed read do completed end",
        },
        {
            // A second failure gets the out-of-retries action as before.
            .name = "retry later fails a quest once more after the batch",
            .num_quests = 2,
            .outcomes = {false, true, true, false},
            .retry_later = true,
            .expected_completed = 2,
            .expected_attempted = {BBQuests::make_tm, BBQuests::auto_10, BBQuests::sneak_up, BBQuests::make_tm},
            .expected = "read process do read do completed read do completed read do end",
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
            // plans round 2 from a full panel without it (make-tm's read, no
            // round actions), and its replacement for photo-canyon brings it
            // back. Round 3 attempts it again.
            .name = "failed quest that leaves a full panel is attempted again",
            .num_quests = 5,
            .panel = {BBQuests::sneak_up, BBQuests::make_tm, BBQuests::pickup_10, BBQuests::catch_flying},
            .outcomes = {false},
            .also_completes = {{BBQuests::make_tm, BBQuests::sneak_up}},
            .new_quests = {BBQuests::auto_10, BBQuests::photo_canyon, BBQuests::sneak_up},
            .expected_completed = 5,
            .expected =
                "read process do read do completed "
                "process do completed read do completed end "
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
                "read process do read do completed "
                "process do completed read do completed end "
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
        {
            // plan_quests() rerolls a quest not possible on sight, even next
            // to doable quests. The reroll empties the batch, so the doable
            // quests run from the next read, one read later than before.
            .name = "a quest not possible is rerolled before the doable quests run",
            .num_quests = 3,
            .panel = {BBQuests::make_tm, BBQuests::catch_flying, BBQuests::sneak_up},
            .new_quests = {BBQuests::travel_500, BBQuests::auto_10, BBQuests::wash_pokemon},
            .plan_with_plan_quests = true,
            .expected_completed = 3,
            .expected_attempted = {BBQuests::make_tm, BBQuests::travel_500, BBQuests::sneak_up},
            //travel-500 starts from make-tm's read (no-trip chaining).
            .expected = "read process reroll(slot) read process do completed do completed read do completed end",
        },
        {
            // A panel whose doable quests sit next to a quest not possible
            // counts toward the read limit, since its batch is empty. Only
            // rerolls that keep drawing quests not possible reach it.
            .name = "rerolls that keep drawing quests not possible reach the read limit",
            .num_quests = 3,
            .panel = {BBQuests::make_tm, BBQuests::catch_flying, BBQuests::sneak_up},
            .new_quests = {BBQuests::pickup_10, BBQuests::catch_flying},
            .plan_with_plan_quests = true,
            .expected_completed = 0,
            .expected = read_and_reroll_30_times + "error",
        },
        {
            // Out of BP, plan_quests() does not reroll a quest not possible
            // next to doable quests (#25 follow-up): it is skipped and the
            // doable quests run, as before C15.
            .name = "out of BP, a quest not possible is skipped and the doable quests run",
            .num_quests = 2,
            .panel = {BBQuests::make_tm, BBQuests::catch_flying, BBQuests::sneak_up},
            .out_of_bp = true,
            .plan_with_plan_quests = true,
            .expected_completed = 2,
            .expected_attempted = {BBQuests::make_tm, BBQuests::sneak_up},
            .expected = "read process do completed read do completed end",
        },
        {
            // Out of BP, nothing doable: plan_quests() rerolls a quest not possible
            // even though the table skips it, the reroll does not happen, and the
            // program stops.
            .name = "out of BP, quests not possible stop the program when their reroll fails",
            .num_quests = 3,
            .panel = {BBQuests::pickup_10, BBQuests::catch_flying, BBQuests::UnableToDetect},
            .exclusions = {{BBQuests::pickup_10, BBQAction::skip}, {BBQuests::catch_flying, BBQAction::skip}},
            .out_of_bp = true,
            .plan_with_plan_quests = true,
            .expected_completed = 0,
            .expected = "read process reroll(slot) stopped",
        },
        {
            // make-tm ends round 1 back at Central Plaza: its read plans
            // round 2 with no round actions between it and travel-500.
            .name = "no-trip quests chain with no round actions between",
            .num_quests = 5,
            .panel = {BBQuests::photo_savanna, BBQuests::sneak_up, BBQuests::make_tm},
            .new_quests = {BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .expected_completed = 6,
            .expected_attempted = {
                BBQuests::photo_savanna, BBQuests::sneak_up, BBQuests::make_tm,
                BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal,
            },
            .expected_reads = 5,
            .expected =
                "read process do completed read do completed read do completed "
                "process do completed read do completed read do completed end",
        },
        {
            // make-tm, travel-500 and photo-canyon on one board: only
            // travel-500 starts from make-tm's read. The trip quest reads
            // the panel and ends the round as usual.
            .name = "no-trip then trip quest keeps the trip's usual steps",
            .num_quests = 3,
            .panel = {BBQuests::make_tm, BBQuests::travel_500, BBQuests::photo_canyon},
            .new_quests = {BBQuests::photo_coastal, BBQuests::photo_polar, BBQuests::photo_savanna},
            .expected_completed = 3,
            .expected_reads = 2,
            .expected = "read process do completed do completed read do completed end",
        },
        {
            .name = "failed no-trip quest ends the round as usual",
            .num_quests = 3,
            .panel = {BBQuests::photo_savanna, BBQuests::sneak_up, BBQuests::make_tm},
            .outcomes = {true, true, false},
            .new_quests = {BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .expected_completed = 4,
            .expected =
                "read process do completed read do completed read do end "
                "read process do completed read do completed end",
        },
        {
            .name = "no-trip quest does not chain past a save",
            .num_quests = 6,
            .save_num_quests = 3,
            .panel = {BBQuests::photo_savanna, BBQuests::sneak_up, BBQuests::make_tm},
            .new_quests = {BBQuests::travel_500, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar},
            .expected_completed = 6,
            .expected =
                "read process do completed read do completed read do completed end(save) "
                "read process do completed read do completed read do completed end(save)",
        },
        {
            .name = "no-trip quest that reaches the goal ends the round",
            .num_quests = 3,
            .panel = {BBQuests::photo_savanna, BBQuests::sneak_up, BBQuests::make_tm},
            .expected_completed = 3,
            .expected = "read process do completed read do completed read do completed end",
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
    std::vector<Read> reads = {};
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

// A quest, a BP/min threshold, and whether the quest is rerolled for paying below it.
struct BBQRerollThresholdScenario{
    std::string name;
    BBQuests quest;
    uint8_t bp_per_min;
    bool expected_reroll = false;
};

class Test_BBQRerollThreshold : public UnitTest{
public:
    Test_BBQRerollThreshold(BBQRerollThresholdScenario scenario)
        : UnitTest("PokemonSV::BBQRerollThreshold - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool got = below_reroll_threshold(m_scenario.quest, m_scenario.bp_per_min);
        if (got != m_scenario.expected_reroll){
            return std::string("Expected ") + (m_scenario.expected_reroll ? "reroll" : "run")
                + ", got " + (got ? "reroll" : "run");
        }
        return true;
    }

private:
    const BBQRerollThresholdScenario m_scenario;
};

void add_tests_BBQRerollThreshold(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQRerollThresholdScenario scenarios[] = {
        {
            // 20 + 11 BP in 280 s: 31 + 10 + 10/6 < 10 x 4.67.
            .name = "auto battle 10 is rerolled at 10 BP/min",
            .quest = auto_10,
            .bp_per_min = 10,
            .expected_reroll = true,
        },
        {
            .name = "auto battle 10 runs at 8 BP/min",
            .quest = auto_10,
            .bp_per_min = 8,
        },
        {
            // 40 + 11 BP in 183 s.
            .name = "a typed catch runs at 10 BP/min",
            .quest = catch_bug,
            .bp_per_min = 10,
        },
        {
            .name = "make tm runs at 10 BP/min",
            .quest = make_tm,
            .bp_per_min = 10,
        },
        {
            // Red: a reroll costs 50 BP, and it keeps the next red quest away.
            .name = "a red quest is never below",
            .quest = hatch_egg,
            .bp_per_min = 60,
        },
        {
            .name = "a quest without measurements is never below",
            .quest = photo_coastal,
            .bp_per_min = 60,
        },
        {
            .name = "0 turns it off",
            .quest = auto_10,
            .bp_per_min = 0,
        },
    };
    for (const BBQRerollThresholdScenario& scenario : scenarios){
        database.add<Test_BBQRerollThreshold>(scenario);
    }
}

// A catch quest, the species read from the opponent's status screen, and whether
// it counts for the quest (nothing: no table or nothing read, so catch anyway).
struct BBQCatchCountsScenario{
    std::string name;
    BBQuests quest;
    std::set<std::string> slugs;
    std::optional<bool> expected = std::nullopt;
};

std::string describe(const std::optional<bool>& counts){
    return counts ? (*counts ? "counts" : "wrong type") : "unknown";
}

class Test_BBQCatchCounts : public UnitTest{
public:
    Test_BBQCatchCounts(BBQCatchCountsScenario scenario)
        : UnitTest("PokemonSV::BBQCatchCounts - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<bool> got = catch_quest_counts(m_scenario.quest, m_scenario.slugs);
        if (got != m_scenario.expected){
            return "Expected " + describe(m_scenario.expected) + ", got " + describe(got);
        }
        return true;
    }

private:
    const BBQCatchCountsScenario m_scenario;
};

void add_tests_BBQCatchCounts(UnitTestDatabase& database){
    using enum BBQuests;
    const BBQCatchCountsScenario scenarios[] = {
        {
            // The Coastal grassland spawns: Pikipek 36%, Trumbeak 10%, Toucannon 6%.
            .name = "pikipek counts for catch flying",
            .quest = catch_flying,
            .slugs = {"pikipek"},
            .expected = true,
        },
        {
            .name = "toucannon counts for catch flying",
            .quest = catch_flying,
            .slugs = {"toucannon"},
            .expected = true,
        },
        {
            // Bug/Flying: the canyon route's Scyther counts.
            .name = "scyther counts for catch flying",
            .quest = catch_flying,
            .slugs = {"scyther"},
            .expected = true,
        },
        {
            // Bug/Rock: the fixed canyon Kleavor that R15 fought twice.
            .name = "kleavor is the wrong type for catch flying",
            .quest = catch_flying,
            .slugs = {"kleavor"},
            .expected = false,
        },
        {
            // Grass/Poison, next to the Coastal Plaza spawns.
            .name = "oddish is the wrong type for catch flying",
            .quest = catch_flying,
            .slugs = {"oddish"},
            .expected = false,
        },
        {
            // OCR can return more than one candidate. When they disagree the read
            // cannot tell, and it is caught anyway.
            .name = "candidates that disagree are unknown",
            .quest = catch_flying,
            .slugs = {"oddish", "pikipek"},
        },
        {
            .name = "candidates that all count, count",
            .quest = catch_flying,
            .slugs = {"trumbeak", "pikipek"},
            .expected = true,
        },
        {
            .name = "unread name is unknown",
            .quest = catch_flying,
            .slugs = {},
        },
        {
            // Every typed catch reads the species now.
            .name = "scyther counts for catch bug",
            .quest = catch_bug,
            .slugs = {"scyther"},
            .expected = true,
        },
        {
            // R16: the catch-bug lead fainted to the fixed Kleavor at the court.
            .name = "kleavor counts for catch bug",
            .quest = catch_bug,
            .slugs = {"kleavor"},
            .expected = true,
        },
        {
            .name = "kleavor is the wrong type for catch fairy",
            .quest = catch_fairy,
            .slugs = {"kleavor"},
            .expected = false,
        },
        {
            // Muk and Alolan Muk: only one form is Dark, so the read cannot tell.
            .name = "muk is unknown for catch dark",
            .quest = catch_dark,
            .slugs = {"muk"},
        },
        {
            .name = "muk counts for catch poison",
            .quest = catch_poison,
            .slugs = {"muk"},
            .expected = true,
        },
        {
            .name = "catch any never reads the species",
            .quest = catch_any,
            .slugs = {"pikipek"},
        },
        {
            .name = "an unknown slug is unknown",
            .quest = catch_bug,
            .slugs = {"not-a-pokemon"},
        },
    };
    for (const BBQCatchCountsScenario& scenario : scenarios){
        database.add<Test_BBQCatchCounts>(scenario);
    }
}

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

class Test_BBQCatchLooks : public UnitTest{
public:
    Test_BBQCatchLooks(const std::string& name, BBQuests quest, bool coastal_flying_catch, size_t expected)
        : UnitTest("PokemonSV::BBQCatchLooks - " + name)
        , m_quest(quest), m_coastal_flying_catch(coastal_flying_catch), m_expected(expected)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const size_t got = catch_quest_looks(m_quest, m_coastal_flying_catch);
        if (got != m_expected){
            return "Expected " + std::to_string(m_expected) + " looks, got " + std::to_string(got);
        }
        return true;
    }

private:
    const BBQuests m_quest;
    const bool m_coastal_flying_catch;
    const size_t m_expected;
};

void add_tests_BBQCatchLooks(UnitTestDatabase& database){
    //The Coastal hill retries from the closest fly point; the canyon and the rest look once.
    database.add<Test_BBQCatchLooks>("flying at the canyon", BBQuests::catch_flying, false, 1);
    database.add<Test_BBQCatchLooks>("flying at the Coastal hill", BBQuests::catch_flying, true, 3);
    database.add<Test_BBQCatchLooks>("poison at the Coastal hill", BBQuests::catch_poison, false, 3);
    database.add<Test_BBQCatchLooks>("dark at the Coastal hill", BBQuests::catch_dark, false, 3);
    database.add<Test_BBQCatchLooks>("bug is not affected by the Coastal option", BBQuests::catch_bug, true, 1);
}

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

namespace{

std::string join_bools(const std::vector<bool>& items){
    std::string ret;
    for (bool item : items){
        if (!ret.empty()){
            ret += ", ";
        }
        ret += item ? "true" : "false";
    }
    return "{" + ret + "}";
}


struct PlazaBannerToCheckCase{
    std::string name;
    Language language;
    bool ocr_available;
    std::string expected;
};

class Test_BBQPlazaBannerToCheck : public UnitTest{
public:
    Test_BBQPlazaBannerToCheck(PlazaBannerToCheckCase test_case)
        : UnitTest("PokemonSV::BBQPlazaBannerToCheck - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string banner = central_plaza_banner_to_check(m_case.language, m_case.ocr_available);
        if (banner != m_case.expected){
            return "Expected \"" + m_case.expected + "\", got \"" + banner + "\"";
        }
        return true;
    }

private:
    const PlazaBannerToCheckCase m_case;
};

void add_tests_BBQPlazaBannerToCheck(UnitTestDatabase& database){
    const PlazaBannerToCheckCase cases[] = {
        {.name = "english",                     .language = Language::English,              .ocr_available = true,  .expected = "Central Plaza"},
        {.name = "chinese traditional",         .language = Language::ChineseTraditional,   .ocr_available = true,  .expected = "中心廣場"},
        {.name = "no OCR data",                 .language = Language::ChineseTraditional,   .ocr_available = false, .expected = ""},
        {.name = "name not known yet",          .language = Language::Japanese,             .ocr_available = true,  .expected = ""},
        {.name = "no language",                 .language = Language::None,                 .ocr_available = true,  .expected = ""},
    };
    for (const PlazaBannerToCheckCase& test_case : cases){
        database.add<Test_BBQPlazaBannerToCheck>(test_case);
    }
}


struct PlazaBannerMatchCase{
    std::string name;
    std::string expected;
    std::string ocr_text;
    bool matches;
};

class Test_BBQPlazaBannerReadable : public UnitTest{
public:
    Test_BBQPlazaBannerReadable(const std::string& name, std::string expected, std::string ocr_text, bool readable)
        : UnitTest("PokemonSV::BBQPlazaBannerReadable - " + name)
        , m_expected(std::move(expected)), m_ocr_text(std::move(ocr_text)), m_readable(readable)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (place_name_banner_readable(m_expected, m_ocr_text) != m_readable){
            return std::string("Expected ") + (m_readable ? "readable" : "unreadable");
        }
        return true;
    }

private:
    const std::string m_expected;
    const std::string m_ocr_text;
    const bool m_readable;
};

class Test_BBQPlazaBannerMatch : public UnitTest{
public:
    Test_BBQPlazaBannerMatch(PlazaBannerMatchCase test_case)
        : UnitTest("PokemonSV::BBQPlazaBannerMatch - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool matches = place_name_banner_matches(m_case.expected, m_case.ocr_text);
        if (matches != m_case.matches){
            return "Banner \"" + m_case.ocr_text + "\" against \"" + m_case.expected + "\": expected "
                + (m_case.matches ? "a match" : "no match") + ", got "
                + (matches ? "a match" : "no match");
        }
        return true;
    }

private:
    const PlazaBannerMatchCase m_case;
};

void add_tests_BBQPlazaBannerMatch(UnitTestDatabase& database){
    const PlazaBannerMatchCase cases[] = {
        {.name = "chinese as seen in the lab",          .expected = "中心廣場",        .ocr_text = "中心廣場",              .matches = true},
        {.name = "chinese with spaces",                 .expected = "中心廣場",        .ocr_text = "中 心\xE3\x80\x80廣 場",  .matches = true},
        {.name = "chinese with noise around it",        .expected = "中心廣場",        .ocr_text = "|一中心廣場。",          .matches = true},
        {.name = "chinese, one character misread",      .expected = "中心廣場",        .ocr_text = "中心廣塲",              .matches = true},
        {.name = "chinese, one character missing",      .expected = "中心廣場",        .ocr_text = "中心場",                .matches = true},
        {.name = "chinese, coastal plaza",              .expected = "中心廣場",        .ocr_text = "海岸廣場",              .matches = false},
        {.name = "chinese, savanna plaza",              .expected = "中心廣場",        .ocr_text = "草原廣場",              .matches = false},
        {.name = "chinese, canyon plaza",               .expected = "中心廣場",        .ocr_text = "峽谷廣場",              .matches = false},
        {.name = "chinese, polar plaza",                .expected = "中心廣場",        .ocr_text = "極地廣場",              .matches = false},
        {.name = "chinese, only half the name",         .expected = "中心廣場",        .ocr_text = "廣場",                  .matches = false},
        {.name = "english",                             .expected = "Central Plaza",   .ocr_text = "Central Plaza",         .matches = true},
        {.name = "english, case and punctuation",       .expected = "Central Plaza",   .ocr_text = " CENTRAL  plaza.",      .matches = true},
        {.name = "english, two characters misread",     .expected = "Central Plaza",   .ocr_text = "Centra1 P1aza",         .matches = true},
        {.name = "english, coastal plaza",              .expected = "Central Plaza",   .ocr_text = "Coastal Plaza",         .matches = false},
        {.name = "english, canyon plaza",               .expected = "Central Plaza",   .ocr_text = "Canyon Plaza",          .matches = false},
        {.name = "english, savanna plaza",              .expected = "Central Plaza",   .ocr_text = "Savanna Plaza",         .matches = false},
        {.name = "english, polar plaza",                .expected = "Central Plaza",   .ocr_text = "Polar Plaza",           .matches = false},
        {.name = "english, chargestone cavern",         .expected = "Central Plaza",   .ocr_text = "Chargestone Cavern",    .matches = false},
        {.name = "nothing read",                        .expected = "中心廣場",        .ocr_text = "",                      .matches = false},
        {.name = "only punctuation read",               .expected = "Central Plaza",   .ocr_text = " .|- ",                 .matches = false},
        {.name = "nothing expected",                    .expected = "",                .ocr_text = "Central Plaza",         .matches = false},
    };
    for (const PlazaBannerMatchCase& test_case : cases){
        database.add<Test_BBQPlazaBannerMatch>(test_case);
    }
    database.add<Test_BBQPlazaBannerReadable>("stray letter", "中心廣場", "a", false);
    database.add<Test_BBQPlazaBannerReadable>("nothing read", "中心廣場", "", false);
    database.add<Test_BBQPlazaBannerReadable>("another plaza", "中心廣場", "海岸廣場", true);
    database.add<Test_BBQPlazaBannerReadable>("half the name", "Central Plaza", "Centra", true);
    database.add<Test_BBQPlazaBannerReadable>("too short for the name", "Central Plaza", "Cen", false);
}


// Plays back each leave_spot() step's result in order and records the calls,
// e.g. "map fast pc2 back flee". A step past its script returns DONE.
class ScriptedLeaveSpotActions : public LeaveSpotActions{
public:
    std::vector<LeaveStep> maps, fast, pokecenter, backs;
    std::vector<bool> flees;
    std::string calls;

    virtual LeaveStep open_map() override{ return next("map", maps, m_map); }
    virtual LeaveStep fly_to_closest_fast_travel() override{ return next("fast", fast, m_fast); }
    virtual LeaveStep fly_to_closest_pokecenter(size_t tries) override{
        return next("pc" + std::to_string(tries), pokecenter, m_pokecenter);
    }
    virtual LeaveStep back_to_overworld() override{ return next("back", backs, m_back); }
    virtual bool flee() override{
        add("flee");
        return m_flee < flees.size() ? flees[m_flee++] : true;
    }

private:
    LeaveStep next(const std::string& name, const std::vector<LeaveStep>& script, size_t& index){
        add(name);
        return index < script.size() ? script[index++] : LeaveStep::DONE;
    }
    void add(const std::string& name){
        calls += (calls.empty() ? "" : " ") + name;
    }
    size_t m_map = 0, m_fast = 0, m_pokecenter = 0, m_back = 0, m_flee = 0;
};

class Test_BBQRecoverToOverworld : public UnitTest{
public:
    Test_BBQRecoverToOverworld(
        const std::string& name, std::vector<LeaveStep> backs, std::vector<bool> flees,
        bool expected, std::string calls
    )
        : UnitTest("PokemonSV::BBQRecoverToOverworld - " + name)
        , m_backs(std::move(backs)), m_flees(std::move(flees)), m_expected(expected), m_calls(std::move(calls))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NullLogger logger;
        ScriptedLeaveSpotActions actions;
        actions.backs = m_backs;
        actions.flees = m_flees;
        const bool got = recover_to_overworld(logger, actions);
        if (got != m_expected){
            return std::string("Expected ") + (m_expected ? "recovered" : "not recovered");
        }
        if (actions.calls != m_calls){
            return "Expected steps \"" + m_calls + "\", got \"" + actions.calls + "\"";
        }
        return true;
    }

private:
    const std::vector<LeaveStep> m_backs;
    const std::vector<bool> m_flees;
    const bool m_expected;
    const std::string m_calls;
};

struct LeaveSpotCase{
    std::string name;
    std::vector<LeaveStep> maps = {}, fast = {}, pokecenter = {}, backs = {};
    std::vector<bool> flees = {};
    LeaveResult result;
    std::string calls;
};

class Test_BBQLeaveSpot : public UnitTest{
public:
    Test_BBQLeaveSpot(LeaveSpotCase test_case)
        : UnitTest("PokemonSV::BBQLeaveSpot - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NullLogger logger;
        ScriptedLeaveSpotActions actions;
        actions.maps = m_case.maps;
        actions.fast = m_case.fast;
        actions.pokecenter = m_case.pokecenter;
        actions.backs = m_case.backs;
        actions.flees = m_case.flees;
        const LeaveResult result = leave_spot(logger, actions);
        if (result != m_case.result){
            return "Expected " + leave_result_to_string(m_case.result) + ", got " + leave_result_to_string(result);
        }
        if (actions.calls != m_case.calls){
            return "Expected steps \"" + m_case.calls + "\", got \"" + actions.calls + "\"";
        }
        return true;
    }

private:
    const LeaveSpotCase m_case;
};

// Plays back the screens seen in a wild battle, and whether the fight ends it.
class ScriptedBattleEscapeActions : public BattleEscapeActions{
public:
    std::vector<BattleScreen> screens;
    bool fight_ends = true;
    std::string calls;

    virtual BattleScreen wait_for_screen() override{
        //  Past the script, the battle menu stays up: a run that never works.
        return m_screen < screens.size() ? screens[m_screen++] : BattleScreen::BATTLE_MENU;
    }
    virtual void try_run() override{ add("run"); }
    virtual bool fight() override{ add("fight"); return fight_ends; }

private:
    void add(const std::string& name){
        calls += (calls.empty() ? "" : " ") + name;
    }
    size_t m_screen = 0;
};

class Test_BBQEscapeBattle : public UnitTest{
public:
    Test_BBQEscapeBattle(
        const std::string& name, std::vector<BattleScreen> screens, bool fight_ends,
        EscapeResult expected, std::string calls
    )
        : UnitTest("PokemonSV::BBQEscapeBattle - " + name)
        , m_screens(std::move(screens)), m_fight_ends(fight_ends), m_expected(expected), m_calls(std::move(calls))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NullLogger logger;
        ScriptedBattleEscapeActions actions;
        actions.screens = m_screens;
        actions.fight_ends = m_fight_ends;
        const EscapeResult got = escape_battle(logger, actions);
        if (got != m_expected){
            return "Expected " + escape_result_to_string(m_expected) + ", got " + escape_result_to_string(got);
        }
        if (actions.calls != m_calls){
            return "Expected steps \"" + m_calls + "\", got \"" + actions.calls + "\"";
        }
        return true;
    }

private:
    const std::vector<BattleScreen> m_screens;
    const bool m_fight_ends;
    const EscapeResult m_expected;
    const std::string m_calls;
};

void add_tests_BBQLeaveSpot(UnitTestDatabase& database){
    using enum LeaveStep;
    const LeaveSpotCase cases[] = {
        {
            .name = "closest fast-travel icon",
            .result = LeaveResult::FLEW,
            .calls = "map fast",
        },
        {
            .name = "no fast-travel icon, a Pokecenter",
            .fast = {NOT_FOUND},
            .result = LeaveResult::FLEW,
            .calls = "map fast pc2",
        },
        {
            // The beach below Coastal Plaza (15:52): nothing on the map. Before,
            // the Pokecenter search retried 17 times.
            .name = "nothing to fly to leaves from here",
            .fast = {NOT_FOUND},
            .pokecenter = {NOT_FOUND},
            .result = LeaveResult::STAYED,
            .calls = "map fast pc2 back",
        },
        {
            // R17 19:47: the sneak-up battle started 0.5 s after its watch, as the
            // map opened, and stopped the farmer.
            .name = "sneak-up battle starts as the map opens",
            .maps = {BATTLE},
            .result = LeaveResult::FLEW,
            .calls = "map flee map fast",
        },
        {
            .name = "battle during the fly",
            .fast = {BATTLE},
            .result = LeaveResult::FLEW,
            .calls = "map fast flee map fast",
        },
        {
            // R17 19:47: backing out of the map ran into the same battle.
            .name = "battle while backing out of the map",
            .fast = {NOT_FOUND, DONE},
            .pokecenter = {NOT_FOUND},
            .backs = {BATTLE},
            .result = LeaveResult::FLEW,
            .calls = "map fast pc2 back flee map fast",
        },
        {
            .name = "a battle every attempt leaves from here",
            .maps = {BATTLE, BATTLE, BATTLE},
            .result = LeaveResult::STAYED,
            .calls = "map flee map flee map flee",
        },
        {
            .name = "a battle that can't be fled",
            .maps = {BATTLE},
            .flees = {false},
            .result = LeaveResult::STUCK_IN_BATTLE,
            .calls = "map flee",
        },
        {
            .name = "map fails to open leaves from here",
            .maps = {FAILED},
            .result = LeaveResult::STAYED,
            .calls = "map back",
        },
    };
    for (const LeaveSpotCase& test_case : cases){
        database.add<Test_BBQLeaveSpot>(test_case);
    }

    // recover_to_overworld() at program start, from what stopped runs left on screen.
    database.add<Test_BBQRecoverToOverworld>("already in the overworld", std::vector<LeaveStep>{}, std::vector<bool>{}, true, "back");
    database.add<Test_BBQRecoverToOverworld>(
        "map left open (R17 19:27)", std::vector<LeaveStep>{LeaveStep::DONE}, std::vector<bool>{}, true, "back"
    );
    database.add<Test_BBQRecoverToOverworld>(
        "battle left on screen (R17 19:49)", std::vector<LeaveStep>{LeaveStep::BATTLE}, std::vector<bool>{}, true, "back flee back"
    );
    database.add<Test_BBQRecoverToOverworld>(
        "a battle that can't be fled", std::vector<LeaveStep>{LeaveStep::BATTLE}, std::vector<bool>{false}, false, "back flee"
    );
    database.add<Test_BBQRecoverToOverworld>(
        "battles keep coming", std::vector<LeaveStep>{LeaveStep::BATTLE, LeaveStep::BATTLE, LeaveStep::BATTLE, LeaveStep::BATTLE},
        std::vector<bool>{}, false, "back flee back flee back flee back"
    );
    database.add<Test_BBQRecoverToOverworld>(
        "overworld never comes", std::vector<LeaveStep>{LeaveStep::FAILED}, std::vector<bool>{}, false, "back"
    );

    // escape_battle(): sneak-up, leaving a spot, and an ambush after the day skip.
    using Screens = std::vector<BattleScreen>;
    const BattleScreen MENU = BattleScreen::BATTLE_MENU, OVERWORLD = BattleScreen::OVERWORLD;
    database.add<Test_BBQEscapeBattle>(
        "fled on the first run", Screens{MENU, OVERWORLD}, true, EscapeResult::FLED, "run"
    );
    database.add<Test_BBQEscapeBattle>(
        "fled on the third run", Screens{MENU, MENU, MENU, OVERWORLD}, true, EscapeResult::FLED, "run run run"
    );
    database.add<Test_BBQEscapeBattle>(
        "already over", Screens{OVERWORLD}, true, EscapeResult::FLED, ""
    );
    database.add<Test_BBQEscapeBattle>(
        "Arena Trap Trapinch can't be fled (R17 21:08)", Screens{}, true, EscapeResult::DEFEATED, "run run run fight"
    );
    database.add<Test_BBQEscapeBattle>(
        "can't flee and the fight doesn't end", Screens{}, false, EscapeResult::FAILED, "run run run fight"
    );
    database.add<Test_BBQEscapeBattle>(
        "screen not recognized", Screens{MENU, BattleScreen::OTHER}, true, EscapeResult::FAILED, "run"
    );
}

// Plays back the banner texts read after each landing, in order. Each
// landing is a list of reads (frames x thresholds), handed to `matches` the
// way the console reads them.
class ScriptedPlazaReturnActions : public PlazaReturnActions{
public:
    ScriptedPlazaReturnActions(std::vector<std::vector<std::string>> landings)
        : m_landings(std::move(landings))
    {}

    virtual bool fly_to_plaza(bool overshoot, size_t searches) override{
        const size_t flight = overshoots.size();
        overshoots.push_back(overshoot);
        this->searches.push_back(searches);
        return flight >= fly_results.size() || fly_results[flight];
    }
    virtual std::string read_landing_banner(const std::function<bool(const std::string& text)>& matches) override{
        const size_t landing = banner_reads++;
        if (landing >= m_landings.size()){
            return "";
        }
        std::string last;
        for (const std::string& text : m_landings[landing]){
            if (matches(text)){
                return text;
            }
            if (!text.empty()){
                last = text;
            }
        }
        return last;
    }

public:
    // Whether each fly happened; flies past the end do.
    std::vector<bool> fly_results;
    std::vector<bool> overshoots;
    std::vector<size_t> searches;
    size_t banner_reads = 0;

private:
    const std::vector<std::vector<std::string>> m_landings;
};

struct PlazaReturnCase{
    std::string name;
    std::string expected_banner;
    std::vector<std::vector<std::string>> landings = {};
    std::vector<bool> fly_results = {};
    PlazaReturnResult result;
    std::vector<bool> overshoots;
    size_t banner_reads;
    std::vector<size_t> searches = {};    //  checked when not empty
};

class Test_BBQPlazaReturn : public UnitTest{
public:
    Test_BBQPlazaReturn(PlazaReturnCase test_case)
        : UnitTest("PokemonSV::BBQPlazaReturn - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NullLogger logger;
        ScriptedPlazaReturnActions actions(m_case.landings);
        actions.fly_results = m_case.fly_results;
        const PlazaReturnResult result = return_to_plaza_checked(logger, actions, m_case.expected_banner, 3);
        if (result != m_case.result){
            return "Expected result " + plaza_return_result_to_string(m_case.result)
                + ", got " + plaza_return_result_to_string(result);
        }
        if (actions.overshoots != m_case.overshoots){
            return "Expected flights with overshoot " + join_bools(m_case.overshoots)
                + ", got " + join_bools(actions.overshoots);
        }
        if (!m_case.searches.empty() && actions.searches != m_case.searches){
            return "Expected different Pokecenter search caps per flight";
        }
        if (actions.banner_reads != m_case.banner_reads){
            return "Expected " + std::to_string(m_case.banner_reads) + " banner reads, got "
                + std::to_string(actions.banner_reads);
        }
        return true;
    }

private:
    const PlazaReturnCase m_case;
};

void add_tests_BBQPlazaReturn(UnitTestDatabase& database){
    const PlazaReturnCase cases[] = {
        {
            .name = "arrives first time",
            .expected_banner = "中心廣場",
            .landings = {{"", "中心廣場"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false},
            .banner_reads = 1,
        },
        {
            // As in the lab on 2026-10-22, when the closest-Pokecenter
            // fallback flew to another fly point.
            .name = "lands elsewhere, then arrives with overshoot",
            .expected_banner = "中心廣場",
            .landings = {{"海岸廣場"}, {"中心廣場"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true},
            .banner_reads = 2,
        },
        {
            .name = "banner not seen, then arrives",
            .expected_banner = "Central Plaza",
            .landings = {{"", ""}, {"", ""}, {"Central Plaza"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true, true},
            .banner_reads = 3,
        },
        {
            .name = "never arrives",
            .expected_banner = "Central Plaza",
            .landings = {{"Coastal Plaza"}, {"Canyon Plaza"}, {""}},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 3,
        },
        {
            .name = "nothing to check",
            .expected_banner = "",
            .landings = {{"Coastal Plaza"}},
            .result = PlazaReturnResult::NOT_CHECKED,
            .overshoots = {false},
            .banner_reads = 0,
        },
        {
            // No Pokecenter to fly to, or a battle on the way: no landing to
            // read, and the next attempt overshoots.
            .name = "a fly that didn't happen uses an attempt, then arrives",
            .expected_banner = "中心廣場",
            .landings = {{"中心廣場"}},
            .fly_results = {false},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true},
            .banner_reads = 1,
        },
        {
            // bbq-m5 11:20: the fly kept failing and the return looped for
            // minutes. Now it fails after 3 flies.
            .name = "flies that never happen fail the return",
            .expected_banner = "中心廣場",
            .fly_results = {false, false, false},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 0,
        },
        {
            // Routine bench T2 rep 14: landed at Central Plaza with no
            // banner; its icon is under the player, so the next fly can't
            // happen.
            .name = "no banner, then no fly: at Central Plaza",
            .expected_banner = "中心廣場",
            .landings = {{"", ""}},
            .fly_results = {true, false},
            .result = PlazaReturnResult::UNCONFIRMED,
            .overshoots = {false, true},
            .banner_reads = 1,
        },
        {
            // Leave-area bench (2026-10-25 19:21): landed at Central Plaza
            // and read "a" off the banner, then retried for 3 minutes.
            .name = "unreadable banner, then no fly: at Central Plaza",
            .expected_banner = "中心廣場",
            .landings = {{"a"}},
            .fly_results = {true, false},
            .result = PlazaReturnResult::UNCONFIRMED,
            .overshoots = {false, true},
            .banner_reads = 1,
            .searches = {3, 1},
        },
        {
            // A wrong banner followed by no fly is not Central Plaza.
            .name = "wrong banner, then no fly: keeps trying",
            .expected_banner = "中心廣場",
            .landings = {{"海岸廣場"}, {"中心廣場"}},
            .fly_results = {true, false, true},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true, true},
            .banner_reads = 2,
        },
        {
            .name = "a failed fly and a wrong landing both count",
            .expected_banner = "Central Plaza",
            .landings = {{"Coastal Plaza"}},
            .fly_results = {false, true, false},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 1,
        },
        {
            .name = "nothing to check retries a fly that didn't happen",
            .expected_banner = "",
            .fly_results = {false},
            .result = PlazaReturnResult::NOT_CHECKED,
            .overshoots = {false, true},
            .banner_reads = 0,
        },
    };
    for (const PlazaReturnCase& test_case : cases){
        database.add<Test_BBQPlazaReturn>(test_case);
    }
}

}



// Which fly point the map flight picks, from real map screenshots. The Terarium's fly
// points are fast-travel icons: a Pokecenter search there finds nothing and, before it
// was capped, retried for minutes (2026-10-25, from the beach below Coastal Plaza).
class Test_BBQMapFlyPoint : public UnitTest{
public:
    //  expected_x/y: the center of the icon the flight should pick, normalized to the frame;
    //  negative for "no icon found".
    Test_BBQMapFlyPoint(const std::string& image, FlyPoint fly_point, double expected_x, double expected_y)
        : UnitTest("PokemonSV::BBQMapFlyPoint - " + image + " - " + (fly_point == FlyPoint::POKECENTER ? "Pokecenter" : "Fast Travel"))
        , m_image(UNIT_TEST_RESOURCE_PATH() + image)
        , m_fly_point(fly_point), m_expected_x(expected_x), m_expected_y(expected_y)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        ImageRGB32 screen(m_image);
        const std::vector<ImageFloatBox> boxes = m_fly_point == FlyPoint::POKECENTER
            ? MapPokeCenterIconDetector(COLOR_RED, MAP_READABLE_AREA).detect_all(screen)
            : FastTravelDetector(COLOR_RED, MAP_READABLE_AREA).detect_all(screen);
        const int closest = closest_flypoint_to_player(boxes, screen.width(), screen.height());
        if (m_expected_x < 0){
            if (closest >= 0){
                return "Expected no icon, found " + std::to_string(boxes.size());
            }
            return true;
        }
        if (closest < 0){
            return "Expected an icon, found none";
        }
        const ImageFloatBox& box = boxes[closest];
        const double x = box.x + box.width/2, y = box.y + box.height/2;
        logger.log("Closest of " + std::to_string(boxes.size()) + ": (" + std::to_string(x) + ", " + std::to_string(y) + ")");
        if (std::abs(x - m_expected_x) > 0.01 || std::abs(y - m_expected_y) > 0.01){
            return "Expected the icon at (" + std::to_string(m_expected_x) + ", " + std::to_string(m_expected_y)
                + "), picked (" + std::to_string(x) + ", " + std::to_string(y) + ")";
        }
        return true;
    }

private:
    const std::string m_image;
    const FlyPoint m_fly_point;
    const double m_expected_x, m_expected_y;
};

class Test_BBQSpotNeedsReload : public UnitTest{
public:
    Test_BBQSpotNeedsReload(const std::string& name, BBQuests quest, bool expected)
        : UnitTest("PokemonSV::BBQSpotNeedsReload - " + name)
        , m_quest(quest), m_expected(expected)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (quest_spot_needs_reload(m_quest) != m_expected){
            return std::string("Expected ") + (m_expected ? "reload" : "no reload");
        }
        return true;
    }

private:
    const BBQuests m_quest;
    const bool m_expected;
};

class Test_BBQBenchArmLabel : public UnitTest{
public:
    Test_BBQBenchArmLabel(BBQBenchVariant variant, char arm, std::string expected)
        : UnitTest("PokemonSV::BBQBenchArmLabel - " + expected)
        , m_variant(variant), m_arm(arm), m_expected(std::move(expected))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string got = bbq_bench_arm_label(m_variant, m_arm);
        if (got != m_expected){
            return "Expected \"" + m_expected + "\", got \"" + got + "\"";
        }
        return true;
    }

private:
    const BBQBenchVariant m_variant;
    const char m_arm;
    const std::string m_expected;
};

class Test_BBQClosestFlyPoint : public UnitTest{
public:
    Test_BBQClosestFlyPoint(const std::string& name, std::vector<ImageFloatBox> boxes, int expected)
        : UnitTest("PokemonSV::BBQClosestFlyPoint - " + name)
        , m_boxes(std::move(boxes)), m_expected(expected)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const int actual = closest_flypoint_to_player(m_boxes, 1920, 1080);
        if (actual != m_expected){
            return "Expected box " + std::to_string(m_expected) + ", picked " + std::to_string(actual);
        }
        return true;
    }

private:
    const std::vector<ImageFloatBox> m_boxes;
    const int m_expected;
};

// BBQ lab tools: the command parsing of the Lab Console and the Routine Bench summary.
// Both are lab only, but a wrong parse or summary would mislead a whole bench run.

class Test_BBQLabConsoleWords : public UnitTest{
public:
    Test_BBQLabConsoleWords(std::string name, std::string line, std::vector<std::string> expected)
        : UnitTest("PokemonSV::BBQLabConsole - words: " + name)
        , m_line(std::move(line))
        , m_expected(std::move(expected))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::string> got = bbq_console_words(m_line);
        if (got != m_expected){
            std::string text;
            for (const std::string& word : got){
                text += "[" + word + "]";
            }
            return "Unexpected words: " + text;
        }
        return true;
    }

private:
    const std::string m_line;
    const std::vector<std::string> m_expected;
};

class Test_BBQLabConsoleButtons : public UnitTest{
public:
    Test_BBQLabConsoleButtons(std::string text, Button expected, bool throws)
        : UnitTest("PokemonSV::BBQLabConsole - buttons: " + text)
        , m_text(std::move(text))
        , m_expected(expected)
        , m_throws(throws)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            const Button got = bbq_console_buttons(m_text);
            if (m_throws){
                return std::string("Expected a parse error");
            }
            if (got != m_expected){
                return "Expected buttons " + std::to_string(m_expected) + ", got " + std::to_string(got);
            }
        }catch (ParseException&){
            if (!m_throws){
                return std::string("Unexpected parse error");
            }
        }
        return true;
    }

private:
    const std::string m_text;
    const Button m_expected;
    const bool m_throws;
};

class Test_BBQBenchSummary : public UnitTest{
public:
    Test_BBQBenchSummary(std::string name, std::vector<BBQBenchRep> reps, char arm, std::string expected)
        : UnitTest("PokemonSV::BBQRoutineBench - summary: " + name)
        , m_reps(std::move(reps))
        , m_arm(arm)
        , m_expected(std::move(expected))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string got = m_arm == 0 ? bbq_bench_summary(m_reps) : bbq_bench_arm_summary(m_reps, m_arm);
        if (got != m_expected){
            return "Expected \"" + m_expected + "\", got \"" + got + "\"";
        }
        return true;
    }

private:
    const std::vector<BBQBenchRep> m_reps;
    const char m_arm;
    const std::string m_expected;
};

void add_tests_BBQLabTools(UnitTestDatabase& database){
    //  Which quests fly to the closest fly point before the day skip.
    database.add<Test_BBQSpotNeedsReload>("tera_self_defeat", BBQuests::tera_self_defeat, true);
    database.add<Test_BBQSpotNeedsReload>("sneak_up", BBQuests::sneak_up, true);
    database.add<Test_BBQSpotNeedsReload>("wild_tera", BBQuests::wild_tera, true);
    database.add<Test_BBQSpotNeedsReload>("catch_any", BBQuests::catch_any, true);
    database.add<Test_BBQSpotNeedsReload>("catch_flying", BBQuests::catch_flying, true);
    database.add<Test_BBQSpotNeedsReload>("catch_hint", BBQuests::catch_hint, true);
    //  R17 19:41: the bath flew to Savanna Plaza to leave, then sneak-up flew there again.
    database.add<Test_BBQSpotNeedsReload>("wash_pokemon", BBQuests::wash_pokemon, false);
    database.add<Test_BBQSpotNeedsReload>("photo_swim", BBQuests::photo_swim, false);
    database.add<Test_BBQSpotNeedsReload>("photo_flying", BBQuests::photo_flying, false);
    database.add<Test_BBQSpotNeedsReload>("sandwich_three", BBQuests::sandwich_three, false);
    database.add<Test_BBQSpotNeedsReload>("hatch_egg", BBQuests::hatch_egg, false);
    database.add<Test_BBQSpotNeedsReload>("tera_raid", BBQuests::tera_raid, false);
    database.add<Test_BBQSpotNeedsReload>("auto_10", BBQuests::auto_10, false);

    database.add<Test_BBQBenchArmLabel>(BBQBenchVariant::None, '-', "-");
    database.add<Test_BBQBenchArmLabel>(BBQBenchVariant::TeraSelfSpot, 'A', "A: Pride Rock");
    database.add<Test_BBQBenchArmLabel>(BBQBenchVariant::TeraSelfSpot, 'B', "B: Chargestone court");
    database.add<Test_BBQBenchArmLabel>(BBQBenchVariant::DirectLeaveArea, 'A', "A: return to Central");
    database.add<Test_BBQBenchArmLabel>(BBQBenchVariant::DirectLeaveArea, 'B', "B: fly directly, leave the spot");

    //  Zoomed-out map from the beach below Coastal Plaza: the plaza's fly point is up-left
    //  of the player, two farther ones are at the edges, and there is no Pokecenter.
    database.add<Test_BBQMapFlyPoint>("PokemonSV/BBQMap/coastal-beach.png", FlyPoint::FAST_TRAVEL, 895/1920., 452/1080.);
    database.add<Test_BBQMapFlyPoint>("PokemonSV/BBQMap/coastal-beach.png", FlyPoint::POKECENTER, -1, -1);
    //  Standing on the fly point: the player icon covers it, so only "fly here" works.
    database.add<Test_BBQMapFlyPoint>("PokemonSV/BBQMap/coastal-on-flypoint.png", FlyPoint::POKECENTER, -1, -1);
    database.add<Test_BBQClosestFlyPoint>("none", std::vector<ImageFloatBox>{}, -1);
    database.add<Test_BBQClosestFlyPoint>(
        "pixel distance, not normalized",
        //  Box 0 is 0.1 away in x (192 px), box 1 is 0.15 away in y (162 px).
        std::vector<ImageFloatBox>{{0.59, 0.49, 0.02, 0.02}, {0.49, 0.64, 0.02, 0.02}}, 1
    );

    database.add<Test_BBQLabConsoleWords>("command", "press B 160", std::vector<std::string>{"press", "B", "160"});
    database.add<Test_BBQLabConsoleWords>("extra spaces", "  stick L  0 1   500 ", std::vector<std::string>{"stick", "L", "0", "1", "500"});
    database.add<Test_BBQLabConsoleWords>("comment line", "# turn left", std::vector<std::string>{});
    database.add<Test_BBQLabConsoleWords>("blank line", "   ", std::vector<std::string>{});
    database.add<Test_BBQLabConsoleWords>("trailing comment", "wait 800 # let the glide open", std::vector<std::string>{"wait", "800"});

    database.add<Test_BBQLabConsoleButtons>("B", BUTTON_B, false);
    database.add<Test_BBQLabConsoleButtons>("L+PLUS", (Button)(BUTTON_L | BUTTON_PLUS), false);
    database.add<Test_BBQLabConsoleButtons>("LCLICK", BUTTON_LCLICK, false);
    database.add<Test_BBQLabConsoleButtons>("b", BUTTON_NONE, true);
    database.add<Test_BBQLabConsoleButtons>("A+Q", BUTTON_NONE, true);

    database.add<Test_BBQBenchSummary>("no reps", std::vector<BBQBenchRep>{}, 0, "0/0");
    database.add<Test_BBQBenchSummary>(
        "odd count, median of all reps",
        std::vector<BBQBenchRep>{{true, 10}, {false, 30}, {true, 20}}, 0, "2/3, median 20 s"
    );
    database.add<Test_BBQBenchSummary>(
        "even count, median between the middle two",
        std::vector<BBQBenchRep>{{true, 10}, {true, 20}, {false, 30}, {true, 41}}, 0, "3/4, median 25 s"
    );
    database.add<Test_BBQBenchSummary>(
        "arm A only",
        std::vector<BBQBenchRep>{{true, 10, 'A'}, {false, 99, 'B'}, {false, 30, 'A'}, {true, 99, 'B'}}, 'A', "1/2, median 20 s"
    );
    database.add<Test_BBQBenchSummary>(
        "arm with no reps",
        std::vector<BBQBenchRep>{{true, 10, 'A'}}, 'B', "0/0"
    );
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
    add_tests_BBQCatchLooks(database);
    add_tests_BBQCatchCounts(database);
    add_tests_BBQRerollThreshold(database);
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
    add_tests_BBQPlazaBannerToCheck(database);
    add_tests_BBQPlazaBannerMatch(database);
    add_tests_BBQPlazaReturn(database);
    add_tests_BBQLeaveSpot(database);
    add_tests_BBQLabTools(database);
}



}
}
}

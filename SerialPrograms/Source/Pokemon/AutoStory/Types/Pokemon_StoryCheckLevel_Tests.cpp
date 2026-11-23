/*  Story Check Level Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryCheckLevel.h"
#include "Pokemon_StoryCheckLevel_Tests.h"
#include "Pokemon_StoryLevelCheck.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const StoryCheckLevel LEVELS[] = {
    StoryCheckLevel::TESTING,
    StoryCheckLevel::THOROUGH,
    StoryCheckLevel::DEFAULT,
    StoryCheckLevel::FAST,
    StoryCheckLevel::SPEEDRUN,
};

struct CheckTableCase{
    struct Input{
        StoryCheckKind kind;
    };
    struct Expectation{
        //  Whether it runs at testing, thorough, default, fast, speedrun.
        std::vector<bool> runs;
        //  Its line in the option's help.
        std::string_view help;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const CheckTableCase CHECK_TABLE_CASES[] = {
    {
        .name = "the save card's place after a reload",
        .input = {.kind = StoryCheckKind::SAVE_PLACE},
        .expectation = {.runs = {true, true, true, true, true}, .help = "save place: every level"},
    },
    {
        .name = "the screen a checkpoint starts on",
        .input = {.kind = StoryCheckKind::START_SCREEN},
        .expectation = {.runs = {true, true, true, true, true}, .help = "start screen: every level"},
    },
    {
        .name = "a menu row before pressing it",
        .input = {.kind = StoryCheckKind::MENU_ROW},
        .expectation = {.runs = {true, true, true, false, false}, .help = "menu row: default and above"},
    },
    {
        .name = "a tap at a leg's wall",
        .input = {.kind = StoryCheckKind::LEG_END_TAP},
        .expectation = {.runs = {true, true, true, false, false}, .help = "leg end tap: default and above"},
    },
    {
        .name = "the Bag's counts",
        .input = {.kind = StoryCheckKind::BAG_CONTENTS},
        .expectation = {.runs = {true, true, false, false, false}, .help = "bag contents: thorough and above"},
    },
    {
        .name = "a member's level or stats before a fight",
        .input = {.kind = StoryCheckKind::PARTY_STATS},
        .expectation = {.runs = {true, true, false, false, false}, .help = "party stats: thorough and above"},
    },
};

class Test_StoryCheckTable : public UnitTest{
public:
    explicit Test_StoryCheckTable(const CheckTableCase& scenario)
        : UnitTest(std::format("Pokemon::StoryCheckLevel - checks: {}", scenario.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::vector<bool> runs;
        for (StoryCheckLevel level : LEVELS){
            runs.push_back(runs_check(level, m_scenario.input.kind));
        }
        if (runs != m_scenario.expectation.runs){
            std::string got;
            for (size_t c = 0; c < runs.size(); c++){
                got += std::format(" {}={}", story_check_level_name(LEVELS[c]), runs[c] ? "runs" : "skips");
            }
            return std::format("Ran at the wrong levels:{}", got);
        }
        if (story_checks_help().find(m_scenario.expectation.help) == std::string::npos){
            return std::format("Expected \"{}\" in the option's help: {}", m_scenario.expectation.help, story_checks_help());
        }
        return true;
    }

private:
    CheckTableCase m_scenario;
};

struct LevelVerdictCase{
    struct Input{
        unsigned level = 0;
        unsigned minimum = 0;
    };
    struct Expectation{
        LevelVerdict verdict = LevelVerdict::MET;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

//  Tolerance of 3 levels below minimum (17.04, console B, 2026-11-15:
//  Lv49 passes against min 50 with a warning, Lv46 stops).
const LevelVerdictCase LEVEL_VERDICT_CASES[] = {
    {.name = "at minimum", .input = {.level = 50, .minimum = 50}, .expectation = {.verdict = LevelVerdict::MET}},
    {.name = "one below minimum passes within tolerance", .input = {.level = 49, .minimum = 50}, .expectation = {.verdict = LevelVerdict::WITHIN_TOLERANCE}},
    {.name = "three below minimum passes within tolerance", .input = {.level = 47, .minimum = 50}, .expectation = {.verdict = LevelVerdict::WITHIN_TOLERANCE}},
    {.name = "four below minimum fails", .input = {.level = 46, .minimum = 50}, .expectation = {.verdict = LevelVerdict::BELOW}},
};

class Test_LevelVerdict : public UnitTest{
public:
    explicit Test_LevelVerdict(const LevelVerdictCase& scenario)
        : UnitTest(std::format("Pokemon::StoryCheckLevel - level verdict: {}", scenario.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LevelVerdict got = level_verdict(m_scenario.input.level, m_scenario.input.minimum);
        if (got != m_scenario.expectation.verdict){
            return std::format(
                "level_verdict({}, {}) returned {}, expected {}",
                m_scenario.input.level, m_scenario.input.minimum,
                static_cast<int>(got), static_cast<int>(m_scenario.expectation.verdict)
            );
        }
        return true;
    }

private:
    LevelVerdictCase m_scenario;
};

} // namespace


void add_tests_StoryCheckLevel(UnitTestDatabase& database){
    for (const CheckTableCase& scenario : CHECK_TABLE_CASES){
        database.add<Test_StoryCheckTable>(scenario);
    }
    for (const LevelVerdictCase& scenario : LEVEL_VERDICT_CASES){
        database.add<Test_LevelVerdict>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

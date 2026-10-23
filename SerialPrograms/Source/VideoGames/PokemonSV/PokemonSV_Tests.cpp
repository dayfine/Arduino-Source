/*  Pokemon SV Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
#include "Programs/PokemonSV_TerariumFlyPoints.h"
#include "PokemonSV_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{



namespace{

// Keeps every line logged, so a test can check the fly log.
class TerariumFlyLogger : public Logger{
public:
    std::vector<std::string> lines;

    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
};

std::string join_lines(const std::vector<std::string>& items, const std::string& separator){
    std::string ret;
    for (const std::string& item : items){
        if (!ret.empty()){
            ret += separator;
        }
        ret += item;
    }
    return ret;
}


struct TerariumFlyPointMoveCase{
    TerariumFlyPoint point;
    std::string name;
    MapCursorMove move;
};

// The table must keep the literals central_to_*() passed to
// map_move_cursor_fly(), since central_to_*() now reads them from it.
class Test_TerariumFlyPointMove : public UnitTest{
public:
    Test_TerariumFlyPointMove(TerariumFlyPointMoveCase test_case)
        : UnitTest("PokemonSV::TerariumFlyPointMove - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TerariumFlyPointInfo& info = terarium_fly_point_info(m_case.point);
        if (info.name != m_case.name){
            return "Expected name \"" + m_case.name + "\", got \"" + info.name + "\"";
        }
        const MapCursorMove& move = info.from_central_plaza;
        if (move.x != m_case.move.x || move.y != m_case.move.y || move.hold != m_case.move.hold){
            return "Expected move (" + std::to_string(m_case.move.x) + ", " + std::to_string(m_case.move.y)
                + ") for " + std::to_string(m_case.move.hold.count()) + " ms, got ("
                + std::to_string(move.x) + ", " + std::to_string(move.y)
                + ") for " + std::to_string(move.hold.count()) + " ms";
        }
        return true;
    }

private:
    const TerariumFlyPointMoveCase m_case;
};

void add_tests_TerariumFlyPointMove(UnitTestDatabase& database){
    const TerariumFlyPointMoveCase cases[] = {
        {.point = TerariumFlyPoint::CENTRAL_PLAZA,      .name = "Central Plaza",      .move = {}},
        {.point = TerariumFlyPoint::COASTAL_PLAZA,      .name = "Coastal Plaza",      .move = {.x = +0.409, .y = +1,      .hold = Milliseconds(1680)}},
        {.point = TerariumFlyPoint::CANYON_PLAZA,       .name = "Canyon Plaza",       .move = {.x = -1,     .y = -1,      .hold = Milliseconds(1720)}},
        {.point = TerariumFlyPoint::SAVANNA_PLAZA,      .name = "Savanna Plaza",      .move = {.x = +0.291, .y = -1,      .hold = Milliseconds(1440)}},
        {.point = TerariumFlyPoint::POLAR_PLAZA,        .name = "Polar Plaza",        .move = {.x = -0.844, .y = +0.805,  .hold = Milliseconds(1960)}},
        {.point = TerariumFlyPoint::POLAR_REST_AREA,    .name = "Polar Rest Area",    .move = {.x = -0.414, .y = +1,      .hold = Milliseconds(1840)}},
        {.point = TerariumFlyPoint::POLAR_CLASSROOM_1,  .name = "Polar Classroom 1",  .move = {.x = -1,     .y = +0.844,  .hold = Milliseconds(1200)}},
        {.point = TerariumFlyPoint::CANYON_REST_AREA,   .name = "Canyon Rest Area",   .move = {.x = -1,     .y = -0.094,  .hold = Milliseconds(1280)}},
        {.point = TerariumFlyPoint::SAVANNA_CLASSROOM,  .name = "Savanna Classroom",  .move = {.x = +1,     .y = -0.724,  .hold = Milliseconds(1120)}},
        {.point = TerariumFlyPoint::CHARGESTONE_CAVERN, .name = "Chargestone Cavern", .move = {.x = -1,     .y = -0.055,  .hold = Milliseconds(1040)}},
    };
    for (const TerariumFlyPointMoveCase& test_case : cases){
        database.add<Test_TerariumFlyPointMove>(test_case);
    }
}


struct FlyPointLabelCase{
    std::string name;
    TerariumFlyPoint expected_point;
    Language language;
    std::string label;
    FlyPointLabelMatch expected;
};

class Test_FlyPointLabelMatch : public UnitTest{
public:
    Test_FlyPointLabelMatch(FlyPointLabelCase test_case)
        : UnitTest("PokemonSV::FlyPointLabelMatch - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const FlyPointLabelMatch match = match_fly_point_label(
            m_case.expected_point, m_case.language, m_case.label
        );
        if (match != m_case.expected){
            return "Label \"" + m_case.label + "\" (normalized \"" + normalize_fly_point_label(m_case.label)
                + "\"): expected " + fly_point_label_match_to_string(m_case.expected)
                + ", got " + fly_point_label_match_to_string(match);
        }
        return true;
    }

private:
    const FlyPointLabelCase m_case;
};

void add_tests_FlyPointLabelMatch(UnitTestDatabase& database){
    const FlyPointLabelCase cases[] = {
        {
            .name = "english name",
            .expected_point = TerariumFlyPoint::CENTRAL_PLAZA,
            .language = Language::English,
            .label = "Central Plaza",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            .name = "case, spacing and punctuation don't matter",
            .expected_point = TerariumFlyPoint::POLAR_PLAZA,
            .language = Language::English,
            .label = " POLAR  plaza.",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            .name = "OCR noise around the name",
            .expected_point = TerariumFlyPoint::COASTAL_PLAZA,
            .language = Language::English,
            .label = "| Coastal Plaza -",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            // The words in a full in-game name, whatever else it holds.
            .name = "longer english name",
            .expected_point = TerariumFlyPoint::POLAR_CLASSROOM_1,
            .language = Language::English,
            .label = "Polar Outdoor Classroom 1",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            .name = "same biome, other fly point",
            .expected_point = TerariumFlyPoint::POLAR_REST_AREA,
            .language = Language::English,
            .label = "Polar Plaza",
            .expected = FlyPointLabelMatch::MISMATCH,
        },
        {
            .name = "other biome",
            .expected_point = TerariumFlyPoint::CANYON_PLAZA,
            .language = Language::English,
            .label = "Savanna Plaza",
            .expected = FlyPointLabelMatch::MISMATCH,
        },
        {
            .name = "chinese name as seen in the lab",
            .expected_point = TerariumFlyPoint::CENTRAL_PLAZA,
            .language = Language::ChineseTraditional,
            .label = "中心廣場",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            .name = "chinese with spaces between characters",
            .expected_point = TerariumFlyPoint::SAVANNA_PLAZA,
            .language = Language::ChineseTraditional,
            .label = "草 原\xE3\x80\x80廣 場",
            .expected = FlyPointLabelMatch::MATCH,
        },
        {
            .name = "chinese, other plaza",
            .expected_point = TerariumFlyPoint::COASTAL_PLAZA,
            .language = Language::ChineseTraditional,
            .label = "中心廣場",
            .expected = FlyPointLabelMatch::MISMATCH,
        },
        {
            .name = "nothing read",
            .expected_point = TerariumFlyPoint::CANYON_PLAZA,
            .language = Language::English,
            .label = "",
            .expected = FlyPointLabelMatch::NO_LABEL,
        },
        {
            .name = "only punctuation read",
            .expected_point = TerariumFlyPoint::CANYON_PLAZA,
            .language = Language::English,
            .label = " .|- ",
            .expected = FlyPointLabelMatch::NO_LABEL,
        },
        {
            .name = "chinese name not known yet",
            .expected_point = TerariumFlyPoint::CHARGESTONE_CAVERN,
            .language = Language::ChineseTraditional,
            .label = "中心廣場",
            .expected = FlyPointLabelMatch::NOT_CHECKED,
        },
        {
            .name = "language without names",
            .expected_point = TerariumFlyPoint::CENTRAL_PLAZA,
            .language = Language::Japanese,
            .label = "",
            .expected = FlyPointLabelMatch::NOT_CHECKED,
        },
    };
    for (const FlyPointLabelCase& test_case : cases){
        database.add<Test_FlyPointLabelMatch>(test_case);
    }
}


// Map steps that follow a script, one entry per attempt, and record what ran.
class ScriptedTerariumFly : public TerariumFlyActions{
public:
    std::vector<bool> central_plaza_found;
    std::vector<std::string> labels;
    std::vector<bool> fly_point_under_cursor;

    std::vector<std::string> steps;

    virtual bool open_map_at_central_plaza(size_t attempt) override{
        steps.push_back("open(" + std::to_string(attempt) + ")");
        return next(central_plaza_found, m_opens, true);
    }
    virtual void move_cursor(const MapCursorMove& move) override{
        steps.push_back("move(" + std::to_string(move.hold.count()) + "ms)");
    }
    virtual std::string read_fly_point_label() override{
        steps.push_back("read");
        return next(labels, m_reads, std::string());
    }
    virtual bool fly_here() override{
        steps.push_back("fly");
        return next(fly_point_under_cursor, m_flies, true);
    }
    virtual void close_map() override{
        steps.push_back("close");
    }

private:
    // Past the end of a script, every attempt gets the default.
    template <typename Type>
    static Type next(const std::vector<Type>& script, size_t& index, Type fallback){
        return index < script.size() ? Type(script[index++]) : fallback;
    }

    size_t m_opens = 0;
    size_t m_reads = 0;
    size_t m_flies = 0;
};

struct TerariumFlyScenario{
    std::string name;
    TerariumFlyPoint destination = TerariumFlyPoint::COASTAL_PLAZA;
    Language language = Language::English;
    std::vector<bool> central_plaza_found = {};
    std::vector<std::string> labels = {};
    std::vector<bool> fly_point_under_cursor = {};
    bool expected_flew = true;
    std::string expected_steps;
    std::vector<std::string> expected_log = {}; // Checked when not empty.
};

class Test_TerariumFly : public UnitTest{
public:
    Test_TerariumFly(TerariumFlyScenario scenario)
        : UnitTest("PokemonSV::TerariumFly - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        ScriptedTerariumFly actions;
        actions.central_plaza_found = m_scenario.central_plaza_found;
        actions.labels = m_scenario.labels;
        actions.fly_point_under_cursor = m_scenario.fly_point_under_cursor;

        TerariumFlyLogger logger;
        const bool flew = fly_to_terarium_point(logger, actions, m_scenario.language, m_scenario.destination);
        if (flew != m_scenario.expected_flew){
            return std::string("Expected ") + (m_scenario.expected_flew ? "to fly" : "to give up")
                + ", but it " + (flew ? "flew" : "gave up");
        }
        const std::string steps = join_lines(actions.steps, " ");
        if (steps != m_scenario.expected_steps){
            return "Expected steps: " + m_scenario.expected_steps + "\nActual steps:   " + steps;
        }
        if (!m_scenario.expected_log.empty() && logger.lines != m_scenario.expected_log){
            return "Expected log:\n" + join_lines(m_scenario.expected_log, "\n")
                + "\nActual log:\n" + join_lines(logger.lines, "\n");
        }
        return true;
    }

private:
    const TerariumFlyScenario m_scenario;
};

void add_tests_TerariumFly(UnitTestDatabase& database){
    const TerariumFlyScenario scenarios[] = {
        {
            .name = "right name on the first try",
            .labels = {"Coastal Plaza"},
            .expected_steps = "open(1) move(1680ms) read fly",
            .expected_log = {
                "Fly to Coastal Plaza, attempt 1/3: map label \"Coastal Plaza\": match.",
                "Fly to Coastal Plaza, attempt 1/3: flew.",
            },
        },
        {
            .name = "wrong name, then the right one",
            .labels = {"Central Plaza", "Coastal Plaza"},
            .expected_steps = "open(1) move(1680ms) read close open(2) move(1680ms) read fly",
            .expected_log = {
                "Fly to Coastal Plaza, attempt 1/3: map label \"Central Plaza\": mismatch.",
                "Fly to Coastal Plaza, attempt 2/3: map label \"Coastal Plaza\": match.",
                "Fly to Coastal Plaza, attempt 2/3: flew.",
            },
        },
        {
            // The cursor missed every fly point: give up, with the map closed.
            .name = "no label three times",
            .labels = {"", "", ""},
            .expected_flew = false,
            .expected_steps = "open(1) move(1680ms) read close open(2) move(1680ms) read close open(3) move(1680ms) read close",
            .expected_log = {
                "Fly to Coastal Plaza, attempt 1/3: map label \"\": no label.",
                "Fly to Coastal Plaza, attempt 2/3: map label \"\": no label.",
                "Fly to Coastal Plaza, attempt 3/3: map label \"\": no label.",
                "Fly to Coastal Plaza: gave up after 3 attempts.",
            },
        },
        {
            .name = "central plaza not found, then found",
            .central_plaza_found = {false, true},
            .labels = {"Coastal Plaza"},
            .expected_steps = "open(1) close open(2) move(1680ms) read fly",
            .expected_log = {
                "Fly to Coastal Plaza, attempt 1/3: Central Plaza not found on the map. Closing the map.",
                "Fly to Coastal Plaza, attempt 2/3: map label \"Coastal Plaza\": match.",
                "Fly to Coastal Plaza, attempt 2/3: flew.",
            },
        },
        {
            .name = "right name but no fly menu",
            .labels = {"Coastal Plaza", "Coastal Plaza"},
            .fly_point_under_cursor = {false, true},
            .expected_steps = "open(1) move(1680ms) read fly close open(2) move(1680ms) read fly",
        },
        {
            // Without a known name, it flies as central_to_*() does: unchecked.
            .name = "name not known in the language",
            .language = Language::Japanese,
            .expected_steps = "open(1) move(1680ms) fly",
            .expected_log = {
                "Fly to Coastal Plaza: its name isn't known in this language. Flying without checking it.",
                "Fly to Coastal Plaza, attempt 1/3: flew.",
            },
        },
        {
            // Central Plaza is where the cursor starts: no move.
            .name = "central plaza",
            .destination = TerariumFlyPoint::CENTRAL_PLAZA,
            .language = Language::ChineseTraditional,
            .labels = {"中心廣場"},
            .expected_steps = "open(1) read fly",
        },
    };
    for (const TerariumFlyScenario& scenario : scenarios){
        database.add<Test_TerariumFly>(scenario);
    }
}

}


void add_tests(UnitTestDatabase& database){
    add_tests_TerariumFlyPointMove(database);
    add_tests_FlyPointLabelMatch(database);
    add_tests_TerariumFly(database);
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
}



}
}
}

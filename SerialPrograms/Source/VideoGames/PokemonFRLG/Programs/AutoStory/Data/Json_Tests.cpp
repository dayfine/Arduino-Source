/*  FRLG Checkpoint Export Tests
 *  From: https://github.com/PokemonAutomation/
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "JsonWriter.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_StoryReplay.h"
#include "Json_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

struct ExportInput{
    std::string_view id;
    bool custom = false;
};

struct ExportExpectation{
    std::vector<std::string> events;
    std::string_view menu;
    std::string_view native_json = {};
};

struct ExportCase{
    ExportInput input;
    ExportExpectation expectation;
};

const ExportCase CASES[] = {
    {.input = {.id = "00.05"}, .expectation = {.events = {"below_mart_door", "mart_door", "parcel_from_clerk"}, .menu = "before_pokedex"}},
    {.input = {.id = "00.05", .custom = true}, .expectation = {
        .events = {"below_mart_door", "mart_door", "parcel_from_clerk"}, .menu = "full",
        .native_json = R"({"menu":"full","supplies":{"potions":2,"antidotes":3,"parlyz_heals":4,"pocket":["antidote",""],"status":"PSN","hurt":true,"hp_red":true,"hp_fill":0.25,"lead":{"species":"wartortle","moves":[{"move":"bite","pp":0}],"moves_through_level":18,"moves_read":true,"level":19,"stats":{"hp":50,"attack":20,"defense":30,"sp_attack":40,"sp_defense":35,"speed":25}},"protected_slots":[0,2],"foes":["staryu"],"trainer":"misty","risk":"high","foe_out":"staryu","wild_foes":["pidgey"]}})"
    }},
};

void customize(StoryCheckpoint& row){
    row.menu = StartMenu::FULL;
    Supplies& state = row.supplies;
    state.potions = 2;
    state.antidotes = 3;
    state.parlyz_heals = 4;
    state.pocket = {"antidote", ""};
    state.status = StatusCondition::POISON;
    state.hurt = true;
    state.hp_red = true;
    state.hp_fill = 0.25;
    state.lead = PartyMember{.species = "wartortle", .moves = {{"bite", 0}},
        .moves_through_level = 18, .moves_read = true, .level = 19,
        .stats = Stats{.hp = 50, .attack = 20, .defense = 30, .sp_attack = 40, .sp_defense = 35, .speed = 25}};
    state.protected_slots = {0, 2};
    state.foes = {"staryu"};
    state.trainer = "misty";
    state.risk = BattleRisk::HIGH;
    state.foe_out = "staryu";
    state.wild_foes = {"pidgey"};
}

std::string check_export(StoryCheckpoint row, const ExportCase& test){
    if (test.input.custom){
        customize(row);
    }
    const JsonValue encoded = story_checkpoint_export(row);
    const JsonObject& root = encoded.to_object_throw();
    const JsonObject& native = root.get_object_throw("native_fields");
    if (native.get_string_throw("menu") != test.expectation.menu){
        return "Expected menu " + std::string(test.expectation.menu) + ", read " + native.get_string_throw("menu");
    }
    if (!test.expectation.native_json.empty() && !(JsonValue(native.clone()) == parse_json(std::string(test.expectation.native_json)))){
        return "Expected native fields " + std::string(test.expectation.native_json) + ", read " + native.dump();
    }
    const auto decoded = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), "export test");
    if (!(Pokemon::story_checkpoint_json(decoded) == Pokemon::story_checkpoint_json(row))){
        return "Expected every authored common field to survive export/import.";
    }
    std::vector<std::string> names;
    for (const auto& step : decoded.steps){
        names.push_back(step.name);
    }
    return names == test.expectation.events ? "" : "Expected Parcel's three authored events in order.";
}

class ExportTest : public UnitTest{
public:
    explicit ExportTest(ExportCase test)
        : UnitTest("PokemonFRLG::CheckpointExport - " + std::string(test.input.id) + (test.input.custom ? " custom" : " authored")), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        for (const auto& row : story_checkpoints()){
            if (row.id == m_test.input.id){
                const std::string error = check_export(row, m_test);
                return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
            }
        }
        return UnitTestResult("Expected checkpoint " + std::string(m_test.input.id) + ", read no matching row.");
    }
private:
    ExportCase m_test;
};


} // namespace

void add_tests_AutoStoryJson(UnitTestDatabase& database){
    add_tests_StoryReplay(database);
    for (const ExportCase& test : CASES){
        database.add<ExportTest>(test);
    }
}

} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

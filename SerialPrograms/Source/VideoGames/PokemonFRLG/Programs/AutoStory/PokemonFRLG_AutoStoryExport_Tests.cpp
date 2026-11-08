/*  FRLG Checkpoint Export Tests
 *  From: https://github.com/PokemonAutomation/
 */
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "PokemonFRLG_AutoStoryExport.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_StoryReplay.h"
#include "PokemonFRLG_AutoStoryExport_Tests.h"
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
        .native_json = R"({"menu":"full","supplies":{"potions":2,"antidotes":3,"parlyz_heals":4,"pocket":["antidote",""],"status":"PSN","hurt":true,"hp_red":true,"hp_fill":0.25,"lead":{"species":"wartortle","moves":[{"move":"bite","pp":0}],"moves_through_level":18,"moves_read":true,"level":19,"stats":{"hp":50,"attack":20,"defense":30,"sp_attack":40,"sp_defense":35,"speed":25}},"protected_slots":[0,2],"foes":["staryu"],"trainer":"misty","risk":"high","foe_out":0,"wild_foes":["pidgey"]}})"
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
    state.foe_out = 0;
    state.wild_foes = {"pidgey"};
}
//  The export as the tool writes it to a file: with its source revision.
JsonValue exported_file(const StoryCheckpoint& row, const std::string& revision){
    JsonValue result = story_checkpoint_export(row);
    result.to_object_throw()["source_revision"] = revision;
    return result;
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
    //  Every native field too: the whole file, read back, exports the same.
    const StoryCheckpoint reread = read_story_checkpoint_export(exported_file(row, "test"), "export test");
    if (!(exported_file(reread, "test") == exported_file(row, "test"))){
        return "Expected every native field to survive export/import, read " + story_checkpoint_export(reread).dump(-1);
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


//  Reading the Parcel capture built into the program (StoryData/
//  frlg-parcel-00.05.json, PokemonSimulation's data/story/native copy of
//  the export tool's output): as written, it is row 00.05 and exports back
//  to the same file; changed, it is refused rather than read with a guess.
struct LoadInput{
    std::string_view name;
    //  Applied to the capture before reading; none reads it as written.
    void (*mutate)(JsonObject& root) = nullptr;
};
struct LoadExpectation{
    //  The table row the loaded one must export like; empty when rejected.
    std::string_view row = {};
    bool rejected = false;
};
struct LoadCase{
    LoadInput input;
    LoadExpectation expectation;
};
//  `object` without `key`: JsonObject has no erase.
JsonObject without(const JsonObject& object, std::string_view key){
    JsonObject result;
    for (const auto& [name, value] : object){
        if (name != key){
            result[name] = value.clone();
        }
    }
    return result;
}
void add_top_level_field(JsonObject& root){
    root["mystery"] = 1;
}
void change_format(JsonObject& root){
    root["format"] = "pokesim-story-plan/1";
}
void add_native_field(JsonObject& root){
    root.get_object_throw("native_fields")["speed"] = "fast";
}
void unknown_menu(JsonObject& root){
    root.get_object_throw("native_fields")["menu"] = "before_bag";
}
void drop_potions(JsonObject& root){
    JsonObject& native = root.get_object_throw("native_fields");
    native["supplies"] = without(native.get_object_throw("supplies"), "potions");
}
void negative_pp(JsonObject& root){
    JsonObject& supplies = root.get_object_throw("native_fields").get_object_throw("supplies");
    supplies.get_object_throw("lead").get_array_throw("moves")[0].to_object_throw()["pp"] = -1;
}
void foe_out_as_text(JsonObject& root){
    root.get_object_throw("native_fields").get_object_throw("supplies")["foe_out"] = "0";
}
void drop_revision(JsonObject& root){
    root = without(root, "source_revision");
}
const LoadCase LOAD_CASES[] = {
    {.input = {.name = "the Parcel capture is row 00.05"}, .expectation = {.row = "00.05"}},
    {.input = {.name = "an unknown envelope field", .mutate = add_top_level_field}, .expectation = {.rejected = true}},
    {.input = {.name = "another format", .mutate = change_format}, .expectation = {.rejected = true}},
    {.input = {.name = "an unknown native field", .mutate = add_native_field}, .expectation = {.rejected = true}},
    {.input = {.name = "an unknown START menu", .mutate = unknown_menu}, .expectation = {.rejected = true}},
    {.input = {.name = "a missing supplies field", .mutate = drop_potions}, .expectation = {.rejected = true}},
    {.input = {.name = "PP below zero", .mutate = negative_pp}, .expectation = {.rejected = true}},
    {.input = {.name = "foe_out as text", .mutate = foe_out_as_text}, .expectation = {.rejected = true}},
    {.input = {.name = "no source revision", .mutate = drop_revision}, .expectation = {.rejected = true}},
};
const std::string& parcel_text(){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == "frlg-parcel-00.05"){
            return text;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "StoryData/frlg-parcel-00.05.json is not built into the program.");
}
const StoryCheckpoint* table_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return &row;
        }
    }
    return nullptr;
}
std::string check_loaded(const StoryCheckpoint& loaded, const JsonValue& capture, const LoadCase& test){
    const std::string& revision = capture.to_object_throw().get_string_throw("source_revision");
    if (!(exported_file(loaded, revision) == capture)){
        return "Expected the loaded row to export as the capture, read " + story_checkpoint_export(loaded).dump(-1);
    }
    const StoryCheckpoint* row = table_row(test.expectation.row);
    if (!row){
        return "Expected table row " + std::string(test.expectation.row) + ", read no matching row.";
    }
    if (!(story_checkpoint_export(loaded) == story_checkpoint_export(*row))){
        return "Expected the loaded row to export as the table's " + row->id + ", read " + story_checkpoint_export(loaded).dump(-1);
    }
    return "";
}
std::string check_load(const LoadCase& test){
    JsonValue capture = parse_json(parcel_text());
    if (test.input.mutate){
        test.input.mutate(capture.to_object_throw());
    }
    try{
        const StoryCheckpoint loaded = read_story_checkpoint_export(capture, test.input.name);
        if (test.expectation.rejected){
            return "Expected rejection, read checkpoint " + loaded.id;
        }
        return check_loaded(loaded, capture, test);
    }catch (const ParseException& error){
        return test.expectation.rejected ? "" : "Unexpected parse failure: " + error.message();
    }
}
class LoadTest : public UnitTest{
public:
    explicit LoadTest(LoadCase test)
        : UnitTest("PokemonFRLG::CheckpointExport - read " + std::string(test.input.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_load(m_test);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    LoadCase m_test;
};
} // namespace
void add_tests_AutoStoryExport(UnitTestDatabase& database){
    add_tests_StoryReplay(database);
    for (const ExportCase& test : CASES){
        database.add<ExportTest>(test);
    }
    for (const LoadCase& test : LOAD_CASES){
        database.add<LoadTest>(test);
    }
}
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

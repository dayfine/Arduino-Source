/*  BDSP authored checkpoint tests
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP_AutoStory_StoryData_Tests.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace{
struct Input{
    std::string_view name;
    void (*mutate)(JsonObject&) = nullptr;
};
struct Expectation{ bool rejected = false; };
struct Case{ Input input; Expectation expectation; };
void extra(JsonObject& root){ root["unknown"] = true; }
void wrong_game(JsonObject& root){ root["game"] = "frlg"; }
void wrong_policy(JsonObject& root){ root.get_object_throw("execution")["on_failure"] = "guess"; }
void missing_card(JsonObject& root){ root.get_object_throw("execution")["saves_in"] = ""; }
void stop_with_save(JsonObject& root){ root.get_object_throw("execution")["on_failure"] = "stop"; }
const Case CASES[] = {
    {.input = {.name = "authored mine row replaces the table entry"}, .expectation = {}},
    {.input = {.name = "extra envelope field", .mutate = extra}, .expectation = {.rejected = true}},
    {.input = {.name = "wrong game", .mutate = wrong_game}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown failure policy", .mutate = wrong_policy}, .expectation = {.rejected = true}},
    {.input = {.name = "missing save-card location", .mutate = missing_card}, .expectation = {.rejected = true}},
    {.input = {.name = "stop policy must not claim a start save", .mutate = stop_with_save}, .expectation = {.rejected = true}},
};
JsonValue plan(){
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (name == "bdsp-mine-01.03.plan"){
            return parse_json(text);
        }
    }
    throw ParseException("Expected embedded bdsp-mine-01.03.plan.json.");
}
std::string check_table(const StoryCheckpoint& loaded, const JsonValue& input){
    const auto& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& x){ return x.id == "01.03"; });
    if (row == rows.end() || Pokemon::story_checkpoint_json(*row) != input.to_object_throw().get_value_throw("checkpoint")){
        return "Expected the actual 01.03 table entry to contain every authored field.";
    }
    if (loaded.on_failure != StoryFailure::RELOAD_AND_RETRY || loaded.saves_in != "Oreburgh City"
        || row->on_failure != loaded.on_failure || row->saves_in != loaded.saves_in){
        return "Expected reload-and-retry with Oreburgh City save-card guard.";
    }
    const std::vector<std::string> expected{
        "above_the_mine", "into_the_mine", "into_the_coal_room", "south_to_the_rock",
        "to_roarks_corner", "beside_roarks_rock", "below_roark", "roark", "roark_smashes_the_rock",
        "below_the_coal_rooms_warp", "up_to_the_hall", "out_of_the_mine",
        "north_to_the_centers_wall", "east_below_the_centers_door", "heal_oreburgh"
    };
    std::vector<std::string> actual;
    for (const auto& step : loaded.steps){
        actual.push_back(step.name);
    }
    return actual == expected ? "" : "Expected all 15 current #226 mine events in order.";
}
class DataTest : public UnitTest{
public:
    explicit DataTest(Case test) : UnitTest("PokemonBDSP::StoryData - " + std::string(test.input.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        JsonValue input = plan();
        if (m_test.input.mutate){
            m_test.input.mutate(input.to_object_throw());
        }
        try{
            const StoryCheckpoint loaded = read_authored_checkpoint(input, m_test.input.name);
            if (m_test.expectation.rejected){
                return "Expected authored data rejection.";
            }
            const std::string error = check_table(loaded, input);
            return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
        }catch (const Exception& error){
            return m_test.expectation.rejected ? UnitTestResult(true) : UnitTestResult(error.to_str());
        }
    }
private:
    Case m_test;
};
} // namespace
void add_tests_AutoStory_StoryData(UnitTestDatabase& database){
    for (const Case& test : CASES){
        database.add<DataTest>(test);
    }
}
} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

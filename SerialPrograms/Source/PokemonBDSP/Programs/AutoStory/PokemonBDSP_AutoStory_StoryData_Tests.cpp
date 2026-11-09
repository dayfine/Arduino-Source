/*  BDSP Authored Checkpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
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

struct Expectation{
    bool rejected = false;
};
struct Case{
    Input input;
    Expectation expectation;
};
void extra(JsonObject& root){
    root["unknown"] = true;
}

void wrong_game(JsonObject& root){
    root["game"] = "frlg";
}

void wrong_policy(JsonObject& root){
    root.get_object_throw("execution")["on_failure"] = "guess";
}

void missing_card(JsonObject& root){
    root.get_object_throw("execution")["saves_in"] = "";
}

void stop_with_save(JsonObject& root){
    root.get_object_throw("execution")["on_failure"] = "stop";
}

const Case CASES[] = {
    {.input = {.name = "mine resource is registered with ordered events and save policy"}, .expectation = {}},
    {.input = {.name = "extra envelope field", .mutate = extra}, .expectation = {.rejected = true}},
    {.input = {.name = "wrong game", .mutate = wrong_game}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown failure policy", .mutate = wrong_policy}, .expectation = {.rejected = true}},
    {.input = {.name = "missing save-card location", .mutate = missing_card}, .expectation = {.rejected = true}},
    {.input = {.name = "stop policy must not claim a start save", .mutate = stop_with_save}, .expectation = {.rejected = true}},
};

std::string string_list(const std::vector<std::string>& values){
    JsonArray result;
    for (const std::string& value : values){
        result.push_back(value);
    }
    return result.dump(-1);
}

std::string tile_list(const std::vector<Pokemon::TilePos>& tiles){
    std::vector<std::string> values;
    for (const auto& tile : tiles){
        values.push_back(Pokemon::to_string(tile));
    }
    return string_list(values);
}

std::string policy(const StoryCheckpoint& row){
    return std::format("{}; saves_in=\"{}\"",
        row.on_failure == StoryFailure::RELOAD_AND_RETRY ? "reload_and_retry" : "stop", row.saves_in);
}

JsonValue plan(){
    std::vector<std::string> names;
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        names.push_back(name);
        if (name == "bdsp-mine-01.03.plan"){
            return parse_json(text);
        }
    }
    throw ParseException("Expected embedded bdsp-mine-01.03.plan.json; read resource names " + string_list(names));
}

//  Literal authored poses must stay aligned with the map-derived anchors
//  used by adjacent checkpoints; a map correction must trigger plan review.
std::string check_map_anchors(const StoryCheckpoint& loaded){
    const std::vector<Pokemon::TilePos> doorstep{
        {47, 21}, map_point("OreburghCity", "pokemon_center_doorstep")
    };
    if (!loaded.start || Pokemon::tiles_of(*loaded.start) != doorstep){
        return "Expected Center exit tiles " + tile_list(doorstep) + "; read "
            + (loaded.start ? Pokemon::story_pose_json(*loaded.start).dump(-1) : "absent start");
    }
    const std::vector<Pokemon::TilePos> below_roark{
        map_point("OreburghMine", "roark").next(Pokemon::Direction::SOUTH)
    };
    for (const auto& step : loaded.steps){
        const bool roark = step.name == "below_roark" || step.name == "roark_smashes_the_rock";
        if (roark || step.name == "heal_oreburgh"){
            const auto& expected = roark ? below_roark : doorstep;
            if (!step.pose_after || Pokemon::tiles_of(*step.pose_after) != expected){
                return "Expected " + step.name + " arrival tiles " + tile_list(expected) + "; read "
                    + (step.pose_after ? Pokemon::story_pose_json(*step.pose_after).dump(-1) : "absent arrival");
            }
        }
    }
    return "";
}

std::string check_registration(const StoryCheckpoint& loaded){
    const auto& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& x){ return x.id == "01.03"; });
    //  Wiring check only: both values originate in the authored resource.
    //  Route correctness is covered by separate scenario expectations.
    if (row == rows.end()){
        std::vector<std::string> ids;
        for (const auto& entry : rows){
            ids.push_back(entry.id);
        }
        return "Expected registered checkpoint 01.03; read checkpoint IDs " + string_list(ids);
    }
    if (Pokemon::story_checkpoint_json(*row) != Pokemon::story_checkpoint_json(loaded)){
        return "Expected registered resource " + Pokemon::story_checkpoint_json(loaded).dump(-1)
            + "; read table record " + Pokemon::story_checkpoint_json(*row).dump(-1);
    }
    if (loaded.on_failure != StoryFailure::RELOAD_AND_RETRY || loaded.saves_in != "Oreburgh City"
        || row->on_failure != loaded.on_failure || row->saves_in != loaded.saves_in){
        return "Expected reload_and_retry; saves_in=\"Oreburgh City\" for resource and table; read resource "
            + policy(loaded) + "; table " + policy(*row);
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
    if (actual != expected){
        return "Expected event IDs " + string_list(expected) + "; read " + string_list(actual);
    }
    return check_map_anchors(loaded);
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
                return "Expected authored data rejection; read accepted record " + Pokemon::story_checkpoint_json(loaded).dump(-1);
            }
            const std::string error = check_registration(loaded);
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

/*  Offline native replay. No console, profile, app instance or controller IO.
 *  cmake -S SerialPrograms -B build -DPA_FRLG_STORY_REPLAY=ON
 *  cmake --build build --target StoryCheckpointReplay -j4
 *  build/StoryCheckpointReplay > parcel-native-replay.json
 *  Compare against the pinned PokemonSimulation scenarios in CI.
 */
#include <iostream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_StoryReplay.h"
int main(int argc, char**){
    using namespace PokemonAutomation;
    using namespace NintendoSwitch::PokemonFRLG;
    if (argc != 1){
        std::cerr << "Usage: StoryCheckpointReplay > native-replay.json\n";
        return 2;
    }
    try{
        const JsonValue cases = parse_json(story_replay_fixture("cases"));
        JsonArray reports;
        for (const JsonValue& test : cases.to_object_throw().get_array_throw("cases")){
            JsonValue report = replay_story_checkpoint(story_replay_fixture("capture"), story_replay_fixture("binding"),
                story_replay_fixture("route-candidate"), test.to_object_throw().get_value_throw("input"));
            JsonObject item;
            item["id"] = test.to_object_throw().get_string_throw("id");
            item["report"] = std::move(report);
            reports.push_back(std::move(item));
        }
        JsonObject result;
        result["format"] = "pokesim-native-replay-suite/1";
        result["native_execution_revision"] = PA_STORY_REPLAY_REVISION;
        result["fixture_pin"] = parse_json(story_replay_fixture("pin"));
        result["reports"] = std::move(reports);
        std::cout << result.dump(-1) << '\n';
        return 0;
    }catch (const Exception& error){
        std::cerr << error.to_str() << '\n';
        return 1;
    }
}

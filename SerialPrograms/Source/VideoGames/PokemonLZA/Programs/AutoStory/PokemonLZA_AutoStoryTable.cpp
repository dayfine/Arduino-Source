/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "Step N" in the comments is the step of the recording (console B,
 *  2026-11-08/09) a walk comes from; the checkpoints are in StoryData/.
 *
 */

#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Data/Pokemon_AuthoredCheckpointJson.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

//  A walk of `legs`, each {turn in degrees clockwise (negative: left), walk
//  time}.
StoryMove walk(std::vector<StoryWalkLeg> legs){
    StoryMove move;
    move.legs = std::move(legs);
    return move;
}

//  The walks below were measured 2026-11-08/09 on console B, walking (not
//  dashing); none has run as a story step yet. A walk the recording ends at
//  a prompt or a fade overshoots into it, and the step after it presses on.
using MoveList = std::vector<std::pair<std::string, StoryMove>>;

MoveList mission_01_02_moves(){
    return {
        //  Steps 6-7. Measured 2026-11-08 on console B, walk not run.
        {"platform_to_plaza", walk({{0, 5500ms}, {90, 6000ms}})},
        //  Step 11. NEEDS TUNING. Measured 2026-11-08 on console B, walk not
        //  run. The sidestep around the starters had no time recorded:
        //  1000 ms is a placeholder.
        {"chase_pancham", walk({
            {-15, 4000ms}, {-90, 3500ms}, {90, 9000ms}, {-90, 0ms}, {90, 1000ms}, {-90, 4000ms},
        })},
        //  Step 18. NEEDS TUNING. Measured 2026-11-08 on console B, walk not run.
        {"battle_zone_to_brigitte", walk({
            {0, 3500ms}, {90, 0ms}, {-90, 500ms}, {90, 4000ms}, {5, 2500ms}, {90, 4000ms},
        })},
        //  Step 21. Measured 2026-11-08 on console B, walk not run.
        {"to_blocked_street", walk({{0, 2000ms}, {-75, 3500ms}, {90, 3000ms}})},
        //  Step 22. Measured 2026-11-08 on console B, walk not run.
        {"to_battle_zone_gate", walk({{0, 5000ms}})},
        //  Step 23. Measured 2026-11-08 on console B, walk not run.
        {"through_covered_passage", walk({{75, 3000ms}, {0, 5000ms}})},
        //  Step 24: 3-4.5 s, the longest. Measured 2026-11-08 on console B, walk not run.
        {"to_hotel_z_door", walk({{0, 4500ms}})},
        //  Step 26: 2.25 s was one step short. Measured 2026-11-08 on console B, walk not run.
        {"to_front_counter", walk({{-5, 2500ms}, {-85, 0ms}})},
        //  Step 28. Measured 2026-11-08 on console B, walk not run.
        {"to_lobby_elevator", walk({{0, 2000ms}})},
        //  Step 29. Measured 2026-11-08 on console B, walk not run.
        {"to_room_202", walk({{-15, 2000ms}})},
        //  Step 31: 1.5 s explored, 1 s enough. Measured 2026-11-08 on console B, walk not run.
        {"to_bed", walk({{45, 1000ms}})},
        //  Step 33. Measured 2026-11-08 on console B, walk not run.
        {"to_room_door", walk({{0, 2000ms}})},
    };
}

MoveList mission_03_moves(){
    return {
        //  Step 36. Measured 2026-11-08 on console B, walk not run.
        {"out_of_room_202", walk({{0, 1000ms}, {-45, 1000ms}})},
        //  Step 37. Measured 2026-11-08 on console B, walk not run.
        {"to_2f_elevator", walk({{-15, 2000ms}})},
        //  Step 40: stops at the door. Measured 2026-11-08 on console B, walk not run.
        {"to_hotel_front_door", walk({{0, 6000ms}})},
        //  Step 41. Measured 2026-11-08 on console B, walk not run.
        {"to_taunie_at_gate", walk({{0, 3000ms}})},
        //  Step 46. Measured 2026-11-08 on console B, walk not run.
        {"through_hotel_gate", walk({{0, 7000ms}})},
        //  Step 47: 6 s down the side street, 2 s across. Measured 2026-11-08 on console B, walk not run.
        {"side_street_to_wall", walk({{0, 8000ms}})},
        //  Step 51: walks off the edge and drops. Measured 2026-11-08 on console B, walk not run.
        {"off_the_roof", walk({{0, 4000ms}})},
        //  Step 53: the last turn ("right") had no angle recorded: 90 is a
        //  guess. Measured 2026-11-08 on console B, walk not run.
        {"into_pokemon_center", walk({{0, 3000ms}, {20, 2000ms}, {-90, 2000ms}, {90, 0ms}})},
    };
}

MoveList mission_03_zone_moves(){
    return {
        //  Step 56: ~5 units/s; the first 2 s replace a dash test of the
        //  same distance. Measured 2026-11-08 on console B, walk not run.
        {"center_to_zone_1", walk({{0, 2000ms}, {0, 3000ms}, {0, 6000ms}})},
        //  Step 57: left 45, right 15. Measured 2026-11-08 on console B, walk not run.
        {"toward_zone_1_gate", walk({{-45, 0ms}, {15, 2500ms}})},
        //  Step 58: 22 units at ~5.1 units/s. Measured 2026-11-08 on console B, walk not run.
        {"into_zone_1", walk({{0, 4300ms}})},
        //  Step 62. Measured 2026-11-08 on console B, walk not run.
        {"zone_1_to_far_gate", walk({{90, 1000ms}, {-90, 15000ms}, {0, 2000ms}})},
        //  Step 63. Measured 2026-11-08 on console B, walk not run.
        {"to_research_lab", walk({{45, 4000ms}, {0, 2000ms}, {-90, 2000ms}})},
        //  Step 65: 1 s was short. Measured 2026-11-09 on console B, walk not run.
        {"up_the_lab_steps", walk({{0, 1500ms}})},
        //  Step 66. Measured 2026-11-09 on console B, walk not run.
        {"to_lab_elevator", walk({{0, 1000ms}})},
        //  Step 68. NEEDS TUNING: "left 0.5 s, right 2 s", the angles not
        //  recorded (90 is a guess). Measured 2026-11-09 on console B, walk
        //  not run.
        {"to_mable", walk({{-90, 500ms}, {90, 2000ms}})},
    };
}

//  The checkpoints of the JSON files, in file order, and the ids of those
//  whose execution says the game autosaves at their end.
struct StoryData{
    std::vector<StoryCheckpoint> checkpoints;
    std::set<std::string, std::less<>> autosaves;
};

StoryData load_story_data(){
    StoryData data;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        bool autosave = false;
        const Pokemon::ExecutionReader read_execution = [&autosave](const JsonObject& execution, std::string_view source){
            Pokemon::validate_json_fields(execution, {"save"}, source);
            const std::string& save = execution.get_string_throw("save");
            if (save != "autosave" && save != "menu"){
                throw ParseException(std::string(source) + ": save must be \"autosave\" or \"menu\", not " + save);
            }
            autosave = save == "autosave";
        };
        StoryCheckpoint checkpoint = Pokemon::read_authored_checkpoint(
            parse_json(text), "plza", name, read_execution, plza_story_specs()
        );
        if (autosave){
            data.autosaves.insert(checkpoint.id);
        }
        data.checkpoints.push_back(std::move(checkpoint));
    }
    return data;
}

const StoryData& story_data(){
    static const StoryData data = load_story_data();
    return data;
}

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    return story_data().checkpoints;
}


const std::map<std::string, StoryMove, std::less<>>& story_moves(){
    static const std::map<std::string, StoryMove, std::less<>> moves = []{
        std::map<std::string, StoryMove, std::less<>> all;
        for (MoveList part : {mission_01_02_moves(), mission_03_moves(), mission_03_zone_moves()}){
            for (auto& [name, move] : part){
                all.emplace(std::move(name), std::move(move));
            }
        }
        return all;
    }();
    return moves;
}

const std::map<std::string, Pokemon::ScreenEvent, std::less<>>& story_move_ends(){
    static const std::map<std::string, Pokemon::ScreenEvent, std::less<>> ends{
        //  Steps 6-7: right along the plaza "until the white fade" into the
        //  arrival scenes. 6000 ms is the recorded walk, which overshot into it.
        {"platform_to_plaza", Pokemon::ScreenEvent::FADE_WHITE},
    };
    return ends;
}

const std::set<std::string, std::less<>>& story_autosave_rows(){
    return story_data().autosaves;
}

bool story_row_saves(const Pokemon::StoryCheckpoint& row){
    return !story_autosave_rows().contains(row.id);
}

size_t story_rows_ready(){
    size_t ready = 0;
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (step.kind == StoryStepKind::GAME && story_actions_not_implemented().contains(step.action)){
                return ready;
            }
        }
        ready++;
    }
    return ready;
}

const std::map<StarterChoice, StoryMove>& starter_moves(){
    static const std::map<StarterChoice, StoryMove> moves{
        //  Step 13: ends at the "A Talk Chikorita" prompt. Measured
        //  2026-11-08 on console B and checked by hand from the autosave,
        //  walk not run as a story step.
        {StarterChoice::CHIKORITA, walk({{-70, 750ms}})},
    };
    return moves;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

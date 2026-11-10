/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "Step N" in the comments is the step of the recording (console B,
 *  2026-11-08/09) a row or a walk comes from.
 *
 */

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::GridPose;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


namespace{

constexpr std::string_view LUMIOSE = "LumioseCity";
constexpr std::string_view HOTEL_Z = "HotelZ";
constexpr std::string_view WILD_ZONE_1 = "WildZone1";
constexpr std::string_view RESEARCH_LAB = "PokemonResearchLab";

//  Somewhere in `map`: the game reads no position yet.
GridPose in(std::string_view map){
    GridPose pose;
    pose.map = std::string(map);
    return pose;
}

StoryStep make(StoryStepKind kind, std::string name, std::string why, std::string game_action = ""){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = std::move(game_action);
    return step;
}

//  A GAME step that walks the move of story_moves() named `name`.
StoryStep move(std::string name, std::string why){
    std::string game_action = name;
    return make(StoryStepKind::GAME, std::move(name), std::move(why), std::move(game_action));
}

//  A GAME step doing the executor's action `game_action`: a choice, a story
//  battle, or one of story_actions_not_implemented().
StoryStep action(std::string game_action, std::string name, std::string why){
    return make(StoryStepKind::GAME, std::move(name), std::move(why), std::move(game_action));
}

//  A scene: A through its text (choices: the top one) until the overworld.
StoryStep mash_a(std::string name, std::string why){
    return make(StoryStepKind::MASH, std::move(name), std::move(why), "a");
}

//  A scene: B through its text until the overworld. B never starts a talk.
StoryStep mash_b(std::string name, std::string why){
    return make(StoryStepKind::MASH, std::move(name), std::move(why));
}

//  A at the "A Talk" (or "A Take Elevator", "A Rest") prompt, until a dialogue shows.
StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}

StoryCheckpoint row(std::string id, std::string title, std::optional<GridPose> start, std::vector<StoryStep> steps){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = std::move(title);
    checkpoint.start = std::move(start);
    checkpoint.steps = std::move(steps);
    return checkpoint;
}


//  Main Mission 01: a new game, Pancham, the starter.
std::vector<StoryCheckpoint> mission_01(){
    return {
        //  No save before the starter's autosave: a failed attempt reloads
        //  to the title screen with no save, where this row starts again.
        row("01.00", "New game to the starter", std::nullopt, {
            action("new_game_language", "language", "step 1: start a new game in English"),
            action("pick_look", "look", "step 2: pick the first look"),
            action("type_player_name", "name", "steps 3-4: name the player and confirm"),
            action("mash_to_move_hints", "opening", "step 5: the opening cutscene until the Move / Camera hints"),
            move("platform_to_plaza", "steps 6-7: down the platform, then right until the white fade"),
            action("mash_to_move_hints", "arrival_scenes", "steps 8-10: \"That I am\", Jett's ad, \"So you hired a Pokemon...\" (top choices)"),
            move("chase_pancham", "step 11: after Pancham, through the archway, around the starters into the scene"),
            action("text_to_choice", "pancham_cornered", "step 12: the scene up to its choice"),
            action("choose_1", "battle_with_pokemon", "step 12: choose \"Battle with Pokemon!\" (second choice)"),
            action("mash_to_move_hints", "battle_area", "step 12: the scene until free control in the battle area; autosave"),
        }),
        //  Starts on the autosave before the starter.
        row("01.01", "Starter and the first battle", in(LUMIOSE), {
            action("approach_starter", "approach_starter", "step 13: walk up to the starter (only Chikorita's walk was recorded)"),
            talk("talk_to_starter", "step 13: A at \"A Talk Chikorita\""),
            action("story_battle_start", "andi_battle_start", "steps 14-15: choose it, the pre-battle talk, \"Go! Chikorita!\", the battling tutorial"),
            action("story_battle", "andi_battle", "step 16: beat Andi's Pancham (Lv 6), until Andi's speech"),
            mash_a("after_andi", "step 17: fist bump, night in the Battle Zone, the Main Mission 02 banner and its autosave"),
        }),
    };
}

//  Main Mission 02: out of the Battle Zone, Hotel Z, the first night.
std::vector<StoryCheckpoint> mission_02(){
    return {
        //  Starts on the autosave at the Main Mission 02 banner. No
        //  autosave was seen until Main Mission 03's, and the menu cannot
        //  save yet, so the whole mission is one row.
        row("02.01", "Main Mission 02: Battle Zone to the first night", in(LUMIOSE), {
            move("battle_zone_to_brigitte", "step 18: through the Battle Zone until the black fade into Brigitte's scene"),
            action("story_battle_start", "brigitte_battle_start", "step 19: A through her talk to \"Brigitte sent out Cleffa!\""),
            action("story_battle", "brigitte_battle", "step 19: beat Brigitte's Cleffa (Lv 7), until her speech"),
            mash_a("after_brigitte", "step 20: Brigitte's and Taunie's talk, until free control alone in the plaza"),
            move("to_blocked_street", "step 21: on until the black fade into the street the trainers block"),
            mash_a("street_scene", "step 21: a Pokemon drives the trainers off (no battle)"),
            move("to_battle_zone_gate", "step 22: on to the gate's \"A Leave\""),
            mash_a("leave_battle_zone", "steps 22-23: A Leave, Taunie: Hotel Z's just through here"),
            move("through_covered_passage", "step 23: through the wooden door and the covered passage into the Hotel Z scene"),
            mash_a("hotel_z_arrival", "step 23: the Hotel Z arrival scene"),
            move("to_hotel_z_door", "step 24: up the steps to the door's \"A Enter\""),
            mash_a("hotel_z_lobby", "steps 24-25: A Enter, the loading screen, the lobby scene with Taunie"),
            move("to_front_counter", "step 26: along the counter to the clerk's \"A Talk\""),
            talk("talk_to_clerk", "step 26: A at the clerk (\"A Talk ???\")"),
            mash_a("check_in", "step 27: check-in with AZ, the Key to Room 202"),
            move("to_lobby_elevator", "step 28: to the elevator's \"A Take Elevator\""),
            talk("take_elevator_up", "step 28: A Take Elevator, \"Which floor?\""),
            action("choose_0", "ride_to_2f", "step 28: \"2F\", the first floor listed"),
            mash_b("arrive_2f", "step 28: the ride, until control on 2F"),
            move("to_room_202", "step 29: to \"Room 202 / A Enter\""),
            mash_a("enter_room_202", "step 30: A Enter, \"Here's room 202.\", until control in the room"),
            move("to_bed", "step 31: to the bed's \"A Rest\""),
            talk("rest", "step 31: A Rest, \"Rest until tomorrow morning?\""),
            action("choose_0", "rest_until_morning", "step 31: yes"),
            mash_a("wake_up", "step 32: sleep and wake up, until control facing the door"),
            move("to_room_door", "step 33: to the door's \"A Leave\""),
            //  The scenes and a ~25 s black screen may outlast MASH's 60 s.
            mash_a("first_evening", "steps 33-35: Taunie, the rooftop, AZ, the Main Mission 03 banner and its autosave"),
        }),
    };
}

//  Main Mission 03: a new life in Lumiose, to Mable's Rock Smash TM.
std::vector<StoryCheckpoint> mission_03_hotel_z(){
    return {
        //  Starts on the autosave after the Main Mission 03 banner.
        row("03.01", "Down to the lobby", in(HOTEL_Z), {
            move("out_of_room_202", "step 36: to the room door's \"A Leave\""),
            mash_a("leave_room_202", "step 36: A Leave, the loading screen"),
            move("to_2f_elevator", "step 37: down the 2F hallway to \"A Take Elevator\""),
            talk("take_elevator_down", "step 37: A Take Elevator, \"Which floor?\""),
            action("choose_0", "ride_to_lobby", "step 38: \"1F (Lobby)\", the first floor listed"),
            mash_a("lobby_scene", "step 39: Z-A Royale, the app, the new outfit, AZ, Lida; autosave"),
        }),
        //  Starts on the lobby's autosave. From here on the main menu saves
        //  at each row's end.
        row("03.02", "Out of Hotel Z, battle Taunie", in(HOTEL_Z), {
            move("to_hotel_front_door", "step 40: across the lobby to the front door's \"A Leave\""),
            mash_a("leave_hotel_z", "steps 40-41: A Leave, the Main Menu tutorial card"),
            move("to_taunie_at_gate", "step 41: to Taunie at the gate, \"A Talk Taunie\""),
            talk("talk_to_taunie", "step 42: A at Taunie"),
            action("story_battle_start", "taunie_battle_start", "step 42: A through her talk to \"Taunie sent out Totodile!\""),
            action("story_battle", "battle_totodile", "step 43: beat Totodile, until \"We're not done just yet!\""),
            action("story_battle_start", "taunie_sends_tepig", "step 44: the Type Matchups tutorial (3 pages), Tepig sent out"),
            action("story_battle", "battle_tepig", "step 44: beat Tepig, until Taunie's speech"),
            action("talk_through_map_screen", "after_taunie_battle", "step 45: Taunie's talk, closing the map it opens with B"),
            mash_a("taunie_runs_off", "step 45: the Main Missions tutorial, Taunie runs off"),
        }),
    };
}

std::vector<StoryCheckpoint> mission_03_to_pokemon_center(){
    return {
        row("03.03", "Over the rooftops to Station Front", in(LUMIOSE), {
            move("through_hotel_gate", "step 46: through the Hotel Z gate passage into the scene"),
            mash_a("rock_pile_scene", "step 46: Taunie at the rocks blocking the street"),
            move("side_street_to_wall", "step 47: down the side street to the wall across it"),
            action("climb_ladder", "climb_ladder", "steps 48-49: sidestep right 0.6 s, face up (150 ms), A Climb Up, hold up ~4-6 s"),
            mash_a("rooftop_scene", "step 50: Taunie on the roof"),
            move("off_the_roof", "step 51: off the roof's edge, down to South Boulevard"),
            mash_a("south_boulevard", "step 52: Taunie walks to the Pokemon Center, until Station Front"),
        }),
        row("03.04", "Pokemon Center, the Pokedex", in(LUMIOSE), {
            move("into_pokemon_center", "step 53: into the Pokemon Center, to the nurse's \"A Talk\""),
            talk("talk_to_nurse", "step 53: A at the nurse"),
            action("talk_to_pokedex_screen", "pokedex_app", "step 54: the Pokedex app (\"Ready!\"), Chikorita's entry, B on its screen"),
            mash_b("after_pokedex", "step 55: the nurse's wrap-up, Taunie outside; autosave"),
        }),
    };
}

std::vector<StoryCheckpoint> mission_03_wild_zone(){
    return {
        //  Starts on 03.04's save outside the Pokemon Center.
        row("03.05", "To Wild Zone 1", in(LUMIOSE), {
            move("center_to_zone_1", "step 56: from the Pokemon Center to Wild Zone 1's gate"),
            move("toward_zone_1_gate", "step 57: toward the gate, into Taunie's scene"),
            mash_a("wild_zones_tutorial", "step 57: Taunie on wild zones, the Wild Zones card"),
            move("into_zone_1", "step 58: to the gate's \"A Enter\""),
            mash_a("enter_zone_1", "step 58: A Enter, Taunie, the Throwing Poke Balls card"),
        }),
        //  Design only (not code): Wild Zone 1's catches and crossing.
        row("03.06", "Wild Zone 1: first catches", in(WILD_ZONE_1), {
            action("catch_tutorial", "catch_fletchling", "step 59: catch Fletchling, throwing again when it breaks out"),
            action("battle_then_catch", "catch_bunnelby", "step 60: weaken Bunnelby, then catch it in the catch window"),
            mash_a("after_catches", "step 61: Taunie: tall grass, more Poke Balls, moves smash boulders"),
        }),
        row("03.07", "Five species, into the Research Lab", in(WILD_ZONE_1), {
            move("zone_1_to_far_gate", "step 62: back across Wild Zone 1 to its gate's \"A Leave\""),
            mash_a("leave_zone_1", "step 62: A Leave, out on the street"),
            move("to_research_lab", "step 63: across to the Pokemon Research Lab's gate, into Taunie's scene"),
            mash_a("research_lab_gate", "step 63: Taunie at the lab gate: register five species"),
            action("register_five_species", "register_five_species", "step 64: register five species (3/5 after the tutorial), back to the lab gate"),
            mash_a("five_registered", "step 65: Taunie's scene at the lab gate after 5/5"),
            move("up_the_lab_steps", "step 65: up the steps to the lab's \"A Enter\""),
            mash_a("lab_lobby", "steps 65-66: A Enter, the receptionist: third floor; autosave"),
        }),
        //  Starts on the autosave in the lab lobby.
        row("03.08", "Mable's research, the Rock Smash TM", in(RESEARCH_LAB), {
            move("to_lab_elevator", "step 66: to the elevator's \"A Take Elevator\""),
            talk("take_lab_elevator", "step 66: A Take Elevator, \"Which floor would you like to go to?\""),
            //  By name: "3F: Director's Office" under "2F: Laboratory".
            action("choose_1", "ride_to_3f", "step 67: \"3F: Director's Office\", the second floor listed"),
            mash_b("arrive_3f", "step 67: the ride, until control on 3F"),
            move("to_mable", "step 68: to the acting director's desk, into her talk"),
            action("mash_to_instruction", "mable_talk", "step 69: Mable's research requests, until \"Press X...\", then X"),
            action("claim_mable_research_reward", "rock_smash_tm", "step 70: Mable's Research, Rewards: TM004 Rock Smash"),
            mash_b("leave_mable", "step 71: out of the menu, through Mable's talk, until control in her office"),
        }),
    };
}

std::vector<StoryCheckpoint> make_story_checkpoints(){
    std::vector<StoryCheckpoint> rows;
    for (std::vector<StoryCheckpoint> part : {
        mission_01(), mission_02(),
        mission_03_hotel_z(), mission_03_to_pokemon_center(), mission_03_wild_zone(),
    }){
        for (StoryCheckpoint& checkpoint : part){
            rows.push_back(std::move(checkpoint));
        }
    }
    return rows;
}


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

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> rows = make_story_checkpoints();
    return rows;
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
    static const std::set<std::string, std::less<>> rows{"01.00", "01.01", "02.01", "03.01"};
    return rows;
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

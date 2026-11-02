/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  00.00 to 00.05 are planned walks on the JSON maps, with the scenes'
 *  triggers from the Platinum and Diamond decomps (Segment00Lines) and
 *  Butters' button sequences for the scenes (#1717). They replace his
 *  recorded stick holds, and have NOT run on a console yet: where a scene
 *  leaves the player (pose_after) is the decomp's or a guess, marked
 *  "assumed", until a new-game run checks it. 00.04b to 00.04d and 00.05's
 *  Jubilife walks were measured on the console (r72 to r261, 2026-10-30);
 *  segment 01 ran there end to end (2026-10-31), its walks kept as events.
 *
 */

#include <string>
#include <utility>
#include <vector>
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryLeg;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


namespace{

constexpr Direction NORTH = Direction::NORTH;
constexpr Direction SOUTH = Direction::SOUTH;
constexpr Direction EAST = Direction::EAST;
constexpr Direction WEST = Direction::WEST;

const char* const BEDROOM = "PlayerHouse2F";
const char* const HOME = "PlayerHouse1F";
const char* const BARRYS_HOUSE = "RivalHouse1F";
const char* const BARRYS_ROOM = "RivalHouse2F";
const char* const LAB = "RowansLab";
const char* const TWINLEAF = "TwinleafTown";
const char* const ROUTE_201 = "Route201";
const char* const LAKEFRONT = "VerityLakefront";
const char* const SANDGEM = "SandgemTown";
const char* const ROUTE_202 = "Route202";
const char* const JUBILIFE = "JubilifeCity";

Pose at(const char* map, int x, int y, std::optional<Direction> facing = std::nullopt){
    return Pokemon::tile_pose(map, {TilePos{x, y}}, facing);
}
Pose at(const char* map, const char* point, std::optional<Direction> facing = std::nullopt){
    return Pokemon::tile_pose(map, {map_point(map, point)}, facing);
}
//  The tile next to `point` of `map` on its `side`.
Pose next_to(const char* map, const char* point, Direction side, std::optional<Direction> facing = std::nullopt){
    return Pokemon::tile_pose(map, {map_point(map, point).next(side)}, facing);
}
//  `point` of `map` or the tile on its `side`: where a scene may leave us.
Pose at_or_next_to(const char* map, const char* point, Direction side){
    const TilePos tile = map_point(map, point);
    return Pokemon::tile_pose(map, {tile, tile.next(side)});
}
Pose on(const char* map, const char* region){
    return Pokemon::tile_pose(map, map_region(map, region));
}
//  Columns from below Sandgem's Pokemon Center sign west to below its door.
int sandgem_sign_to_center_door(){
    return map_point(SANDGEM, "pokemon_center_sign").x - map_point(SANDGEM, "pokemon_center_door").x;
}
//  The tile below the door `door` of `map` and its neighbours either side:
//  a door is not lined up with one tile (Jubilife's Center, r85).
Pose door_front(const char* map, const char* door){
    const TilePos below = map_point(map, door).next(SOUTH);
    return Pokemon::tile_pose(map, {below.next(WEST), below, below.next(EAST)});
}

StoryCheckpoint row(
    std::string id, std::string title, std::optional<Pose> start, std::vector<StoryStep> steps,
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY
){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = std::move(title);
    checkpoint.start = std::move(start);
    checkpoint.steps = std::move(steps);
    checkpoint.on_failure = on_failure;
    return checkpoint;
}

StoryStep make(StoryStepKind kind, std::string name, std::string why){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    return step;
}
StoryStep route(std::string name, Pose target, std::string why, std::string costs = ""){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name), std::move(why));
    step.target = std::move(target);
    step.variant = std::move(costs);
    return step;
}
StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, std::string variant = ""){
    StoryStep step = make(StoryStepKind::LEGS, std::move(name), std::move(why));
    step.legs = std::move(legs);
    step.variant = std::move(variant);
    return step;
}
//  Legs that each walk until something stops them, as measured on the console.
StoryStep to_walls(std::string name, std::vector<StoryLeg> legs, std::string why){
    return PokemonBDSP::legs(std::move(name), std::move(legs), std::move(why), "to_wall");
}
//  TODO(#114 coord events): the map schema has no scene triggers yet, so
//  each row steps onto the decomp's trigger tile, named in its reason.
StoryStep step_into(std::string name, Direction direction, std::string why, std::optional<Pose> pose_after = std::nullopt){
    StoryStep step = make(StoryStepKind::STEP, std::move(name), std::move(why));
    step.direction = direction;
    step.pose_after = std::move(pose_after);
    return step;
}
StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}
//  Text to the overworld, with B; with A ("a") for lines that give an item.
StoryStep scene(std::string name, std::optional<Pose> pose_after, std::string why, std::string button = ""){
    StoryStep step = make(StoryStepKind::MASH, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    step.variant = std::move(button);
    return step;
}
StoryStep battle(std::string name, bool may_lose, std::string why, std::string variant = ""){
    StoryStep step = make(StoryStepKind::BATTLE, std::move(name), std::move(why));
    step.may_lose = may_lose;
    step.variant = std::move(variant);
    return step;
}
//  One of the story's events (PokemonBDSP_AutoStory_Events.h), by name.
StoryStep event(std::string name, std::string why, std::optional<Pose> pose_after = std::nullopt){
    StoryStep step = make(StoryStepKind::GAME, name, std::move(why));
    step.variant = std::move(name);
    step.pose_after = std::move(pose_after);
    return step;
}
StoryStep heal(std::string name, std::string why, std::optional<Pose> pose_after = std::nullopt){
    StoryStep step = make(StoryStepKind::HEAL, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}
//  `battles` wild battles in the grass we stand in.
StoryStep train(size_t battles, Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::TRAIN, "train", std::move(why));
    step.battles = battles;
    step.pose_after = std::move(pose_after);
    return step;
}
StoryStep save(){
    return make(StoryStepKind::SAVE, "save_here", "the next checkpoint starts here");
}
StoryStep skip_if(StoryStep step, std::string flag){
    step.skip_if.push_back(std::move(flag));
    return step;
}


//  00.01's last stretch: Barry's Route 201 warning, then west to the Lakefront.
std::vector<StoryStep> route_201_to_the_lakefront(){
    return {
        //  The guitarist's trigger on Twinleaf's row 3 fires only before his talk upstairs.
        //  Barry's warning covers the whole exit path (14..19, 18), so it ends our walk
        //  north: exact legs to the row below it, never a run to a wall through it (r14
        //  ran north to (15, 13) and back, into it and the row-19 pull-back, in a loop).
        legs("below_barrys_warning", {{.direction = EAST, .tiles = 6}, {.direction = NORTH, .tiles = 18}},
            "along row 12, then up Twinleaf's exit to Route 201 (15, 19)"),
        step_into("barrys_warning", NORTH, "the trigger (14..19, 18)"),
        scene("barry_warns_us", at(ROUTE_201, "barry_warning"), "Barry's tall-grass warning; he then stands north of us at (15, 17)"),
        //  West at once, never back south onto row 19, which pulls us back.
        legs("away_from_barry", {{.direction = WEST, .tiles = 1}, {.direction = NORTH, .tiles = 1}, {.direction = WEST, .tiles = 2}},
            "to (12, 17): row 18 west of the path is trees (r16), the dirt goes on along row 17"),
        //  Route 201's lower level, row 16, clear of the grass, then the Lakefront.
        route("below_the_lake_entrance", at(LAKEFRONT, "below_lake_entrance", NORTH), "west along Route 201's lower level to the Lakefront"),
    };
}

//  00.01: down from the bedroom, Barry's house, and out to the Lakefront.
StoryCheckpoint bedroom_to_lakefront(){
    std::vector<StoryStep> steps{
        //  The top row: the Azure Flute's tile (7, 4), then east over the stairs' mat.
        route("below_the_flute", at(BEDROOM, 7, 5, NORTH), "below the Azure Flute's tile"),
        event("azure_flute_if_there", "take the Azure Flute if this save has one", at(BEDROOM, 7, 5, NORTH)),
        legs("onto_the_flute_tile", {{.direction = NORTH, .tiles = 1}, {.direction = EAST, .tiles = 1}},
            "up onto (7, 4), then east onto the top of the stairs' mat (8, 4)"),
        //  The stairs are entered moving east from the top of the two-tile mat,
        //  the decomp's stairs point (8, 4); east from (8, 5) or north are walls (r8, r9).
        //  The stairs' warp lands on 1F's stairs; a tile below is allowed (assumed).
        step_into("down_the_stairs", EAST, "onto the stairs", at_or_next_to(HOME, "stairs", SOUTH)),
        //  Automatic on arrival (1F frame script): Mom walks over, talks, walks back to
        //  (7, 8). No input; Butters' 1.1 s south hold fell inside the fade (timeline 00.01 #2).
        scene("mom_calls", at_or_next_to(HOME, "stairs", SOUTH),
            "Mom calls the player over as they come down; we stay put"),
        route("above_the_door", at(HOME, 6, 9, SOUTH), "past Mom at (7, 8) to above the door"),
        step_into("mom_warning", SOUTH, "the door's tile (6, 10): Mom's warning (coord event, VAR_PLAYER_HOUSE_STATE 1)"),
        scene("mom_warns_us", at(HOME, "door", SOUTH), "\"don't go into the tall grass\""),
        step_into("out_of_the_house", SOUTH, "out of the door", at(TWINLEAF, "house_doorstep", SOUTH)),
        route("short_of_barrys_door", at(TWINLEAF, 10, 12, WEST), "a tile east of Barry's doorstep"),
        step_into("barrys_door", WEST, "the trigger on his doorstep (9, 12)"),
        //  Barry bumps into us and knocks us back a tile, still facing north (decomp;
        //  r10: the first north tap after the scene gave no warp, the second did).
        scene("barry_bursts_out", at(TWINLEAF, 9, 13, NORTH), "Barry runs out, bumps into us and runs back in", "a"),
        scene("barry_goes_home", at(TWINLEAF, 9, 13, NORTH), "his second lines (Butters waited for two)", "a"),
        legs("back_onto_barrys_doorstep", {{.direction = NORTH, .tiles = 1}}, "back to (9, 12); its trigger is spent"),
        step_into("into_barrys_house", NORTH, "his door", at(BARRYS_HOUSE, "door", NORTH)),
        legs("off_barrys_mat", {{.direction = NORTH, .tiles = 1}}, "off the door's mat, which no route starts on"),
        //  Like ours: stairs are entered sideways from the top of their mat, the
        //  stairs point (r11: north from (2, 4) only bumps the wall).
        route("below_barrys_stairs", at(BARRYS_HOUSE, "stairs", EAST), "past his mother at (4, 6) to the top of the stairs' mat"),
        step_into("up_barrys_stairs", EAST, "onto the stairs", at(BARRYS_ROOM, "stairs")),
        //  Automatic on arrival (2F frame script), not a talk: Barry runs past us down
        //  the stairs and the game walks us a tile south of them (decomp; Butters
        //  never walked to him).
        scene("barry_runs_to_the_lake", at(BARRYS_ROOM, 3, 5, NORTH), "Barry's lines; he runs off to the lake", "a"),
        //  Butters went west from there straight into the stairs; ours are entered from
        //  the mat's top tile, so go there first (unchecked: which works).
        route("above_barrys_stairs", at(BARRYS_ROOM, "stairs", WEST), "back to the top of the stairs' mat"),
        step_into("down_barrys_stairs", WEST, "onto the stairs", at(BARRYS_HOUSE, "stairs")),
        route("above_barrys_door", at(BARRYS_HOUSE, 6, 9, SOUTH), "back to his door"),
        step_into("out_of_barrys_house", SOUTH, "out of his door", at(TWINLEAF, "barry_doorstep", SOUTH)),
    };
    for (StoryStep& step : route_201_to_the_lakefront()){
        steps.push_back(std::move(step));
    }
    return row("00.01", "Bedroom to the Verity Lakefront", at(BEDROOM, "start", NORTH), std::move(steps));
}

std::vector<StoryCheckpoint> intro(){
    StoryStep new_game = make(StoryStepKind::NEW_GAME, "intro", "language, appearance, the player's name, Barry's default name");
    new_game.pose_after = at(BEDROOM, "start", NORTH);
    return {
        row("00.00", "New game: the intro and the settings", std::nullopt, {
            skip_if(new_game, "save_loaded"),
            make(StoryStepKind::OPTIONS, "settings", "fast text, Send to Boxes automatic, no nickname prompt, Autosave off"),
        }, StoryFailure::STOP),
        bedroom_to_lakefront(),
    };
}

std::vector<StoryCheckpoint> lake_and_pokedex(){
    return {
        row("00.02", "Lake Verity: the starter and the Starly battle", at(LAKEFRONT, "below_lake_entrance", NORTH), {
            legs("to_the_lake_entrance", {{.direction = NORTH, .tiles = 1}}, "a tile short of the trigger"),
            step_into("lake_entrance", NORTH, "the trigger (16..17, 12): Barry hears Rowan and the two walk in"),
            event("starter_briefcase", "the lake's scene, to the open briefcase"),
            event("choose_starter", "the starter from the program's option"),
            event("fight_starly", "the Starly battle"),
            //  The game walks us out of the lake, then the Lakefront's scene walks us 3
            //  south: (17, 14) facing south, or a tile below (decomp; timeline 00.02 #5).
            event("after_starly", "Rowan's lines and the walk out of the lake",
                at_or_next_to(LAKEFRONT, "below_lake_entrance", SOUTH)),
        }),
        row("00.03", "Back to Sandgem: Rowan, Dawn and the Pokedex", at_or_next_to(LAKEFRONT, "below_lake_entrance", SOUTH), {
            route("short_of_rowan", at(ROUTE_201, 4, 14, EAST), "back east onto Route 201's lower level"),
            step_into("rowans_stop", EAST, "the trigger (5, 14..16)"),
            scene("rowan_and_dawn", at(ROUTE_201, "rowan_stop"), "Rowan and Dawn come back for the Pokedex's owner"),
            //  Through the grass now, with a starter: wild battles are run from.
            route("short_of_dawn", at(SANDGEM, 3, 12, EAST), "east along Route 201 to Sandgem's edge"),
            step_into("dawns_stop", EAST, "the trigger (4, 10..15)"),
            event("rowan_gives_pokedex", "Dawn walks us to the lab: Rowan, no nickname, the Pokedex",
                at(LAB, "rowan_talk", NORTH)),
        }),
    };
}

std::vector<StoryCheckpoint> to_the_catch(){
    return {
        row("00.04", "Home to Mom, then Dawn's catching tutorial on Route 202", at(LAB, "rowan_talk", NORTH), {
            route("above_the_lab_door", at(LAB, 7, 14, SOUTH), "down the lab to its door (7, 15)"),
            step_into("out_of_the_lab", SOUTH, "out of the lab", door_front(SANDGEM, "lab_door")),
            //  Dawn shows us the Center and the Mart; she leaves us by the Mart's door (assumed).
            scene("dawn_shows_us_sandgem", door_front(SANDGEM, "poke_mart_door"), "Dawn's tour of Sandgem"),
            //  First to one known tile: the trees that end the doors' row.
            route("to_the_row_end", at(SANDGEM, "front_row_east_end"), "east along the doors' row to the trees", "town"),
            route("to_route_201", at(ROUTE_201, "route_201_east_end"), "west along the doors' row out of Sandgem"),
            route("home", at(TWINLEAF, "house_doorstep", NORTH), "west along Route 201 to Twinleaf, running from wild battles"),
            step_into("into_the_house", NORTH, "the house's door", at(HOME, "door", NORTH)),
            legs("off_the_mat", {{.direction = NORTH, .tiles = 1}}, "off the door's mat, which no route starts on"),
            route("beside_mom", at(HOME, 6, 8, EAST), "west of Mom at (7, 8)"),
            talk("mom", "Mom"),
            scene("mom_running_shoes", at(HOME, 6, 8, EAST), "her lines"),
            route("back_above_the_door", at(HOME, 6, 9, SOUTH), "Barry's mother stands in the door"),
            event("barrys_mother", "she gives us Barry's parcel", at(HOME, 6, 9, SOUTH)),
            step_into("out_of_the_house_again", SOUTH, "out of the door", at(TWINLEAF, "house_doorstep", SOUTH)),
            route("short_of_the_catch_tutorial", at(ROUTE_202, 22, 27, WEST), "through Sandgem onto Route 202, east of its trigger column"),
            step_into("catch_tutorial_trigger", WEST, "the trigger (21, 25..29), VAR_ROUTE_202_STATE 0"),
            event("catch_tutorial", "Dawn's catching tutorial"),
            event("catch_pokemon", "a Pokemon of our own from the grass", on(ROUTE_202, "catch_save_area")),
        }),
        //  A Lv5 starter fainted to Route 202's trainers (r72): training, a
        //  heal, and Potions first, each row from the save the one before ends on.
        row("00.04b", "After the catch: train and heal in Sandgem", on(ROUTE_202, "catch_save_area"), {
            route("to_catch_grass", on(ROUTE_202, "catch_grass"), "into the training column's grass"),
            train(3, on(ROUTE_202, "after_training_area"), "three wild battles with the best move"),
            //  One walk round the houses ended two tiles short (r252): down
            //  the road first, then the town plan.
            route("into_sandgem", at(SANDGEM, "route_202_entry_stop"), "south down the road to above the Mart", "robust"),
            route("to_center_sign", next_to(SANDGEM, "pokemon_center_sign", SOUTH), "below the Center's sign, its doorway in sight", "town"),
            //  From below the sign the finder read the signs' panels as
            //  doorways (r252, r255): taps instead. One tile short is fine.
            legs("below_center_door", {{.direction = WEST, .tiles = sandgem_sign_to_center_door()}}, "taps west below the Center's door", "one_short_ok"),
            heal("heal_sandgem", "Sandgem Town Pokemon Center", door_front(SANDGEM, "pokemon_center_door")),
        }),
    };
}

std::vector<StoryCheckpoint> to_jubilife(){
    StoryStep potions = make(StoryStepKind::SHOP, "potions",
        "Potions before Route 202's trainers: the Poke Mart lady's 10 (she stands still, as in the decomp)");
    potions.items = {{.item = "potion", .quantity = 10}};
    potions.variant = "sandgem_potions";
    potions.pose_after = at(ROUTE_202, "save_spot");
    StoryStep stairs = step_into("into_jubilife", NORTH,
        "Jubilife's south edge is a wall with stairs in it, west of where the route may leave us (r78)");
    stairs.variant = "jubilife_stairs";
    return {
        row("00.04c", "Potions, back to Route 202's save spot", door_front(SANDGEM, "pokemon_center_door"), {
            potions,
        }),
        //  From the one save spot: from both at once the planner gives up.
        row("00.04d", "Route 202's trainers, Dawn in Jubilife", at(ROUTE_202, "save_spot"), {
            route("north_to_jubilife", on(ROUTE_202, "jubilife_exit"), "north past Route 202's trainers, fighting who sees us"),
            stairs,
            scene("dawn_scene", std::nullopt, "Dawn walks over while we stand still"),
        }),
    };
}

//  The Jubilife beats, stepped through on the console on 2026-10-30: every
//  leg walks until something stops it, and each talk stands beside the
//  person; the coupons and the Poketch end on the street east to Route 203.
std::vector<StoryCheckpoint> jubilife(){
    return {
        row("00.05", "Jubilife: the Center, the Trainers' School and the Town Map", std::nullopt, {
            event("up_the_steps", "a tile west of the save below the steps, then up them", on(JUBILIFE, "up_the_steps")),
            to_walls("to_the_sign", {{.direction = WEST, .tiles = 8}, {.direction = NORTH, .tiles = 8}},
                "west to the fence's end, north to the sign by the lamp west of the road"),
            to_walls("to_the_center", {{.direction = EAST, .tiles = 14}, {.direction = NORTH, .tiles = 14}},
                "east across the road to the house, north up the sidewalk to the Center's doorway"),
            //  The kids south of the Center can push us aside: the heal lines up by sight.
            heal("heal_jubilife", "in, the nurse, out onto the doorstep (r143a: check we are out)"),
            event("into_the_school", "eleven tiles west of the Center's doorstep, north through the School's door"),
            to_walls("up_to_barry", {{.direction = NORTH, .tiles = 10}}, "the door's column to the back wall, beside Barry"),
            event("barry_town_map", "Barry gives the Town Map"),
            event("out_of_the_school", "straight down the door's column and out"),
        }),
        row("00.05b", "Jubilife: the president and the clowns' coupons", std::nullopt, {
            event("meet_the_president", "four tiles east into the road; the president walks up and talks"),
            to_walls("east_to_the_lamp", {{.direction = EAST, .tiles = 8}}, "east until the lamp by the Center"),
            event("up_to_the_mart_road", "a tile west, then north onto the road between the Center and the Mart"),
            to_walls("east_to_the_building", {{.direction = EAST, .tiles = 14}}, "east along it until the tall building"),
            event("clown_by_the_mart", "his question, Yes, and a coupon"),
            //  The TV station's screen keeps the picture moving after we stop:
            //  r161 saw the stop only at 14 tiles' time, r162a not within it.
            to_walls("to_the_west_lamp", {{.direction = WEST, .tiles = 20}, {.direction = SOUTH, .tiles = 10},
                {.direction = WEST, .tiles = 26}, {.direction = NORTH, .tiles = 6}},
                "west to the hedges, south to the long road, west to the lamp, north a row"),
            event("clown_by_the_company", "along the hedges to the gap by the Poketch Company's sign; he comes by"),
        }),
        row("00.05c", "Jubilife: the Poketch, up to the street east to Route 203", std::nullopt, {
            event("to_the_main_road", "a row south of the gap, east along the long road to the main road"),
            event("to_the_center_lamp", "six rows down, east to the Mart's wall, south to the Center's lamp"),
            event("poketch_from_the_president", "he trades the coupons for the Poketch"),
            event("up_to_the_route_203_street", "let him walk off, then ten rows north"),
        }),
    };
}

std::vector<StoryCheckpoint> to_oreburgh(){
    return {
        row("01.01", "Jubilife to Barry", std::nullopt, {
            event("jubilife_center_to_street", "out of the Center to the street east to Route 203, unless we stand there"),
            event("east_to_barry", "east until Barry stops us at Route 203's entrance"),
            battle("barry", true, "Barry's battle; a loss goes on, as it did on the console"),
        }),
        row("01.02", "Route 203 to Oreburgh", std::nullopt, {
            event("route_203_east_to_the_gate", "east along Route 203, fighting the trainers who stop us"),
            skip_if(event("the_hiker", "the Hiker in the Gate gives HM Rock Smash"), "met_hiker"),
            event("hidden_moves_card", "the card after him, closed with B"),
            event("gate_east_to_oreburgh", "east past Curtis and Diana into Oreburgh"),
            event("oreburgh_welcome", "the man at the entrance, and Barry at the Center"),
        }),
        row("01.03", "Oreburgh Mine to Roark", std::nullopt, {
            event("oreburgh_center_door", "in by the Center's door, talking to Barry in it"),
            event("south_into_the_mine", "straight south from the door into the mine"),
            event("mine_hall", "through the entrance hall"),
            event("roark_in_the_mine", "down the corridor to Roark, his talk, and back out"),
            event("oreburgh_center_heal", "the Center's nurse: 01.04 starts at her counter"),
        }),
        row("01.04", "Oreburgh Gym", std::nullopt, {
            event("out_of_the_center", "out from the counter"),
            event("gym_until_roark_beaten", "west into the gym, up it to Roark; again from the Center after a loss"),
        }),
        row("01.05", "Oreburgh to Route 203", std::nullopt, {
            event("out_of_the_gym", "out of the gym onto its doorstep"),
            event("into_oreburgh_gate", "up to the platform and west into the Gate"),
            event("out_of_oreburgh_gate", "west until Route 203's banner shows"),
        }),
    };
}

std::vector<StoryCheckpoint> to_floaroma(){
    return {
        row("01.06", "Route 203 to Jubilife", std::nullopt, {
            event("route_203_to_row_18", "on Route 203's map to row 18, below the girl"),
            event("row_18_into_jubilife", "west along row 18, counting tiles"),
        }),
        row("01.07", "Jubilife through Rowan's scene", std::nullopt, {
            event("west_to_the_crossroads", "west along the long road"),
            event("north_to_rowan", "north up the street until Rowan's scene"),
            battle("galactic_grunts", false, "the tag battle with Dawn", "doubles"),
            event("rowan_after_the_battle", "Rowan's and Dawn's lines"),
        }),
        row("01.08", "Jubilife to the Ravaged Path", std::nullopt, {
            event("north_out_of_jubilife", "north until Route 204's banner"),
            event("route_204_south_to_the_door", "on Route 204 South's map to below the Ravaged Path's door"),
            event("into_the_ravaged_path", "north through the door"),
        }),
        row("01.09", "Ravaged Path", std::nullopt, {
            event("through_the_ravaged_path", "through the cave with Rock Smash, out onto Route 204 North"),
        }),
        row("01.10", "Route 204 to Floaroma", std::nullopt, {
            event("route_204_to_floaroma", "the human's line north into Floaroma"),
            save(),
        }),
        row("01.11", "Floaroma's old couple", std::nullopt, {
            event("west_8_north_to_the_old_woman", "west 8, north until she stops us"),
            talk("old_woman", "she gives Mew when a Let's Go save is on the console"),
            scene("old_woman_talks", std::nullopt, "her lines"),
            event("below_the_old_man", "a tile west, a tap north"),
            talk("old_man", "he gives Jirachi with a Sword/Shield save"),
            scene("old_man_talks", std::nullopt, "his lines"),
            save(),
        }),
    };
}

std::vector<StoryCheckpoint> make_table(){
    std::vector<StoryCheckpoint> table;
    for (auto part : {intro, lake_and_pokedex, to_the_catch, to_jubilife, jubilife, to_oreburgh, to_floaroma}){
        for (StoryCheckpoint& checkpoint : part()){
            table.push_back(std::move(checkpoint));
        }
    }
    return table;
}

}


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = make_table();
    return checkpoints;
}



}
}
}

/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  00.00 to 00.05 are planned walks on the JSON maps, with the scenes'
 *  triggers from the Platinum and Diamond decomps (Segment00Lines) and
 *  Butters' button sequences for the scenes (#1717). They replace his
 *  recorded stick holds, and have NOT run on a console yet: where a scene
 *  leaves the player (pose_after) is the decomp's or a guess, marked
 *  "assumed", until a new-game run checks it. 00.04b to 00.04d were measured
 *  on the console (r72 to r261, 2026-10-30); 00.05 to 00.05c, measured there
 *  first, are now planned on Jubilife's map with the people and triggers of
 *  the Diamond decomp's zone events and scripts. Segment 01 ran on the
 *  console end to end (2026-10-31), its walks as events; all of it but
 *  01.04's Gym is now planned on the maps the same way (2026-11-02), and
 *  has not run on a console as plans yet.
 *
 */

#include <format>
#include <map>
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
//  No map of it: a pose there names the room, not its tiles.
const char* const TRAINERS_SCHOOL = "TrainersSchool";
const char* const OREBURGH = "OreburghCity";
const char* const GYM = "OreburghGym";
//  The mine's entrance hall (Diamond's MAP_OREBURGH_MINE_B1F) and the coal
//  room below it, Roark's.
const char* const MINE_HALL = "OreburghMineB1F";
const char* const MINE = "OreburghMine";
const char* const GATE = "OreburghGate";
const char* const ROUTE_203 = "Route203";
const char* const ROUTE_204_SOUTH = "Route204South";
const char* const RAVAGED_PATH = "RavagedPath";
const char* const ROUTE_204_NORTH = "Route204North";
const char* const FLOAROMA = "FloaromaTown";

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
//  Somewhere in `map`, which has no tiles here: enough for a door step to
//  know it goes out to another map.
Pose inside(const char* map){
    return Pokemon::tile_pose(map, {});
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
//  `step`, after which the player stands on `pose`: a walk to a wall from
//  a few tiles ends on one.
StoryStep anchored(StoryStep step, Pose pose){
    step.pose_after = std::move(pose);
    return step;
}
//  TODO(#114 coord events): the map schema has no scene triggers yet, so
//  each row steps onto the decomp's trigger tile, named in its reason.
StoryStep step_into(std::string name, Direction direction, std::string why, std::optional<Pose> pose_after = std::nullopt){
    StoryStep step = make(StoryStepKind::STEP, std::move(name), std::move(why));
    step.direction = direction;
    step.pose_after = std::move(pose_after);
    return step;
}
//  Through a door or a cave mouth `direction` a D-pad tap at a time, each
//  looked at, ending on the far map's location banner (the executor's
//  "to_banner"): a hold can carry on past a fade it missed (r75, r219g).
//  A tap after an unseen fade may step once more on the far side, so
//  `pose_after` allows for it.
StoryStep tap_into(std::string name, Direction direction, std::string why, Pose pose_after){
    StoryStep step = step_into(std::move(name), direction, std::move(why), std::move(pose_after));
    step.variant = "to_banner";
    return step;
}
//  Through a door or cave warp by a single D-pad tap onto the warp tile,
//  settling on the far map without holding/running (which overshoots if the
//  fade is missed or delayed).
StoryStep tap_step(std::string name, Direction direction, std::string why, Pose pose_after){
    StoryStep step = step_into(std::move(name), direction, std::move(why), std::move(pose_after));
    step.variant = "tap";
    return step;
}
StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}
//  A talk to someone who wanders a tile either side of home along a row:
//  asks north from each tile below the range, answers with A and steps back
//  below home (the "talk_to_wanderer" event).
StoryStep talk_to_wanderer(std::string name, Pose pose_after, std::string why){
    StoryStep step = talk(std::move(name), std::move(why));
    step.variant = "talk_to_wanderer";
    step.pose_after = std::move(pose_after);
    return step;
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
        anchored(legs("below_barrys_warning", {{.direction = EAST, .tiles = 6}, {.direction = NORTH, .tiles = 18}},
            "along row 12, then up Twinleaf's exit to Route 201 (15, 19)"), at(ROUTE_201, 15, 19, NORTH)),
        step_into("barrys_warning", NORTH, "the trigger (14..19, 18)"),
        scene("barry_warns_us", at(ROUTE_201, "barry_warning"), "Barry's tall-grass warning; he then stands north of us at (15, 17)"),
        //  West at once, never back south onto row 19, which pulls us back.
        legs("away_from_barry", {{.direction = WEST, .tiles = 1}, {.direction = NORTH, .tiles = 2}},
            "to (14, 16): row 18 west of the path is trees (r16) and (13, 17) is the 201 sign (probe-r201-sign), so clear both onto row 16"),
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
        //  In BDSP Mom's downstairs scene already warned us about the tall grass,
        //  so the door's tile (6, 10) warps right out without Platinum's coord event.
        step_into("out_of_the_house", SOUTH, "onto the door's tile (6, 10), and out", at(TWINLEAF, "house_doorstep", SOUTH)),
        route("short_of_barrys_door", at(TWINLEAF, 10, 12, WEST), "a tile east of Barry's doorstep"),
        step_into("barrys_door", WEST, "the trigger on his doorstep (9, 12)"),
        //  Barry bumps into us and knocks us back a tile, still facing north (decomp;
        //  r10: the first north tap after the scene gave no warp, the second did).
        scene("barry_bursts_out", at(TWINLEAF, 9, 13, NORTH), "Barry runs out, bumps into us and runs back in", "a"),
        scene("barry_goes_home", at(TWINLEAF, 9, 13, NORTH), "his second lines (Butters waited for two)", "a"),
        //  From (9, 13) straight north through (9, 12) (its trigger is spent) into
        //  his door: a separate tap to (9, 12) first stepped into the door before
        //  into_barrys_house watched for the fade.
        step_into("into_barrys_house", NORTH, "past (9, 12) into his door", at(BARRYS_HOUSE, "door", NORTH)),
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
            battle("starly", true, "the Starly battle, with the starter alone", "starter"),
            //  The game walks us out of the lake, then the Lakefront's scene walks us 3
            //  south: (17, 14) facing south, or a tile below (decomp; timeline 00.02 #5).
            //  Butters' sequence was B for a fixed 60 s; the scene rule ends it on
            //  the quiet overworld instead (unchecked on the console).
            scene("after_starly", at_or_next_to(LAKEFRONT, "below_lake_entrance", SOUTH),
                "Rowan's lines and the walk out of the lake"),
        }),
        row("00.03", "Back to Sandgem: Rowan, Dawn and the Pokedex", at_or_next_to(LAKEFRONT, "below_lake_entrance", SOUTH), {
            route("short_of_rowan", at(ROUTE_201, 4, 14, EAST), "back east onto Route 201's lower level"),
            step_into("rowans_stop", EAST, "the trigger (5, 14..16)"),
            //  BDSP, unlike Diamond: after Rowan leaves, Dawn's and Barry's lines, then
            //  the game fades to the house, where Mom gives the Running Shoes. The
            //  scene ends beside her on the rug, facing her: (6, 8) in r24 and r27,
            //  (5, 8) in r28.
            scene("rowan_and_dawn", Pokemon::tile_pose(HOME, {TilePos{5, 8}, TilePos{6, 8}}, EAST),
                "Rowan and Dawn come back; Barry, then home for the Running Shoes"),
            //  Mom's warning was spent in 00.01, so the door's tile (6, 10) is a plain
            //  door now: stepping onto it goes out (r27).
            //  Line up on the table's corner west first: a plan from either tile
            //  ran east into Mom at (7, 8), who talks (r29).
            route("by_the_counter", at(HOME, 4, 8, WEST), "west against the table's corner (3, 8)"),
            route("above_the_door_again", at(HOME, 6, 9, SOUTH), "to above the door"),
            step_into("out_of_the_house_again", SOUTH, "onto the door's tile (6, 10), and out", at(TWINLEAF, "house_doorstep", SOUTH)),
            //  Through the grass now, with a starter: wild battles are run from.
            route("short_of_dawn", at(ROUTE_201, 63, 5, EAST), "Twinleaf, then east along Route 201 to its east edge"),
            //  Dawn's scene starts as we come into Sandgem (trigger (4, 10..15));
            //  walk until it does rather than to a tile (r35, r36).
            legs("into_sandgem", {{.direction = EAST, .tiles = 8}}, "east into Sandgem until Dawn's scene starts", "to_dialogue"),
            //  Butters' sequence pressed B through Dawn's and Rowan's lines and
            //  the nickname question (No), then A for the Pokedex. The scene's
            //  B answers No too, and closes an item's lines as it did Barry's
            //  parcel (r45). Into the lab is a fade.
            scene("rowan_gives_pokedex", at(LAB, "rowan_talk", NORTH),
                "Dawn walks us to the lab: Rowan, no nickname, the Pokedex", "a"),
        }),
    };
}

//  West along Sandgem's path (row 12) until Route 201's banner shows: it
//  shows a tile or a few past the border, Sandgem (-1, 12) = Route 201 (63, 5).
StoryStep route_201_by_the_path(){
    StoryStep step = legs("to_route_201", {{.direction = WEST, .tiles = 40}}, "run west along the path until Route 201's banner", "to_map");
    step.pose_after = Pokemon::tile_pose(ROUTE_201, {
        TilePos{57, 5}, TilePos{58, 5}, TilePos{59, 5}, TilePos{60, 5}, TilePos{61, 5}, TilePos{62, 5}, TilePos{63, 5}
    }, WEST);
    return step;
}

StoryStep out_of_the_lab_into_dawns_tour(){
    //  Rowan's spot is in the door's column: run south out of the door. Dawn's
    //  tour plays on arrival, inside the step's settle (r43: a scene row after
    //  it waited 20 s for lines already played). She leaves us below the Mart's
    //  door (27, 10), a row or two down: r39's walk west from "row 11" ran on
    //  along row 12, which is open to Route 201.
    return step_into("out_of_the_lab", SOUTH, "out of the lab; Dawn's tour of Sandgem", Pokemon::tile_pose(SANDGEM, {
            TilePos{26, 11}, TilePos{27, 11}, TilePos{28, 11}, TilePos{26, 12}, TilePos{27, 12}, TilePos{28, 12}
        }, NORTH));
}

//  West along Route 201 to Mom's doorstep:
//  1. Over the ledge at column 57 down onto Route 201's main road (row 14).
//  2. West along the main road (row 14) to the Twinleaf path at column 15.
//  3. South down column 15 into Twinleaf Town to row 22.
//  4. East to Mom's house doorstep at (20, 22), facing north into the door.
std::vector<StoryStep> home_to_the_doors_column(){
    return {
        route("to_ledge_column", at(ROUTE_201, 55, 5, WEST), "west to ledge column 55", "line"),
        anchored(legs("over_the_ledge", {{.direction = SOUTH, .tiles = 8}}, "south over the ledge to the lower road"),
            at(ROUTE_201, 55, 14, SOUTH)),
        route("along_route_201_road", at(ROUTE_201, 15, 14, WEST), "west along Route 201's road to the Twinleaf path", "line"),
        route("into_twinleaf", at(TWINLEAF, 15, 22, SOUTH), "south down column 15 into Twinleaf Town", "line"),
        route("to_moms_doorstep", at(TWINLEAF, 20, 22, NORTH), "east to Mom's house doorstep", "line"),
    };
}

std::vector<StoryCheckpoint> to_the_catch(){
    std::vector<StoryStep> steps{
        out_of_the_lab_into_dawns_tour(),
        //  Not west along the doors' row: running it took us into the lab at its
        //  mat (8, 11) (r41, r42). Row 12 is open into Route 201: run it until
        //  Route 201's banner shows, a few tiles in.
        route("onto_the_path", at(SANDGEM, 29, 12, WEST), "to the east end of the path below the doors' row", "town"),
        route_201_by_the_path(),
    };
    for (StoryStep& s : home_to_the_doors_column()){
        steps.push_back(std::move(s));
    }
    steps.push_back(step_into("into_the_house", NORTH, "the house's door", at(HOME, 6, 9, NORTH)));
    steps.push_back(route("beside_mom", at(HOME, 6, 8, EAST), "west of Mom at (7, 8)"));
    steps.push_back(talk("mom", "Mom"));
    steps.push_back(scene("mom_heals_us", at(HOME, 7, 7, SOUTH), "Mom heals us; Barry's mom gives us his parcel"));
    steps.push_back(legs("around_mom", {{.direction = WEST, .tiles = 1}, {.direction = SOUTH, .tiles = 2}},
        "around Mom to (6, 9) above the door"));
    steps.push_back(step_into("to_moms_call", SOUTH, "onto the door mat (6, 10), triggering Mom's call"));
    steps.push_back(scene("moms_hat", at(TWINLEAF, 19, 22, EAST), "Mom's hat; she walks out with us"));
    steps.push_back(route("short_of_the_catch_tutorial", at(ROUTE_202, 22, 27, WEST), "through Sandgem onto Route 202, east of its trigger column"));
    steps.push_back(step_into("catch_tutorial_trigger", WEST, "the trigger (21, 25..29), VAR_ROUTE_202_STATE 0"));
    steps.push_back(event("catch_tutorial", "Dawn's catching tutorial"));
    steps.push_back(event("catch_pokemon", "a Pokemon of our own from the grass", on(ROUTE_202, "catch_save_area")));

    return {
        row("00.04", "Home to Mom, then Dawn's catching tutorial on Route 202", at(LAB, "rowan_talk", NORTH), std::move(steps)),
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

//  Where Dawn's scene leaves 00.04d, and 00.05's save: below Jubilife's
//  steps (45..47, 61), against the wall east of them, (48, 62). BDSP starts
//  her scene as we come in below the steps, not on Diamond's trigger above
//  them (45..48, 60): 00.05's old event went "a tile west, then up the
//  steps" from its save, two taps north (2026-10-30). Not measured as a
//  tile: the event's numbers and the map's (#134).
Pose below_jubilifes_steps(){
    return at(JUBILIFE, 48, 62);
}

std::vector<StoryCheckpoint> to_jubilife(){
    StoryStep potions = make(StoryStepKind::SHOP, "potions",
        "Potions before Route 202's trainers: the Poke Mart lady's 10 (she stands still, as in the decomp)");
    potions.items = {{.item = "potion", .quantity = 10}};
    potions.variant = "sandgem_potions";
    potions.pose_after = at(ROUTE_202, "save_spot");
    //  An event, not a step: it walks north and west until Dawn's scene
    //  starts. It leaves us on Jubilife, so the scene after it plays where
    //  we stand, with no fade to wait for.
    const StoryStep stairs = event("jubilife_stairs",
        "Jubilife's south edge is a wall with stairs in it, west of where the route may leave us (r78)",
        below_jubilifes_steps());
    return {
        row("00.04c", "Poke Mart lady for Potions, back to Route 202's save spot", door_front(SANDGEM, "pokemon_center_door"), {
            potions,
        }),
        //  From the one save spot: from both at once the planner gives up.
        row("00.04d", "Route 202's trainers, Dawn in Jubilife", at(ROUTE_202, "save_spot"), {
            route("north_to_jubilife", on(ROUTE_202, "jubilife_exit"), "north past Route 202's trainers, fighting who sees us"),
            stairs,
            scene("dawn_scene", below_jubilifes_steps(), "Dawn walks over while we stand still"),
        }),
    };
}

//  Below the Trainers' School's door, where it lets us out (40, 41).
Pose below_the_school(){
    return next_to(JUBILIFE, "trainers_school_door", SOUTH, SOUTH);
}

//  00.05: the steps as measured on the console (2026-10-30); the rest is
//  planned on the map.
StoryCheckpoint the_town_map(){
    return row("00.05", "Jubilife: the Center, the Trainers' School and the Town Map", below_jubilifes_steps(), {
        //  D-pad taps, a tile each: the stick holds this was (200 ms west,
        //  480 ms north) left us between tiles, and every leg after it a
        //  column off (r57 to r61).
        legs("up_the_steps", {{.direction = WEST, .tiles = 1}, {.direction = NORTH, .tiles = 2}},
            "a tile west of the save, then up the steps to (47, 60)"),
        //  Planned on the map: the hand-measured walls (west to the sign, east
        //  across the road, north) leaned on the old turn's nudge and ended
        //  beside the Trainers' School once turns stopped stepping (r57, r58).
        route("to_the_center", at(JUBILIFE, 52, 41, NORTH),
            "below the Center's door (52, 40)", "town"),
        //  The kids south of the Center can push us aside: the heal lines up by sight.
        heal("heal_jubilife", "in, the nurse, out onto the doorstep (r143a: check we are out)"),
        //  Same rule as the Center: plan to the tile below the door, then run in
        //  (r63: "eleven tiles west" counted on the old turn's step; the map says twelve).
        route("to_the_school", next_to(JUBILIFE, "trainers_school_door", SOUTH, NORTH),
            "below the School's door (40, 40)", "town"),
        step_into("into_the_school", NORTH, "the School's door", inside(TRAINERS_SCHOOL)),
        //  Barry stands at the blackboard a tile west of the door's column,
        //  which is clear to the back wall: beside him there, face west and
        //  talk. A tile west by a hold went two in r149a, and the boy at the
        //  table stopped us. (The event walked to the wall again once when he
        //  did not answer.)
        anchored(to_walls("up_to_barry", {{.direction = NORTH, .tiles = 10}}, "the door's column to the back wall, beside Barry"),
            Pokemon::tile_pose(TRAINERS_SCHOOL, {}, WEST)),
        talk("barry", "Barry, west of us at the blackboard"),
        scene("barry_town_map", inside(TRAINERS_SCHOOL), "his lines to \"You obtained a Town Map!\", closed with A", "a"),
        //  Barry stands beside the door's column, which is clear down to the door.
        step_into("out_of_the_school", SOUTH, "straight down the door's column and out", below_the_school()),
    });
}

//  00.05b, from the Diamond decomp (zone event narc_0002, scripts
//  scr_seq narc_0002; local = the decomp's world tile - (128, 736)): the
//  president's coord event, row 40 from x 44 to 48 while
//  VAR_POKETCH_CAMPAIGN_STATE is 1, walks him from (46, 35) to above us;
//  then the three clowns, each with a question whose answer is Yes.
StoryCheckpoint the_coupons(){
    return row("00.05b", "Jubilife: the president and the clowns' coupons", below_the_school(), {
        //  From x 46 he walks straight down 4 to (46, 39); from the others he turns.
        route("below_the_presidents_trigger", at(JUBILIFE, 46, 41, NORTH), "east along the School's row", "town"),
        step_into("presidents_trigger", NORTH, "the trigger (44..48, 40)"),
        scene("president_walks_up", at(JUBILIFE, 46, 40, NORTH), "he walks down to (46, 39) and sends us for the coupons"),
        //  He stays at (46, 39) until the TV station clown's coupon sends him
        //  home; a plan north from his column would walk into him.
        legs("past_the_president", {{.direction = EAST, .tiles = 2}}, "east of his column"),
        //  He wanders (54..56, 33); he comes through (55, 33) every other move.
        route("below_the_mart_clown", next_to(JUBILIFE, "clown_mart", SOUTH, NORTH), "up the road east of the Center", "town"),
        talk_to_wanderer("mart_clown", next_to(JUBILIFE, "clown_mart", SOUTH, NORTH),
            "the clown by the Mart: 'Do Pokemon grow by defeating others?' Yes; Coupon 1"),
        //  He stands still below the TV station's door, facing south.
        route("below_the_tv_clown", next_to(JUBILIFE, "clown_tv_station", SOUTH, NORTH), "west and north to the TV station", "town"),
        talk("tv_clown", "the clown below the TV station's door"),
        //  His script also puts the president back at (46, 35), facing south.
        scene("tv_clowns_coupon", next_to(JUBILIFE, "clown_tv_station", SOUTH, NORTH), "\"Can a Pokemon hold an item?\" Yes; Coupon 3", "a"),
        //  He wanders (14..16, 18), behind the Poketch Company's sign (13, 19).
        route("below_the_company_clown", next_to(JUBILIFE, "clown_poketch_company", SOUTH, NORTH), "west along the long road, north beside the sign", "town"),
        talk_to_wanderer("company_clown", next_to(JUBILIFE, "clown_poketch_company", SOUTH, NORTH),
            "the clown by the Poketch Company: 'Do moves have types?' Yes; Coupon 2"),
    });
}

//  00.05c: the president trades the coupons for the Poketch, walks 9 north
//  and is gone (decomp); then up to the street east to Route 203, where
//  01.01 walks east until Barry stops us.
StoryCheckpoint the_poketch(){
    const Pose beside_the_president = next_to(JUBILIFE, "poketch_president", WEST, EAST);
    return row("00.05c", "Jubilife: the Poketch, up to the street east to Route 203",
        next_to(JUBILIFE, "clown_poketch_company", SOUTH, NORTH), {
        route("to_main_boulevard", at(JUBILIFE, 15, 22, SOUTH), "south to the main boulevard", "line"),
        route("along_main_boulevard", at(JUBILIFE, 45, 22, SOUTH), "east along the main boulevard", "line"),
        route("beside_the_president", beside_the_president, "south down column 45 to the Poketch president", "line"),
        talk("president", "the Poketch Company's president at (46, 36)"),
        //  BDSP adds a help panel on the Poketch; B closes it.
        scene("the_poketch", beside_the_president, "the coupons for the Poketch; he walks off north"),
        //  The street's rows 21 to 24 lead onto Route 203 at x 63.
        route("to_the_route_203_street", at(JUBILIFE, 58, 22, EAST), "up the main road and east along the street", "line"),
    });
}

std::vector<StoryCheckpoint> jubilife(){
    return {the_town_map(), the_coupons(), the_poketch()};
}

//  `tile` of `map`, or the tile past it `direction`: where a tap_into() may
//  leave us.
Pose arrival(const char* map, TilePos tile, Direction direction){
    return Pokemon::tile_pose(map, {tile, tile.next(direction)}, direction);
}
//  Where the Oreburgh Gate lets us out onto Route 203: its warp tile (55, 9)
//  (Diamond's warp (246, 749), zone event narc_0329), walking west. The one
//  tile only: from it and (54, 9) together the joined Route 203 and Jubilife
//  find no plan. The tap stops at the fade, which puts us on the warp tile;
//  r78 passed 01.06 from a save there (by the sign, the cave mouth east).
Pose out_of_oreburgh_gate(Direction facing = WEST){
    return at(ROUTE_203, 55, 9, facing);
}
//  Barry's coord event on Route 203 (Diamond's zone event narc_0329), column
//  5 on rows 17 to 20 (the street's rows 21 to 24) while
//  VAR_ROUTE_203_RIVAL_STATE is 0. He walks up, battles and runs off, and
//  we stay on it (assumed; the event's walk east stopped there, on any row).
Pose on_barrys_trigger(){
    return Pokemon::tile_pose(ROUTE_203, {TilePos{5, 17}, TilePos{5, 18}, TilePos{5, 19}, TilePos{5, 20}}, EAST);
}
//  Where Route 204's door (11, 1) lets us into the Ravaged Path: (19, 50),
//  as in r237a. The tap stops at the fade, on that tile.
Pose into_the_ravaged_path(){
    return at(RAVAGED_PATH, "entrance", NORTH);
}

//  Where Oreburgh's welcome scene leaves us, Barry in the Gym's door.
//  In Diamond/Pearl/BDSP, the guide walks us to the Gym: (23, 23) facing east,
//  with the guide himself standing directly east at (24, 23).
Pose after_the_welcome(){
    return at(OREBURGH, 23, 23, EAST);
}
//  Below Oreburgh's Gym door (26, 20), facing Barry on the doorstep (26, 21).
Pose below_barry(){
    return at(OREBURGH, 26, 22, NORTH);
}
//  Below Oreburgh's Pokemon Center's door (47, 20), facing it. Not the
//  map's doorstep (46, 21): a route there ended against the wall west of
//  the door and the heal found no door (lab-B, 2026-11-07 20:18).
Pose below_oreburghs_center(){
    return at(OREBURGH, 47, 21, NORTH);
}
//  Out of Oreburgh's Pokemon Center, facing south: below its door, or on
//  the map's doorstep (46, 21), from which 01.04's 20 taps west reach the
//  Gym's door column (r217d, r218d).
Pose out_of_oreburgh_center(){
    return Pokemon::tile_pose(OREBURGH, {TilePos{47, 21}, map_point(OREBURGH, "pokemon_center_doorstep")}, SOUTH);
}

//  01.05, planned on Oreburgh's and the Gate's maps with the Diamond decomp's
//  zone events (narc_0044, narc_0250) and Platinum's script for the scene
//  (scripts_oreburgh_city.s, the same trigger and variable). Barry's coord
//  event, column 6 on rows 12 to 15 while VAR_OREBURGH_CITY_STATE is 2 (after
//  Roark: the old walk met his "*thud*" there, 2026-10-29), runs him into us
//  from the east: the game pushes us a tile west, facing east, and he
//  leaves. The Gate's way west is row 22, across the Hiker's trigger (7, 22),
//  spent in 01.02.
StoryCheckpoint oreburgh_to_route_203(){
    return row("01.05", "Oreburgh to Route 203", next_to(GYM, "roark", SOUTH, NORTH), {
        //  01.04 talks to Roark from below him, up the clear middle column.
        tap_into("out_of_the_gym", SOUTH, "down the middle column (5, 4..23) and out of the door (5, 24)",
            arrival(OREBURGH, map_point(OREBURGH, "gym_doorstep"), SOUTH)),
        route("east_of_barrys_trigger", at(OREBURGH, 7, 13, WEST),
            "west along the street, north up the stairs west of the houses, west along the Gate's row", "town"),
        step_into("barrys_trigger", WEST, "the trigger (6, 12..15)"),
        scene("barry_bumps_into_us", at(OREBURGH, 5, 13, EAST), "Barry runs into us, talks of Eterna, and leaves (assumed: the push)"),
        route("by_the_gate", next_to(OREBURGH, "gate_mouth", EAST, WEST), "west to the Gate's warp tile (2, 13)"),
        //  The warp's other end is the Gate's (27, 22), beside its east mouth.
        tap_into("into_oreburgh_gate", WEST, "the Gate's mouth (1, 13)", arrival(GATE, {27, 22}, WEST)),
        route("through_the_gate", at(GATE, 4, 22, WEST), "west along row 22 to the warp tile by the west mouth (3, 22)"),
        //  Route 203's banner or a failure here: the old walk west "until the
        //  banner" went on into the pond's pocket (r75). 01.06 heads south.
        tap_into("out_of_oreburgh_gate", WEST, "the west mouth (3, 22)", out_of_oreburgh_gate()),
    });
}

//  01.02, planned on Route 203's, the Gate's and Oreburgh's maps with the
//  Diamond decomp's zone events (narc_0329, narc_0250, narc_0044): east
//  along Route 203 to the Gate, fighting the trainers who see us
//  (route_203_trainer_sights()); the Hiker's coord event (7, 22) while
//  VAR_OREBURGH_GATE_1F_HIKER_STATE is 1; east along row 22 past Diana and
//  Curtis (oreburgh_gate_trainer_sights()); then the coord event of the man
//  at Oreburgh's entrance, column 10 on rows 12 to 15 while
//  VAR_OREBURGH_CITY_STATE is 0. Each mouth is a tap to its banner, as in
//  01.05; its warp's other end is the tile beside the far mouth.
//
//  Diana and Curtis are not on the trainer list: the walker fights them as
//  trainers the map lacks, one per route (in the grid sim, the second of
//  two on one leg stopped its walk on to the wall unseen). On the list,
//  the grid sim, which does not know them beaten, had them stop 01.05's
//  walk back.
StoryCheckpoint route_203_to_oreburgh(){
    return row("01.02", "Route 203 to Oreburgh", on_barrys_trigger(), {
        //  One known tile to plan from: from all four of the trigger's rows
        //  at once the planner finds no plan. Its column is open from row
        //  17 down to the cliff below row 20.
        anchored(to_walls("south_to_the_cliff", {{.direction = SOUTH, .tiles = 4}}, "down the trigger's column to the cliff"),
            at(ROUTE_203, 5, 20, SOUTH)),
        route("by_the_gate", out_of_oreburgh_gate(EAST), "east along Route 203 to the Gate's warp tile (55, 9)"),
        tap_into("into_oreburgh_gate", EAST, "the Gate's west mouth (56, 9)", arrival(GATE, {4, 22}, EAST)),
        route("west_of_the_hikers_trigger", at(GATE, 6, 22, EAST), "east along row 22"),
        step_into("hikers_trigger", EAST, "the trigger (7, 22): the Hiker walks down to us"),
        //  The "Hidden Moves" card after his lines is no dialogue box, and
        //  can come up a moment after them; the scene's B closes it.
        scene("the_hiker", at(GATE, 7, 22, EAST), "HM Rock Smash, then the Hidden Moves card"),
        route("past_curtis", at(GATE, 20, 22, EAST), "east along row 22 past Camper Curtis's column (17), fighting him"),
        route("by_the_east_mouth", at(GATE, 27, 22, EAST), "east past Picnicker Diana's column (24), fighting her"),
        tap_into("into_oreburgh", EAST, "the Gate's east mouth (28, 22)",
            arrival(OREBURGH, map_point(OREBURGH, "gate_mouth").next(EAST), EAST)),
        route("west_of_the_welcome", at(OREBURGH, 9, 13, EAST), "east along row 13"),
        step_into("welcome_trigger", EAST, "the trigger (10, 12..15): the man walks over"),
        //  He walks us to the Pokemon Center between his lines; Barry joins
        //  there with more lines after each walk.
        scene("oreburgh_welcome", after_the_welcome(), "the man's lines, the walk to the Gym, and Barry's"),
    });
}

//  01.03, planned on Oreburgh's map and the coal room's (Diamond's zone
//  events narc_0044 and narc_0197): Barry in the Center's door says Roark
//  went to the mine (Roark ignored A until then, r101 and r104); down the
//  door's column to the mine's warp tiles (44..48, 59); straight down the
//  entrance hall to its warps (11..13, 21); in the coal room down to
//  Roark, whose scene ends with him gone (Platinum's
//  scripts_oreburgh_mine_b2f.s: he smashes the rock east of him and walks
//  off east); back up and out; and the Center's nurse, so 01.04 starts on
//  the doorstep. The Center's door is the decomp's (47, 20): out of
//  Sandgem's and Jubilife's Centers the console stood in their doors'
//  columns. The map's doorstep (46, 21) is the Gym's door column plus the
//  20 taps of 01.04 (r217d, r218d), which do not say where the taps began;
//  the heal lines up on the lit doorway if north from (47, 21) misses.
//
//  The hall is walked on counted taps, not held: it is a cave, and a wild
//  battle on a hold toward a fade would read as the fade. Its warp tiles
//  are only entered on the step after, which meets no wild Pokemon. Down,
//  from the city's (46, 59) we land on its (12, 1); up, from the coal
//  room's warp we land on its (11, 21) and come out on the city's (45, 59),
//  below the mine's sign (the old walk north stopped against it, r103).
//
//  Across the coal room to Roark's east corner (20, 28): the north wall above
//  it, his rock (19, 28) to its west. A wild battle on a counted tap re-plans
//  to the end of the step, and a step that ends in a corner ends that re-plan
//  on wall clips. Lab-B 2026-11-08 11:55: the re-plan went only to the end of
//  the interrupted leg, the open tile (15, 16); its last leg, south 5 across
//  the open room, overshot a row, and the taps after ran under Roark's rocks.
std::vector<StoryLeg> legs_to_roarks_corner(){
    return {
        {.direction = EAST,  .tiles = 1},
        {.direction = SOUTH, .tiles = 2},
        {.direction = WEST,  .tiles = 1},
        {.direction = SOUTH, .tiles = 5},
        {.direction = EAST,  .tiles = 14},
        {.direction = SOUTH, .tiles = 10},
        {.direction = WEST,  .tiles = 2},
        {.direction = SOUTH, .tiles = 2},
        {.direction = WEST,  .tiles = 7},
    };
}

std::vector<StoryLeg> legs_out_of_coal_room(){
    return {
        {.direction = EAST,  .tiles = 9},
        {.direction = NORTH, .tiles = 3},
        {.direction = EAST,  .tiles = 2},
        {.direction = NORTH, .tiles = 10},
        {.direction = WEST,  .tiles = 14},
        {.direction = NORTH, .tiles = 5},
        {.direction = EAST,  .tiles = 1},
        {.direction = NORTH, .tiles = 2},
        {.direction = WEST,  .tiles = 1},
        {.direction = NORTH, .tiles = 7},
    };
}

//  01.02b: the welcome leaves the party as Route 203 left it (2026-11-07 runs:
//  the lead paralysed, its water move out of PP, and every reload of 01.03
//  replayed that). Barry's lines first, as before the split (he blocks the
//  street east from his doorstep until then: lab-B 2026-11-08), then the
//  Center, so 01.03's save is a healed party on the doorstep.
StoryCheckpoint oreburgh_center_heal(){
    return row("01.02b", "Oreburgh Center heal", after_the_welcome(), {
        anchored(legs("below_barry", {{.direction = SOUTH, .tiles = 1}, {.direction = EAST, .tiles = 3}, {.direction = NORTH, .tiles = 2}},
            "south 1 around the guide, east 3 to Barry's column, north 2 to below Barry"), below_barry()),
        talk("barry", "Barry in the Gym's door"),
        scene("barrys_lines", below_barry(), "Roark has gone to the mine"),
        route("below_the_centers_door", below_oreburghs_center(), "east along the street to below the Center's door", "town"),
        heal("heal_oreburgh_first", "the nurse; out onto the doorstep, where 01.03 starts", out_of_oreburgh_center()),
    });
}

StoryCheckpoint oreburgh_mine_to_roark(){
    const Pose below_roark = next_to(MINE, "roark", SOUTH, NORTH);
    return row("01.03", "Oreburgh Mine to Roark", out_of_oreburgh_center(), {
        route("above_the_mine", at(OREBURGH, 46, 58, SOUTH), "down the door's column to above the mine's warp tiles", "town"),
        step_into("into_the_mine", SOUTH, "the warp tile (46, 59)", at(MINE_HALL, 12, 1, SOUTH)),
        step_into("into_the_coal_room", SOUTH, "down the entrance hall through the cave mouth", at(MINE, 15, 2, SOUTH)),
        anchored(to_walls("south_to_the_rock", {{.direction = SOUTH, .tiles = 8}}, "south against the corridor rock (15, 11)"),
            at(MINE, 15, 9, SOUTH)),
        anchored(legs("to_roarks_corner", legs_to_roarks_corner(), "east around the rock, across the room, down column 29, west to Roark's rock"),
            at(MINE, 20, 28, WEST)),
        //  Counted taps along the open row 29 drifted (lab-B 2026-11-07: south 3
        //  moved 2): south stops at the wall under row 29, two tiles from below him.
        anchored(to_walls("beside_roarks_rock", {{.direction = SOUTH, .tiles = 1}}, "south against the room's wall"),
            at(MINE, 20, 29, SOUTH)),
        anchored(legs("below_roark", {{.direction = WEST, .tiles = 2}}, "west 2 to below Roark"), below_roark),
        talk("roark", "Roark, north of us between his rocks"),
        scene("roark_smashes_the_rock", below_roark, "his lines, Rock Smash on the rock east of him, and he walks off"),
        anchored(legs("below_the_coal_rooms_warp", legs_out_of_coal_room(), "east around crate, up column 29, west across row 17, and up corridor to warp doorstep"),
            at(MINE, 15, 2, NORTH)),
        step_into("up_to_the_hall", NORTH, "the warp tile (15, 1)", at(MINE_HALL, 11, 21, NORTH)),
        step_into("out_of_the_mine", NORTH, "up the entrance hall and out to the city",
            Pokemon::tile_pose(OREBURGH, {TilePos{45, 59}, TilePos{45, 58}}, NORTH)),
        anchored(to_walls("up_to_the_mines_sign", {{.direction = NORTH, .tiles = 3}}, "north against the mine's sign (45, 56)"),
            at(OREBURGH, 45, 57, NORTH)),
        //  Not a route to below the door: its run north up column 46 to the wall
        //  stopped at row 21 and its open step east ended off the door's column
        //  (no fade; the counter a tile east of the nurse, lab-B 2026-11-08
        //  10:39), and the same run, a column east, went through the door into
        //  the Center (12:04). Route to the street west of the door, the last leg
        //  west along row 22, then line up as 01.02b does, against the walls
        //  east and north of (48, 22), and one tap west below the door.
        route("west_of_the_centers_door", at(OREBURGH, 44, 22, WEST), "up to the street west of the Center's door", "town"),
        anchored(to_walls("against_the_walls_east_of_the_door", {{.direction = EAST, .tiles = 4}, {.direction = NORTH, .tiles = 1}},
            "east against the wall at (49, 22), north against the wall at (48, 20)"), at(OREBURGH, 48, 21, NORTH)),
        anchored(legs("below_the_centers_door_lined_up", {{.direction = WEST, .tiles = 1}}, "a tap west below the door"), below_oreburghs_center()),
        heal("heal_oreburgh", "the nurse; out onto the doorstep, where 01.04 starts", out_of_oreburgh_center()),
    });
}

//  Below Rowan's trigger in Jubilife: (47, 8), the east tile under the
//  trigger (45..47, 7), which (48, 7) beside it is not.
Pose below_rowans_trigger(){
    return at(JUBILIFE, 47, 8, NORTH);
}
//  Where Rowan's scene leaves us: walked up to him at (47, 5), then a tile
//  west in front of the grunt (Platinum's movements; assumed for BDSP).
Pose after_the_grunts(){
    return Pokemon::tile_pose(JUBILIFE, {TilePos{46, 5}, TilePos{47, 5}});
}

//  01.07, from the Diamond decomp (zone event narc_0002) and Platinum's
//  script (scripts_jubilife_city.s, the same trigger and variable): the
//  coord event (45..47, 7) while VAR_JUBILIFE_CITY_STATE is 3 walks us up to
//  Rowan, his and the grunts' lines, then the tag battle with Dawn; after
//  it they leave.
StoryCheckpoint rowan_and_the_grunts(){
    return row("01.07", "Jubilife: Rowan, Dawn and the Galactic grunts", below_rowans_trigger(), {
        step_into("rowans_trigger", NORTH, "the trigger (45..47, 7): the game walks us up to Rowan"),
        battle("galactic_grunts", false, "the lines, then the tag battle with Dawn", "doubles"),
        scene("rowan_and_dawn_leave", after_the_grunts(),
            "Rowan's and Dawn's lines, then Bebe walks over with hers (r80 saved over her lines)"),
    });
}

//  01.09 on the Ravaged Path's map (Platinum's, checked on the console in
//  r235b): up the stairs to west of the rock (22, 43), smash it, east to
//  above the way out (28, 45). Its other end is Route 204's (20, 26).
StoryCheckpoint through_the_ravaged_path(){
    const Pose west_of_the_rock = at(RAVAGED_PATH, "before_rocks.2", EAST);
    return row("01.09", "Ravaged Path", into_the_ravaged_path(), {
        route("west_of_the_rock", west_of_the_rock, "up the stairs and along the corridor"),
        event("smash_the_rock", "Rock Smash on the rock east of us", west_of_the_rock),
        route("above_the_way_out", at(RAVAGED_PATH, "above_exit", SOUTH), "east through the rock's tile"),
        //  One tap and Route 204's banner: a timed walk south went on past
        //  the cave's mouth to the cliff (r81).
        tap_into("out_of_the_ravaged_path", SOUTH, "the way out (28, 45)", at(ROUTE_204_NORTH, "cave_exit", SOUTH)),
    });
}

//  01.10: the line a human walked up Route 204's north half (2026-10-31,
//  r246a to r251a), corner to corner (Test_Route204NorthLine checks each
//  stretch is one straight leg), then north between the flower beds into
//  Floaroma. Its trainers who see us are fought: Taylor on the line, the
//  Twins as a pair.
StoryCheckpoint route_204_to_floaroma(){
    std::vector<StoryStep> steps;
    const std::vector<TilePos> line = map_region(ROUTE_204_NORTH, "floaroma_waypoints");
    for (size_t i = 0; i < line.size(); i++){
        steps.push_back(route(std::format("line_{}", i + 1), Pokemon::tile_pose(ROUTE_204_NORTH, {line[i]}),
            "a corner of the human's line", "line"));
    }
    //  01.11 walks on from here: west 8 along row 29, north to the old woman.
    steps.push_back(route("into_floaroma", at(FLOAROMA, "route_204_arrival", NORTH), "north up column 11 into Floaroma", "line"));
    steps.push_back(save());
    return row("01.10", "Route 204 to Floaroma", at(ROUTE_204_NORTH, "cave_exit", SOUTH), std::move(steps));
}

std::vector<StoryCheckpoint> to_oreburgh(){
    return {
        //  From 00.05c's end on the street east to Route 203.
        row("01.01", "Jubilife to Barry", at(JUBILIFE, 58, 22, EAST), {
            route("west_of_barrys_trigger", at(ROUTE_203, 4, 18, EAST), "east along the street onto Route 203", "town"),
            step_into("barrys_trigger", EAST, "the trigger (5, 17..20): Barry walks over"),
            anchored(battle("barry", true, "Barry's battle; a loss goes on, as it did on the console"), on_barrys_trigger()),
        }),
        route_203_to_oreburgh(),
        oreburgh_center_heal(),
        oreburgh_mine_to_roark(),
        //  Stays an event: after a loss it walks back from the Center with
        //  the experience the battles gave, which a reload of this row's
        //  save would take away (Roark won twice by hand before he lost).
        row("01.04", "Oreburgh Gym", out_of_oreburgh_center(), {
            event("gym_until_roark_beaten", "west into the gym, up it to Roark; again from the Center after a loss",
                next_to(GYM, "roark", SOUTH, NORTH)),
        }),
        oreburgh_to_route_203(),
    };
}

//  01.11 on Floaroma's map: the old woman (3, 25) and the old man (2, 25)
//  west of her stand still above the flower field's bottom row (r251a).
//  The walk to her is the one that ran on the console: 8 taps west along
//  row 29, then north until she stops us (r250a: 3 steps, then bumps).
StoryCheckpoint floaromas_old_couple(){
    const Pose below_the_old_woman = next_to(FLOAROMA, "old_woman", SOUTH, NORTH);
    const Pose below_the_old_man = next_to(FLOAROMA, "old_man", SOUTH, NORTH);
    return row("01.11", "Floaroma's old couple", at(FLOAROMA, "route_204_arrival", NORTH), {
        legs("west_along_the_field", {{.direction = WEST, .tiles = 8}}, "8 taps west along row 29"),
        anchored(to_walls("up_to_the_old_woman", {{.direction = NORTH, .tiles = 5}}, "north until she stops us"),
            below_the_old_woman),
        talk("old_woman", "she gives Mew when a Let's Go save is on the console"),
        scene("old_woman_talks", below_the_old_woman, "her lines"),
        //  He stands above that tile, so talk() turning north only turns.
        anchored(legs("below_the_old_man", {{.direction = WEST, .tiles = 1}}, "a tile west"), below_the_old_man),
        talk("old_man", "he gives Jirachi with a Sword/Shield save"),
        scene("old_man_talks", below_the_old_man, "his lines"),
        save(),
    });
}

std::vector<StoryCheckpoint> to_floaroma(){
    return {
        row("01.06", "Route 203 to Jubilife", out_of_oreburgh_gate(), {
            //  Trainers who saw us going east in 01.02 are beaten; any other
            //  sees us here and is fought (route_203_trainer_sights()).
            route("below_rowans_trigger", below_rowans_trigger(),
                "down Route 203 (its ledges only south), west along it into Jubilife's street, north up the road", "town"),
        }),
        rowan_and_the_grunts(),
        row("01.08", "Jubilife to the Ravaged Path", after_the_grunts(), {
            //  One known tile to plan from: from both of the scene's ends
            //  the joined Jubilife and Route 204 find no plan. Row 5 is open
            //  from x 44 to 49, a wall at 43.
            anchored(to_walls("west_to_the_wall", {{.direction = WEST, .tiles = 4}}, "west along row 5 to the wall"),
                at(JUBILIFE, 44, 5, WEST)),
            //  Lass Sarah's east sight crosses the street (r233a, r234a): she
            //  may stop us for a battle.
            route("below_the_ravaged_path", at(ROUTE_204_SOUTH, "below_door", NORTH),
                "north out of Jubilife, up Route 204's street and grass to below the door (11, 1)", "town"),
            //  Its warp (Diamond's zone event narc_0330) lands us on the
            //  cave's (19, 50), as in r237a.
            tap_into("into_the_ravaged_path", NORTH, "the door (11, 1)", into_the_ravaged_path()),
        }),
        through_the_ravaged_path(),
        route_204_to_floaroma(),
        floaromas_old_couple(),
    };
}

//  The map the save screen's card names at each row's start, as the console
//  read it there (lab-B's logs, 2026-10-31 to 11-02). A building's card
//  names its town: 00.04 starts in Rowan's lab.
const std::map<std::string, std::string>& start_save_cards(){
    static const std::map<std::string, std::string> cards{
        {"00.01",  "Twinleaf Town"},
        {"00.02",  "Verity Lakefront"},
        {"00.03",  "Verity Lakefront"},
        {"00.04",  "Sandgem Town"},
        {"00.04b", "Route 202"},
        {"00.04c", "Sandgem Town"},
        {"00.04d", "Route 202"},
        {"00.05",  "Jubilife City"},
        {"00.05b", "Jubilife City"},
        {"00.05c", "Jubilife City"},
        {"01.01",  "Jubilife City"},
        {"01.02",  "Route 203"},
        {"01.02b", "Oreburgh City"},
        {"01.03",  "Oreburgh City"},
        {"01.04",  "Oreburgh City"},
        {"01.05",  "Oreburgh City"},
        {"01.06",  "Route 203"},
        {"01.07",  "Jubilife City"},
        {"01.08",  "Jubilife City"},
        {"01.09",  "Ravaged Path"},
        {"01.10",  "Route 204"},
        {"01.11",  "Floaroma Town"},
    };
    return cards;
}

std::vector<StoryCheckpoint> make_table(){
    std::vector<StoryCheckpoint> table;
    for (auto part : {intro, lake_and_pokedex, to_the_catch, to_jubilife, jubilife, to_oreburgh, to_floaroma}){
        for (StoryCheckpoint& checkpoint : part()){
            table.push_back(std::move(checkpoint));
        }
    }
    for (StoryCheckpoint& checkpoint : table){
        const auto card = start_save_cards().find(checkpoint.id);
        if (checkpoint.on_failure == StoryFailure::RELOAD_AND_RETRY && card != start_save_cards().end()){
            checkpoint.saves_in = card->second;
        }
    }
    return table;
}

} // namespace


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = make_table();
    return checkpoints;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

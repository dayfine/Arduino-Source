/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
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
constexpr Direction WEST = Direction::WEST;

TilePos world(const char* map, const TilePos& local){
    return story_maps().to_world(map, local);
}

//  A pose on any of `tiles`, world tiles of story_maps().
Pose on(const char* map, std::vector<TilePos> tiles, std::optional<Direction> facing = std::nullopt){
    return Pokemon::tile_pose(map, std::move(tiles), facing);
}

Pose save_spot(){
    return on(MAP_ROUTE_202, {world(MAP_ROUTE_202, Route202::SAVE_SPOT)});
}

//  The catch grass training walks into: the tall grass of the training area.
std::vector<TilePos> catch_grass(){
    std::vector<TilePos> grass = story_after_training_area();
    std::erase_if(grass, [](const TilePos& pos){
        return story_maps().world().at(pos) != Pokemon::TileType::TALL_GRASS;
    });
    return grass;
}

//  Route 202's open rows at the top: north from any of them is Jubilife.
std::vector<TilePos> jubilife_exit(){
    std::vector<TilePos> exit;
    const Pokemon::TileGrid& grid = Route202::grid();
    for (int y = 0; y <= 3; y++){
        for (int x = 0; x < grid.width(); x++){
            const TilePos pos = world(MAP_ROUTE_202, {x, y});
            if (story_maps().world().standable(pos) && story_at_jubilife_exit(pos)){
                exit.push_back(pos);
            }
        }
    }
    return exit;
}


StoryStep make(StoryStepKind kind, std::string name, std::string why){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    return step;
}

//  A step that calls the old function named `name` (the segment's variant).
StoryStep call(std::string name, std::string why, std::optional<Pose> pose_after = std::nullopt){
    StoryStep step = make(StoryStepKind::GAME, name, std::move(why));
    step.variant = std::move(name);
    step.pose_after = std::move(pose_after);
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

StoryStep train(size_t battles, Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::TRAIN, "train", std::move(why));
    step.battles = battles;
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep heal(std::string name, Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::HEAL, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep with_variant(StoryStep step, std::string variant){
    step.variant = std::move(variant);
    return step;
}

StoryRow row(std::string id, std::string title, std::optional<Pose> start, std::vector<StoryStep> steps,
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY
){
    StoryRow ret;
    ret.id = std::move(id);
    ret.title = std::move(title);
    ret.start = std::move(start);
    ret.steps = std::move(steps);
    ret.on_failure = on_failure;
    return ret;
}


std::vector<StoryRow> make_segment_00(){
    const int to_center_door = SandgemTown::POKEMON_CENTER_SIGN.x - SandgemTown::POKEMON_CENTER_DOOR.x;
    const Pose center_door_front = on(MAP_SANDGEM_TOWN, story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR));
    StoryStep potions = make(StoryStepKind::SHOP, "potions",
        "Potions before Route 202's trainers: the Poke Mart lady's 10 (she stands still, as in the decomp), the clerk's 5 after two misses");
    potions.items = {{.item = "potion", .quantity = 10}};
    potions.variant = "sandgem_potions";
    potions.pose_after = save_spot();
    StoryStep stairs = make(StoryStepKind::STEP, "into_jubilife",
        "Jubilife's south edge is a wall with stairs in it, west of where the route may leave us (r78)");
    stairs.direction = NORTH;
    stairs.variant = "jubilife_stairs";

    return {
        row("00.00", "Verify Settings", std::nullopt, {
            call("verify_settings", "before any route movement or checkpoint save: fail visibly if options cannot be read"),
        }, StoryFailure::STOP),
        row("00.01", "After TV", std::nullopt, {
            call("tutorial_part_1_navigation", "intro and starter selection: Butters' recorded stick holds to the lake"),
        }),
        row("00.02", "Outside Verity before starter", std::nullopt, {
            call("starter_nav", "into the lake's scene"),
            call("select_starter", "the starter from the program's option"),
            with_variant(make(StoryStepKind::BATTLE, "fight_starly", "the Starly battle"), "fight_starly"),
            with_variant(make(StoryStepKind::MASH, "post_starly_cleanup", "the scene after the battle"), "post_starly_cleanup"),
        }),
        row("00.03", "Outside Verity after starter", std::nullopt, {
            call("tutorial_part_2_navigation", "Pokedex acquisition: back through Route 201 to Rowan's lab"),
        }),
        row("00.04", "After Pokedex, to the catch", std::nullopt, {
            call("tutorial_part_3_navigation", "out of the lab, home to Mom, and the catch on Route 202",
                on(MAP_ROUTE_202, story_catch_save_area())),
        }),
        //  A Lv5 starter fainted to Route 202's trainers (r72): training, a
        //  heal, and Potions first, each row from the save the one before ends on.
        row("00.04b", "After the catch: train and heal in Sandgem", on(MAP_ROUTE_202, story_catch_save_area()), {
            route("to_catch_grass", on(MAP_ROUTE_202, catch_grass()), "into the training column's grass"),
            train(3, on(MAP_ROUTE_202, story_after_training_area()), "three wild battles with the best move"),
            //  One walk round the houses ended two tiles short (r252): down
            //  the road first, then the town plan.
            route("into_sandgem", on(MAP_SANDGEM_TOWN, {world(MAP_SANDGEM_TOWN, SandgemTown::ROUTE_202_ENTRY_STOP)}),
                "south down the road to above the Mart", "robust"),
            route("to_center_sign", on(MAP_SANDGEM_TOWN, {world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Direction::SOUTH))}),
                "below the Center's sign, its doorway in sight", "town"),
            //  From below the sign the finder read the signs' panels as
            //  doorways (r252, r255): taps instead. One tile short is fine.
            legs("below_center_door", {{.direction = WEST, .tiles = to_center_door}}, "taps west below the Center's door", "one_short_ok"),
            heal("heal_sandgem", center_door_front, "Sandgem Town Pokemon Center"),
        }),
        row("00.04c", "Potions, back to Route 202's save spot", center_door_front, {
            potions,
        }),
        //  From the one save spot: from both at once the planner gives up.
        row("00.04d", "Route 202's trainers, Dawn in Jubilife", save_spot(), {
            route("north_to_jubilife", on(MAP_ROUTE_202, jubilife_exit()), "north past Route 202's trainers, fighting who sees us"),
            stairs,
            make(StoryStepKind::MASH, "dawn_scene", "Dawn walks over while we stand still"),
        }),
        row("00.05", "Jubilife After Dawn", std::nullopt, {
            call("tutorial_part_4_navigation", "the Town Map, the president, the coupons, the Poketch and the Center's PC"),
        }),
    };
}

std::vector<StoryRow> make_segment_01(){
    StoryStep save = make(StoryStepKind::SAVE, "save_here", "the next checkpoint starts here");
    return {
        row("01.01", "Jubilife to Barry", std::nullopt, {
            call("jubilife_to_barry", "from the Center's PC out to Barry on Route 203"),
        }),
        row("01.02", "Route 203 to Oreburgh", std::nullopt, {
            call("route203_to_oreburgh", "east through the Oreburgh Gate into Oreburgh City"),
        }),
        row("01.03", "Oreburgh Mine to Roark", std::nullopt, {
            call("oreburgh_to_roark", "into the Mine to Roark and back to the Center"),
        }),
        row("01.04", "Oreburgh Gym", std::nullopt, {
            call("oreburgh_gym", "the Gym's trainers and Roark for the Coal Badge"),
        }),
        row("01.05", "Oreburgh to Route 203", std::nullopt, {
            call("oreburgh_to_route203", "heal, then west out of the Oreburgh Gate"),
        }),
        row("01.06", "Route 203 to Jubilife", std::nullopt, {
            call("route203_to_jubilife", "west along Route 203 into Jubilife"),
        }),
        row("01.07", "Jubilife through Rowan's scene", std::nullopt, {
            call("jubilife_to_rowan", "to the north street, Rowan's scene and the tag battle"),
        }),
        row("01.08", "Jubilife to the Ravaged Path", std::nullopt, {
            call("jubilife_to_ravaged_path", "up Route 204's south half into the Ravaged Path"),
        }),
        row("01.09", "Ravaged Path", std::nullopt, {
            call("ravaged_path", "through the cave with Rock Smash, out onto Route 204 North"),
        }),
        row("01.10", "Route 204 to Floaroma", std::nullopt, {
            call("route204_to_floaroma", "the human's line north into Floaroma"),
            save,
        }),
        row("01.11", "Floaroma's old couple", std::nullopt, {
            call("floaroma_old_couple", "the old woman and the old man in the flower field"),
            save,
        }),
    };
}

}


const std::vector<StoryRow>& segment_00_table(){
    static const std::vector<StoryRow> table = make_segment_00();
    return table;
}

const std::vector<StoryRow>& segment_01_table(){
    static const std::vector<StoryRow> table = make_segment_01();
    return table;
}



}
}
}

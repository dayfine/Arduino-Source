/*  BDSP AutoStory - Segment 00 as story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
#include "PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_Segment00Steps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


const char* const STEP_LOCATE_SAVE = "locate_save";
const char* const STEP_SANDGEM_POTIONS = "sandgem_potions";
const char* const STEP_SIGN_GAP = "sign_gap";
const char* const STEP_JUBILIFE_STAIRS = "jubilife_stairs";


namespace{

//  Steps 2 to 8 are done once the Bag has Potions; 9 and 10 once a save
//  with them stands on Route 202.
const char* const HAS_POTIONS = "has_potions";
const char* const POTIONS_SAVED = "potions_saved";

Pose on(const char* map, std::vector<TilePos> world, std::optional<Direction> facing = std::nullopt){
    return Pokemon::tile_pose(map, std::move(world), facing);
}

TilePos world(const char* map, const TilePos& local){
    return story_maps().to_world(map, local);
}

//  The catch grass the training walks to: tall grass in the training area.
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

StoryStep make(StoryStepKind kind, std::string name, std::string why, std::vector<std::string> skip_if = {}){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    step.skip_if = std::move(skip_if);
    return step;
}

StoryStep with_variant(StoryStep step, std::string variant){
    step.variant = std::move(variant);
    return step;
}

StoryStep with_target(StoryStep step, Pose target){
    step.target = std::move(target);
    return step;
}

StoryStep with_pose_after(StoryStep step, Pose pose){
    step.pose_after = std::move(pose);
    return step;
}

Pokemon::StoryCheckpoint make_after_catch(){
    const TilePos save_spot = world(MAP_ROUTE_202, Route202::SAVE_SPOT);
    const TilePos below_sign = world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Direction::SOUTH));
    const int to_door = SandgemTown::POKEMON_CENTER_SIGN.x - SandgemTown::POKEMON_CENTER_DOOR.x;

    StoryStep train = make(StoryStepKind::TRAIN, "train", "a Lv5 starter fainted to Route 202's trainers (r72)", {HAS_POTIONS});
    train.battles = 3;
    StoryStep below_door = make(StoryStepKind::LEGS, "below_center_door",
        "taps west below the Center's door; the finder read the signs as doorways from below the sign (r252, r255)", {HAS_POTIONS});
    below_door.legs = {{.direction = Direction::WEST, .tiles = to_door}};
    StoryStep potions = make(StoryStepKind::SHOP, "potions",
        "Potions before Route 202's trainers; the lady's 10 are free", {HAS_POTIONS});
    potions.items = {{.item = "potion", .quantity = 10}};
    StoryStep stairs = make(StoryStepKind::STEP, "into_jubilife",
        "Jubilife's south edge is a wall with stairs in it, west of where the route may leave us (r78)");
    stairs.direction = Direction::NORTH;

    return Pokemon::StoryCheckpoint{
        .id = "00.04b",
        .title = "After the catch, to Dawn in Jubilife",
        .start = on(MAP_ROUTE_202, story_catch_save_area()),
        .steps = {
            with_variant(make(StoryStepKind::GAME, "locate_save", "a retry may load the save made with the Potions"), STEP_LOCATE_SAVE),
            with_target(make(StoryStepKind::ROUTE, "to_catch_grass", "into the catch grass to train", {HAS_POTIONS}),
                on(MAP_ROUTE_202, catch_grass())),
            with_pose_after(std::move(train), on(MAP_ROUTE_202, story_after_training_area())),
            //  One walk round the houses ended two tiles short (r252): down
            //  the road first, then the town plan.
            with_variant(with_target(make(StoryStepKind::ROUTE, "into_sandgem", "south down the road to above the Mart", {HAS_POTIONS}),
                on(MAP_SANDGEM_TOWN, {world(MAP_SANDGEM_TOWN, SandgemTown::ROUTE_202_ENTRY_STOP)})), "robust"),
            with_variant(with_target(make(StoryStepKind::ROUTE, "to_center_sign", "below the Center's sign, its doorway in sight", {HAS_POTIONS}),
                on(MAP_SANDGEM_TOWN, {below_sign}, Direction::NORTH)), "town"),
            with_variant(std::move(below_door), "one_short_ok"),
            with_pose_after(make(StoryStepKind::HEAL, "heal_sandgem", "Sandgem Town Pokemon Center", {HAS_POTIONS}),
                on(MAP_SANDGEM_TOWN, story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR))),
            with_variant(std::move(potions), STEP_SANDGEM_POTIONS),
            with_pose_after(with_variant(make(StoryStepKind::GAME, "back_to_route_202",
                "to the save spot under the ledge, where a retry may start; the trainers are north of it", {POTIONS_SAVED}), STEP_SIGN_GAP),
                on(MAP_ROUTE_202, {save_spot})),
            with_target(make(StoryStepKind::SAVE, "save_route_202",
                "with the Potions, at the save spot on Route 202 (r124a saved beside the Mart)", {POTIONS_SAVED}),
                on(MAP_ROUTE_202, {save_spot})),
            //  From the one save spot: from both at once the planner gives up.
            with_target(make(StoryStepKind::ROUTE, "north_to_jubilife", "past Route 202's trainers, fighting who sees us"),
                on(MAP_ROUTE_202, jubilife_exit())),
            with_variant(std::move(stairs), STEP_JUBILIFE_STAIRS),
            make(StoryStepKind::MASH, "dawn_scene", "Dawn walks over while we stand still"),
        },
    };
}

}


const Pokemon::StoryCheckpoint& segment_00_after_catch(){
    static const Pokemon::StoryCheckpoint checkpoint = make_after_catch();
    return checkpoint;
}



}
}
}

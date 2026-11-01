/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr Direction NORTH = Direction::NORTH;
constexpr Direction SOUTH = Direction::SOUTH;
constexpr Direction EAST = Direction::EAST;
constexpr Direction WEST = Direction::WEST;

const char* const BEDROOM = "PalletTown_PlayersHouse_2F";
const char* const PALLET = "PalletTown";
const char* const LAB = "PalletTown_ProfessorOaksLab";
const char* const VIRIDIAN = "ViridianCity";
const char* const VIRIDIAN_MART = "ViridianCity_Mart";
const char* const VIRIDIAN_CENTER = "ViridianCity_PokemonCenter_1F";
const char* const FOREST = "ViridianForest";
const char* const FOREST_GATE = "Route2_ViridianForest_NorthEntrance";
const char* const PEWTER_CENTER = "PewterCity_PokemonCenter_1F";
const char* const GYM = "PewterCity_Gym";

StoryPose at(const char* map, int x, int y, std::optional<Direction> facing = std::nullopt){
    return StoryPose{map, {x, y}, facing};
}

StoryStep make(StoryStepKind kind, std::string name, std::string why){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    return step;
}

StoryStep route(std::string name, StoryPose target, std::string why, bool fight_wild = false){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name), std::move(why));
    step.target = std::move(target);
    step.fight_wild = fight_wild;
    return step;
}

StoryStep step_into(std::string name, Direction direction, std::string why){
    StoryStep step = make(StoryStepKind::STEP, std::move(name), std::move(why));
    step.direction = direction;
    return step;
}

StoryStep answer(std::string name, bool yes, std::string why){
    StoryStep step = make(StoryStepKind::ANSWER, std::move(name), std::move(why));
    step.yes = yes;
    return step;
}

StoryStep scene(std::string name, std::optional<StoryPose> pose_after, std::string why){
    StoryStep step = make(StoryStepKind::MASH, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep battle(std::string name, StoryPose pose_after, bool may_lose, std::string why){
    StoryStep step = make(StoryStepKind::BATTLE, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    step.may_lose = may_lose;
    return step;
}

StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, bool fight_wild = false){
    StoryStep step = make(StoryStepKind::LEGS, std::move(name), std::move(why));
    step.legs = std::move(legs);
    step.fight_wild = fight_wild;
    return step;
}

StoryStep new_game_intro(StoryPose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::NEW_GAME, "intro", std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}

StoryStep ready(std::string name, StoryPose nurse, std::string why){
    StoryStep step = make(StoryStepKind::READY, std::move(name), std::move(why));
    step.target = std::move(nurse);
    return step;
}

StoryStep heal(std::string name){
    return make(StoryStepKind::HEAL, std::move(name), "full HP");
}


StoryCheckpoint new_game(){
    return StoryCheckpoint{"00.01", "new game to the bedroom", std::nullopt, {}, StartMenu::BEFORE_POKEMON, {
        //  new_game.c: the bedroom at (6, 6), facing the TV.
        new_game_intro(at(BEDROOM, 6, 6, NORTH), "Oak's speech, BOY, default names, rival GREEN"),
        make(StoryStepKind::SET_OPTIONS, "options", "fast text; no switch prompt between a trainer's Pokemon"),
    }};
}

StoryCheckpoint leave_house(){
    return StoryCheckpoint{"00.02", "bedroom to Pallet Town", at(BEDROOM, 6, 6, NORTH), {}, StartMenu::BEFORE_POKEMON, {
        route("front_door", at(PALLET, 6, 8), "down the stairs and out of the house"),
    }};
}

StoryCheckpoint oaks_trigger(){
    return StoryCheckpoint{"00.03", "Oak stops the player and walks them to the lab", at(PALLET, 6, 8, SOUTH), {}, StartMenu::BEFORE_POKEMON, {
        route("below_oaks_trigger", at(PALLET, 12, 2, NORTH), "below the north exit, short of Oak's trigger at (12, 1)"),
        step_into("oaks_trigger", NORTH, "the trigger: \"Don't go out!\""),
        //  PlayerEnter walks the player up 8 tiles from the lab's door.
        scene("to_the_lab", at(LAB, 6, 4, NORTH), "Oak walks the player into the lab; the scene ends at the table"),
    }};
}

StoryCheckpoint starter(){
    return StoryCheckpoint{"00.04", "Squirtle, then the rival's battle", at(LAB, 6, 4, NORTH), {}, StartMenu::BEFORE_POKEDEX, {
        route("below_squirtle", at(LAB, 9, 5, NORTH), "under Squirtle's ball (9, 4)"),
        talk("squirtle_ball", "\"So, you want the WATER POKEMON SQUIRTLE?\""),
        answer("choose_squirtle", true, "take Squirtle"),
        answer("no_nickname", false, "\"give a nickname?\" NO"),
        scene("rival_takes_bulbasaur", at(LAB, 9, 5, NORTH), "the rival picks his ball; the player still faces the table (console B, 2026-11-01)"),
        route("above_rival_trigger", at(LAB, 7, 7, SOUTH), "short of the rival's trigger row 8"),
        step_into("rival_trigger", SOUTH, "\"Wait, let's check out our POKEMON!\""),
        //  The rival leaves; PlayerWatchRivalExitAfterBattleRight ends facing down.
        battle("rival_battle", at(LAB, 7, 8, SOUTH), true, "the tutorial battle; losing goes on too"),
    }};
}

StoryCheckpoint parcel(){
    return StoryCheckpoint{"00.05", "Route 1 to Viridian Mart: Oak's Parcel", at(LAB, 7, 8, SOUTH), {}, StartMenu::BEFORE_POKEDEX, {
        route("below_mart_door", at(VIRIDIAN, 36, 20, NORTH), "Route 1, running from wild battles"),
        step_into("mart_door", NORTH, "the clerk's scene starts on entering"),
        //  ApproachCounter: up 4 from the mat, then faces the clerk.
        scene("parcel_from_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "the clerk gives OAK'S PARCEL"),
    }};
}

StoryCheckpoint pokedex(){
    return StoryCheckpoint{"00.06", "Oak's Parcel to the Pokedex", at(VIRIDIAN_MART, 4, 3, WEST), {}, StartMenu::FULL, {
        route("below_oak", at(LAB, 6, 4, NORTH), "back down Route 1 to Oak"),
        talk("give_parcel", "Oak takes the parcel"),
        scene("pokedex_scene", at(LAB, 6, 4, SOUTH), "the rival comes in, Pokedexes from the desk; the player ends facing south (console B, 2026-11-01)"),
    }};
}

StoryCheckpoint forest(){
    return StoryCheckpoint{"00.07", "Viridian shopping to Pewter Center", at(LAB, 6, 4, SOUTH), {}, StartMenu::FULL, {
        route("mart_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "Route 1 again"),
        make(StoryStepKind::SHOP, "potions_and_antidotes", "4 Potions, 2 Antidotes"),
        route("viridian_nurse", at(VIRIDIAN_CENTER, 7, 4, NORTH), "heal before the forest"),
        heal("heal_viridian"),
        route("below_old_man_trigger", at(VIRIDIAN, 20, 9, NORTH), "short of the tutorial trigger (20, 8)"),
        step_into("old_man_trigger", NORTH, "the old man's catching tutorial can't be walked around"),
        //  TutorialTriggerLeft turns the player right, to the old man.
        scene("old_man_tutorial", at(VIRIDIAN, 20, 8, EAST), "about 60 lines and his demo battle; TEACHY TV"),
        //  After his demo the old man stands east of the player, beside (20, 8), where
        //  the map doesn't have him: "east 2" from there didn't move, and the walk north
        //  ended under Route 2's ledge in column 8 (console B, 2026-11-01). Go north first.
        route("past_old_man", at(VIRIDIAN, 20, 5, NORTH), "north along column 20, clear of the old man"),
        route("forest_entrance", at(FOREST, 29, 61), "Route 2, running from wild battles"),
        //  The forest legs walked by hand; wild battles are fought for EXP.
        route("forest_west", at(FOREST, 13, 46), "west side of the forest", true),
        route("forest_east", at(FOREST, 42, 44), "east path, past a Bug Catcher", true),
        route("forest_north", at(FOREST, 39, 25), "north part", true),
        //  To the gate by legs that end on walls, which the walker holds again
        //  after a battle instead of estimating where it stopped: planned
        //  routes with open legs lost the player in the north of the forest
        //  (console B, 2026-11-01). The two 1-tile legs are single steps.
        legs("forest_exit", {
            {NORTH, 10}, {WEST, 12}, {SOUTH, 10}, {WEST, 6}, {NORTH, 20},
            {SOUTH, 1}, {WEST, 10}, {SOUTH, 27}, {WEST, 8}, {NORTH, 20}, {EAST, 1},
        }, "walls all the way to the gate's column", true),
        route("forest_gate", at(FOREST_GATE, 7, 10, NORTH), "north onto the exit, arriving on the gate's mat", true),
        //  Out of the forest, wild battles are run from; trainers are still fought.
        route("pewter_nurse", at(PEWTER_CENTER, 7, 4, NORTH), "Route 2 north to Pewter's Center"),
        heal("heal_pewter"),
    }};
}

StoryCheckpoint brock(){
    //  The by-hand save: 2 Potions and 1 Antidote left after the forest.
    return StoryCheckpoint{"00.08", "Pewter Gym: Camper Liam, then Brock", at(PEWTER_CENTER, 7, 4, NORTH), {2, 1}, StartMenu::FULL, {
        //  Camper Liam (3, 8) always faces east (MOVEMENT_TYPE_FACE_RIGHT, sight
        //  4) and sees (4..7, 8): walking north up column 6 from the door,
        //  the step onto (6, 8) starts his battle. He walks up to the player,
        //  who stays put and keeps facing north (FRLG doesn't turn the player
        //  to a trainer). That step is taken on its own, so no leg is cut.
        route("below_liam_line", at(GYM, 6, 9, NORTH), "in by the door, up column 6 to below Liam's row"),
        step_into("liam_line", NORTH, "onto (6, 8), in Liam's line"),
        battle("camper_liam", at(GYM, 6, 8, NORTH), false, "Geodude and Sandshrew; EXP before Brock"),
        //  On to Brock unless the starter's HP is red with no Potion left.
        ready("ready_for_brock", at(PEWTER_CENTER, 7, 4, NORTH), "a Potion if hurt; the nurse only if red with none"),
        route("before_brock", at(GYM, 6, 6, NORTH), "below Brock (6, 5); Liam, beaten, stands at (5, 8)"),
        talk("challenge_brock", "Brock's speech"),
        battle("brock", at(GYM, 6, 6, NORTH), false, "Geodude and Onix, with Bubble; Boulder Badge and TM39"),
    }};
}

}


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints{
        new_game(), leave_house(), oaks_trigger(), starter(), parcel(), pokedex(), forest(), brock(),
    };
    return checkpoints;
}



}
}
}

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


StoryCheckpoint new_game(){
    StoryCheckpoint cp{"00.01", "new game to the bedroom", std::nullopt, {}, StartMenu::BEFORE_POKEMON, {}};
    StoryStep intro = make(StoryStepKind::NEW_GAME, "intro", "Oak's speech, BOY, default names, rival GREEN");
    intro.pose_after = at(BEDROOM, 6, 6, NORTH);   //  new_game.c: the bedroom at (6, 6), facing the TV
    cp.steps.push_back(intro);
    cp.steps.push_back(make(StoryStepKind::SET_OPTIONS, "options", "fast text; no switch prompt between a trainer's Pokemon"));
    return cp;
}

StoryCheckpoint leave_house(){
    StoryCheckpoint cp{"00.02", "bedroom to Pallet Town", at(BEDROOM, 6, 6, NORTH), {}, StartMenu::BEFORE_POKEMON, {}};
    cp.steps.push_back(route("front_door", at(PALLET, 6, 8), "down the stairs and out of the house"));
    return cp;
}

StoryCheckpoint oaks_trigger(){
    StoryCheckpoint cp{"00.03", "Oak stops the player and walks them to the lab", at(PALLET, 6, 8, SOUTH), {}, StartMenu::BEFORE_POKEMON, {}};
    cp.steps.push_back(route("below_oaks_trigger", at(PALLET, 12, 2, NORTH), "below the north exit, short of Oak's trigger at (12, 1)"));
    cp.steps.push_back(step_into("oaks_trigger", NORTH, "the trigger: \"Don't go out!\""));
    //  PlayerEnter walks the player up 8 tiles from the lab's door.
    cp.steps.push_back(scene("to_the_lab", at(LAB, 6, 4, NORTH), "Oak walks the player into the lab; the scene ends at the table"));
    return cp;
}

StoryCheckpoint starter(){
    StoryCheckpoint cp{"00.04", "Squirtle, then the rival's battle", at(LAB, 6, 4, NORTH), {}, StartMenu::BEFORE_POKEDEX, {}};
    cp.steps.push_back(route("below_squirtle", at(LAB, 9, 5, NORTH), "under Squirtle's ball (9, 4)"));
    cp.steps.push_back(make(StoryStepKind::TALK, "squirtle_ball", "\"So, you want the WATER POKEMON SQUIRTLE?\""));
    cp.steps.push_back(answer("choose_squirtle", true, "take Squirtle"));
    cp.steps.push_back(answer("no_nickname", false, "\"give a nickname?\" NO"));
    cp.steps.push_back(scene("rival_takes_bulbasaur", at(LAB, 9, 5, NORTH), "the rival picks his ball; the player still faces the table (console B, 2026-11-01)"));
    cp.steps.push_back(route("above_rival_trigger", at(LAB, 7, 7, SOUTH), "short of the rival's trigger row 8"));
    cp.steps.push_back(step_into("rival_trigger", SOUTH, "\"Wait, let's check out our POKEMON!\""));
    //  The rival leaves; PlayerWatchRivalExitAfterBattleRight ends facing down.
    cp.steps.push_back(battle("rival_battle", at(LAB, 7, 8, SOUTH), true, "the tutorial battle; losing goes on too"));
    return cp;
}

StoryCheckpoint parcel(){
    StoryCheckpoint cp{"00.05", "Route 1 to Viridian Mart: Oak's Parcel", at(LAB, 7, 8, SOUTH), {}, StartMenu::BEFORE_POKEDEX, {}};
    cp.steps.push_back(route("below_mart_door", at(VIRIDIAN, 36, 20, NORTH), "Route 1, running from wild battles"));
    cp.steps.push_back(step_into("mart_door", NORTH, "the clerk's scene starts on entering"));
    //  ApproachCounter: up 4 from the mat, then faces the clerk.
    cp.steps.push_back(scene("parcel_from_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "the clerk gives OAK'S PARCEL"));
    return cp;
}

StoryCheckpoint pokedex(){
    StoryCheckpoint cp{"00.06", "Oak's Parcel to the Pokedex", at(VIRIDIAN_MART, 4, 3, WEST), {}, StartMenu::FULL, {}};
    cp.steps.push_back(route("below_oak", at(LAB, 6, 4, NORTH), "back down Route 1 to Oak"));
    cp.steps.push_back(make(StoryStepKind::TALK, "give_parcel", "Oak takes the parcel"));
    cp.steps.push_back(scene("pokedex_scene", at(LAB, 6, 4, SOUTH), "the rival comes in, Pokedexes from the desk; the player ends facing south (console B, 2026-11-01)"));
    return cp;
}

StoryCheckpoint forest(){
    StoryCheckpoint cp{"00.07", "Viridian shopping to Pewter Center", at(LAB, 6, 4, SOUTH), {}, StartMenu::FULL, {}};
    cp.steps.push_back(route("mart_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "Route 1 again"));
    cp.steps.push_back(make(StoryStepKind::SHOP, "potions_and_antidotes", "4 Potions, 2 Antidotes"));
    cp.steps.push_back(route("viridian_nurse", at(VIRIDIAN_CENTER, 7, 4, NORTH), "heal before the forest"));
    cp.steps.push_back(make(StoryStepKind::HEAL, "heal_viridian", "full HP"));
    cp.steps.push_back(route("below_old_man_trigger", at(VIRIDIAN, 20, 9, NORTH), "short of the tutorial trigger (20, 8)"));
    cp.steps.push_back(step_into("old_man_trigger", NORTH, "the old man's catching tutorial can't be walked around"));
    //  TutorialTriggerLeft turns the player right, to the old man.
    cp.steps.push_back(scene("old_man_tutorial", at(VIRIDIAN, 20, 8, EAST), "about 60 lines and his demo battle; TEACHY TV"));
    cp.steps.push_back(route("forest_entrance", at(FOREST, 29, 61), "Route 2, running from wild battles"));
    //  The forest legs walked by hand; wild battles are fought for EXP.
    cp.steps.push_back(route("forest_west", at(FOREST, 13, 46), "west side of the forest", true));
    cp.steps.push_back(route("forest_east", at(FOREST, 42, 44), "east path, past a Bug Catcher", true));
    cp.steps.push_back(route("forest_north", at(FOREST, 39, 25), "north part", true));
    cp.steps.push_back(route("pewter_nurse", at(PEWTER_CENTER, 7, 4, NORTH), "out of the forest to Pewter's Center", true));
    cp.steps.push_back(make(StoryStepKind::HEAL, "heal_pewter", "full HP"));
    return cp;
}

StoryCheckpoint brock(){
    //  The by-hand save: 2 Potions and 1 Antidote left after the forest.
    StoryCheckpoint cp{"00.08", "Pewter Gym: Camper Liam, then Brock", at(PEWTER_CENTER, 7, 4, NORTH), {2, 1}, StartMenu::FULL, {}};
    cp.steps.push_back(route("past_camper_liam", at(GYM, 6, 6, NORTH), "Liam (3, 8) stops the player at (6, 8): fought on the way"));
    cp.steps.push_back(route("back_to_nurse", at(PEWTER_CENTER, 7, 4, NORTH), "heal after Liam"));
    cp.steps.push_back(make(StoryStepKind::HEAL, "heal_before_brock", "full HP"));
    cp.steps.push_back(route("before_brock", at(GYM, 6, 6, NORTH), "below Brock (6, 5)"));
    cp.steps.push_back(make(StoryStepKind::TALK, "challenge_brock", "Brock's speech"));
    cp.steps.push_back(battle("brock", at(GYM, 6, 6, NORTH), false, "Geodude and Onix, with Bubble; Boulder Badge and TM39"));
    return cp;
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

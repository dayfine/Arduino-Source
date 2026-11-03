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
const char* const PEWTER = "PewterCity";
const char* const PEWTER_CENTER = "PewterCity_PokemonCenter_1F";
const char* const GYM = "PewterCity_Gym";
const char* const ROUTE3 = "Route3";
const char* const ROUTE4_CENTER = "Route4_PokemonCenter_1F";

Pokemon::Pose at(const char* map, int x, int y, std::optional<Direction> facing = std::nullopt){
    return to_pose(StoryPose{map, {x, y}, facing});
}

StoryCheckpoint row(
    std::string id, std::string title, std::optional<Pokemon::Pose> start,
    Supplies supplies, StartMenu menu, std::vector<StoryStep> steps
){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = std::move(title);
    checkpoint.start = std::move(start);
    checkpoint.steps = std::move(steps);
    checkpoint.supplies = supplies;
    checkpoint.menu = menu;
    return checkpoint;
}

//  What a walk does about wild Pokemon: runs, or fights them for EXP.
enum class WildBattles{
    RUN,
    FIGHT,
};

StoryStep make(StoryStepKind kind, std::string name, std::string why){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    return step;
}

StoryStep route(std::string name, Pokemon::Pose target, std::string why, WildBattles wild = WildBattles::RUN){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name), std::move(why));
    step.target = std::move(target);
    step.fight_wild = wild == WildBattles::FIGHT;
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

StoryStep scene(std::string name, std::optional<Pokemon::Pose> pose_after, std::string why){
    StoryStep step = make(StoryStepKind::MASH, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep battle(std::string name, Pokemon::Pose pose_after, bool may_lose, std::string why){
    StoryStep step = make(StoryStepKind::BATTLE, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    step.may_lose = may_lose;
    return step;
}

StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, WildBattles wild = WildBattles::RUN){
    StoryStep step = make(StoryStepKind::LEGS, std::move(name), std::move(why));
    step.legs = std::move(legs);
    step.fight_wild = wild == WildBattles::FIGHT;
    return step;
}

//  `step`, a ROUTE or LEGS, run with B held: from the Running Shoes on.
//  Indoors, where the game doesn't let the player run, it walks.
StoryStep running(StoryStep step){
    step.variant = RUN_VARIANT;
    return step;
}

StoryStep new_game_intro(Pokemon::Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::NEW_GAME, "intro", std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}

StoryStep ready(std::string name, Pokemon::Pose nurse, std::string why){
    StoryStep step = make(StoryStepKind::READY, std::move(name), std::move(why));
    step.target = std::move(nurse);
    return step;
}

StoryStep heal(std::string name){
    return make(StoryStepKind::HEAL, std::move(name), "full HP");
}

StoryStep shop(std::string name, std::vector<Pokemon::StoryItem> items, std::string why){
    StoryStep step = make(StoryStepKind::SHOP, std::move(name), std::move(why));
    step.items = std::move(items);
    return step;
}


StoryStep game_step(std::string variant, Pokemon::Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::GAME, variant, std::move(why));
    step.variant = std::move(variant);
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep options_step(){
    return make(StoryStepKind::OPTIONS, "options", "fast text; no switch prompt between a trainer's Pokemon");
}


StoryCheckpoint new_game(){
    StoryCheckpoint checkpoint = row("00.01", "new game to the bedroom", std::nullopt, {}, StartMenu::BEFORE_POKEMON, {
        //  new_game.c: the bedroom at (6, 6), facing the TV.
        new_game_intro(at(BEDROOM, 6, 6, NORTH), "Oak's speech, BOY, the player's name (default if none is set), rival GREEN"),
        options_step(),
    });
    checkpoint.handoff = StoryHandoff::SID_HELPER;
    return checkpoint;
}

//  SID Helper confirms the name, finishes the intro and reads the Trainer
//  Card from the START menu, without moving the player or saving.
StoryCheckpoint after_sid_helper(){
    StoryCheckpoint checkpoint = row("00.01b", "after SID Helper: close the Trainer Card, save in the bedroom", at(BEDROOM, 6, 6, NORTH), {}, StartMenu::BEFORE_POKEMON, {
        game_step(CLOSE_TRAINER_CARD, at(BEDROOM, 6, 6, NORTH), "the Trainer Card SID Helper left open, or HOME"),
        options_step(),
    });
    checkpoint.live_start = true;
    return checkpoint;
}

StoryCheckpoint leave_house(){
    return row("00.02", "bedroom to Pallet Town", at(BEDROOM, 6, 6, NORTH), {}, StartMenu::BEFORE_POKEMON, {
        route("front_door", at(PALLET, 6, 8), "down the stairs and out of the house"),
    });
}

StoryCheckpoint oaks_trigger(){
    return row("00.03", "Oak stops the player and walks them to the lab", at(PALLET, 6, 8, SOUTH), {}, StartMenu::BEFORE_POKEMON, {
        route("below_oaks_trigger", at(PALLET, 12, 2, NORTH), "below the north exit, short of Oak's trigger at (12, 1)"),
        step_into("oaks_trigger", NORTH, "the trigger: \"Don't go out!\""),
        //  PlayerEnter walks the player up 8 tiles from the lab's door.
        scene("to_the_lab", at(LAB, 6, 4, NORTH), "Oak walks the player into the lab; the scene ends at the table"),
    });
}

//  Starter RNG starts from a save south of the table, facing the ball.
StoryCheckpoint to_starter_ball(){
    StoryCheckpoint checkpoint = row("00.04", "to Squirtle's ball, south of the table", at(LAB, 6, 4, NORTH), {}, StartMenu::BEFORE_POKEMON, {
        route("below_squirtle", at(LAB, 9, 5, NORTH), "under Squirtle's ball (9, 4)"),
    });
    checkpoint.handoff = StoryHandoff::STARTER_RNG;
    return checkpoint;
}

StoryCheckpoint starter(){
    return row("00.04b", "Squirtle, then the rival's battle", at(LAB, 9, 5, NORTH), {}, StartMenu::BEFORE_POKEDEX, {
        talk("squirtle_ball", "\"So, you want the WATER POKEMON SQUIRTLE?\""),
        answer("choose_squirtle", true, "take Squirtle"),
        answer("no_nickname", false, "\"give a nickname?\" NO"),
        scene("rival_takes_bulbasaur", at(LAB, 9, 5, NORTH), "the rival picks his ball; the player still faces the table (console B, 2026-11-01)"),
        route("above_rival_trigger", at(LAB, 7, 7, SOUTH), "short of the rival's trigger row 8"),
        step_into("rival_trigger", SOUTH, "\"Wait, let's check out our POKEMON!\""),
        //  The rival leaves; PlayerWatchRivalExitAfterBattleRight ends facing down.
        battle("rival_battle", at(LAB, 7, 8, SOUTH), true, "the tutorial battle; losing goes on too"),
    });
}

StoryCheckpoint parcel(){
    return row("00.05", "Route 1 to Viridian Mart: Oak's Parcel", at(LAB, 7, 8, SOUTH), {}, StartMenu::BEFORE_POKEDEX, {
        route("below_mart_door", at(VIRIDIAN, 36, 20, NORTH), "Route 1, running from wild battles"),
        step_into("mart_door", NORTH, "the clerk's scene starts on entering"),
        //  ApproachCounter: up 4 from the mat, then faces the clerk.
        scene("parcel_from_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "the clerk gives OAK'S PARCEL"),
    });
}

StoryCheckpoint pokedex(){
    return row("00.06", "Oak's Parcel to the Pokedex", at(VIRIDIAN_MART, 4, 3, WEST), {}, StartMenu::FULL, {
        route("below_oak", at(LAB, 6, 4, NORTH), "back down Route 1 to Oak"),
        talk("give_parcel", "Oak takes the parcel"),
        scene("pokedex_scene", at(LAB, 6, 4, SOUTH), "the rival comes in, Pokedexes from the desk; the player ends facing south (console B, 2026-11-01)"),
    });
}

StoryCheckpoint shopping(){
    return row("00.07", "Viridian shopping, then into Viridian Forest", at(LAB, 6, 4, SOUTH), {}, StartMenu::FULL, {
        route("mart_clerk", at(VIRIDIAN_MART, 4, 3, WEST), "Route 1 again"),
        //  2000 of the 3000 a new game starts with, plus the rival's prize.
        shop("potions_and_antidotes", {{"potion", 5}, {"antidote", 3}, {"parlyz_heal", 1}}, "5 Potions, 3 Antidotes, 1 Parlyz Heal"),
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
        //  The gate's north warp lands on the forest's exit mat (29, 62), facing
        //  north. The save there keeps a forest failure from going back to Pallet.
        route("forest_entrance", at(FOREST, 29, 62, NORTH), "Route 2 and the south gate, running from wild battles"),
    });
}

StoryCheckpoint forest(){
    //  The supplies bought in 00.07.
    return row("00.08", "Viridian Forest to Pewter Center", at(FOREST, 29, 62, NORTH), {5, 3, 1}, StartMenu::FULL, {
        //  Wild battles are fought for EXP. Straight up the east side from the
        //  entrance, each leg ending on a wall (the Boy at (45, 58) stops the
        //  east one). Rick (47, 45) sees (42..46, 45), so the last leg meets
        //  his battle and is held on to its wall after it. The by-hand run's
        //  detour west to (13, 46) and back is dropped.
        legs("forest_east", {{EAST, 1}, {NORTH, 4}, {EAST, 14}, {NORTH, 15}}, "up the east side to (44, 43)", WildBattles::FIGHT),
        route("forest_north", at(FOREST, 39, 25), "north part", WildBattles::FIGHT),
        //  To the gate by legs that end on walls, which the walker holds again
        //  after a battle instead of estimating where it stopped: planned
        //  routes with open legs lost the player in the north of the forest
        //  (console B, 2026-11-01). The two 1-tile legs are single steps.
        legs("forest_exit", {
            {NORTH, 10}, {WEST, 12}, {SOUTH, 10}, {WEST, 6}, {NORTH, 20},
            {SOUTH, 1}, {WEST, 10}, {SOUTH, 27}, {WEST, 8}, {NORTH, 20}, {EAST, 1},
        }, "walls all the way to the gate's column", WildBattles::FIGHT),
        route("forest_gate", at(FOREST_GATE, 7, 10, NORTH), "north onto the exit, arriving on the gate's mat", WildBattles::FIGHT),
        //  Out of the forest, wild battles are run from; trainers are still fought.
        route("pewter_nurse", at(PEWTER_CENTER, 7, 4, NORTH), "Route 2 north to Pewter's Center"),
        heal("heal_pewter"),
    });
}

StoryCheckpoint brock(){
    //  The by-hand save: 2 Potions and 1 Antidote left after the forest.
    return row("00.09", "Pewter Gym: Camper Liam, then Brock", at(PEWTER_CENTER, 7, 4, NORTH), {2, 1}, StartMenu::FULL, {
        //  Camper Liam (3, 8) always faces east (MOVEMENT_TYPE_FACE_RIGHT, sight
        //  4) and sees (4..7, 8): walking north up column 6 from the door,
        //  the step onto (6, 8) starts his battle. He walks up to the player,
        //  who stays put and keeps facing north (FRLG doesn't turn the player
        //  to a trainer). That step is taken on its own, so no leg is cut.
        route("below_liam_line", at(GYM, 6, 9, NORTH), "in by the door, up column 6 to below Liam's row"),
        step_into("liam_line", NORTH, "onto (6, 8), in Liam's line"),
        battle("camper_liam", at(GYM, 6, 8, NORTH), false, "Geodude and Sandshrew; EXP before Brock"),
        //  On to Brock unless red HP or a status is left that no item cures.
        ready("ready_for_brock", at(PEWTER_CENTER, 7, 4, NORTH), "items for a status or low HP; the nurse only if they can't"),
        route("before_brock", at(GYM, 6, 6, NORTH), "below Brock (6, 5); Liam, beaten, stands at (5, 8)"),
        talk("challenge_brock", "Brock's speech"),
        battle("brock", at(GYM, 6, 6, NORTH), false, "Geodude and Onix, with Bubble; Boulder Badge and TM39"),
    });
}

StoryCheckpoint running_shoes(){
    return row("00.10", "Pewter: the Running Shoes from Oak's aide", at(GYM, 6, 6, NORTH), {}, StartMenu::FULL, {
        //  Brock's win sets VAR_MAP_SCENE_PEWTER_CITY to 1 and shows the aide at
        //  (46, 20); his triggers are (46, 21..23) (PewterCity map.json). The
        //  Gym guide's triggers at (42..43, 21..23) need the scene at 0.
        route("before_aide_trigger", at(PEWTER, 45, 21, EAST), "out of the Gym, east along row 21, short of the top trigger"),
        step_into("aide_trigger", EAST, "onto (46, 21), below the aide"),
        //  RunningShoesAideTriggerTop: the aide talks from (46, 20), no YES/NO;
        //  the player only turns up. He walks off and is removed; FLAG_SYS_B_DASH
        //  is set (console B, 2026-11-02: about 20 B presses to the overworld).
        scene("shoes_from_aide", at(PEWTER, 46, 21, NORTH), "\"Oh, PLAYER!\" ... the Running Shoes; ends facing north"),
        //  Where the by-hand walk stopped to start 00.11 (console B, 2026-11-02).
        route("below_pewter_center", at(PEWTER, 17, 26, SOUTH), "west along row 21 and down to below the Pokemon Center's door (17, 25)"),
    });
}

//  Route 3's trainers (Route3 map.json): the way east meets three of them.
//  Bug Catcher Colton (12, 6) faces east, sight 3: the only way north past
//  the row 7 ledges is the gap at x 14..15, below his line. Youngster Ben
//  (17, 4) faces south, sight 2, so the way east keeps to row 3 past him.
//  Bug Catcher Greg (25, 4) faces south, sight 2, over the way east along
//  row 5. Bug Catcher James (32, 6) faces east, sight 4, over the way down
//  at x 35..36. The others are kept out of: Lass Janice (19, 9) sees west
//  along row 9 to x 14, Lass Sally (30, 3) west along row 3 to x 25, and
//  Youngster Calvin (29, 10) west along row 10. Each line is stepped into on
//  its own, so no leg is cut; the trainer walks up to the player, who stays
//  put facing the way they stepped.
StoryCheckpoint route3(){
    return row("00.11", "Pewter Center, then Route 3's trainers", at(PEWTER, 17, 26, SOUTH), {}, StartMenu::FULL, {
        running(route("pewter_nurse", at(PEWTER_CENTER, 7, 4, NORTH), "in by the door below the player")),
        heal("heal_pewter"),
        //  Up from row 9 west of Janice's line, which ends at x 14.
        running(route("west_of_janice_line", at(ROUTE3, 13, 8), "out of Pewter by row 21, east into Route 3, short of Janice's line")),
        running(legs("below_colton_line", {{EAST, 1}, {NORTH, 1}}, "to (14, 7), in the ledges' gap")),
        step_into("colton_line", NORTH, "onto (14, 6), in Colton's line"),
        battle("bug_catcher_colton", at(ROUTE3, 14, 6, NORTH), false, "his Bug Pokemon; Colton stops at (13, 6)"),
        running(legs("past_ben", {{NORTH, 3}, {EAST, 4}, {SOUTH, 2}, {EAST, 6}}, "up to row 3 above Ben, down at x 18 east of his line, to (24, 5)")),
        step_into("greg_line", EAST, "onto (25, 5), below Greg"),
        battle("bug_catcher_greg", at(ROUTE3, 25, 5, EAST), false, "his Bug Pokemon; Greg is already next to the player"),
        running(legs("above_james_line", {{EAST, 11}}, "along row 5 to the wall at (36, 5)")),
        step_into("james_line", SOUTH, "onto (36, 6), at the end of James's line"),
        battle("bug_catcher_james", at(ROUTE3, 36, 6, SOUTH), false, "his Bug Pokemon; James stops at (35, 6)"),
        //  Saved here, the trainers behind, so a failure on the way to Route 4
        //  doesn't fight them again.
        running(legs("save_spot", {{SOUTH, 2}}, "down to (36, 8), beside the wall east of it")),
    });
}

//  Route 3's north exit (x 68..75) joins Route 4 (offset 60) at x 8..15, up
//  which column 12 leads to the Pokemon Center's door (12, 5). Lass Robin
//  (40, 11) wanders up and down at x 40, looking north and south, sight 3.
//  Rock above (y <= 8) and grass below (y >= 12) leave every way east
//  across her column at y 9..11, so whether she sees the player depends on
//  her facing. The walk watches for her after the leg along row 9, which
//  crosses her sight on (40, 9) only; if she saw the player, her message
//  goes to story_interrupts(), which fight her, and the route goes on from
//  (40, 9) (run_route(); console B, 2026-11-02, she stopped the player
//  there and spoke after the leg's short watch).
StoryCheckpoint magikarp_salesman(){
    StoryCheckpoint checkpoint = row("00.12", "Route 3 and 4 to the Magikarp salesman", at(ROUTE3, 36, 8, SOUTH), {}, StartMenu::FULL, {
        running(route("route4_nurse", at(ROUTE4_CENTER, 7, 4, NORTH), "east over Route 3, off the grass, north into Route 4 and its Pokemon Center")),
        heal("heal_route4"),
        //  Gift RNG starts from a save facing the salesman (1, 3).
        running(route("before_salesman", at(ROUTE4_CENTER, 2, 3, WEST), "beside the Magikarp salesman, facing him")),
    });
    checkpoint.handoff = StoryHandoff::GIFT_RNG;
    return checkpoint;
}

//  Gift RNG buys the Magikarp (500 of the money 00.12 brings) without
//  moving the player and leaves its summary open, or HOME. This saves the
//  shiny where 00.12 ended. A run started here with Gift RNG's handoff on
//  loads the save beside the salesman and runs Gift RNG first.
StoryCheckpoint after_gift_rng(){
    StoryCheckpoint checkpoint = row("00.12b", "after Gift RNG: save with the Magikarp", at(ROUTE4_CENTER, 2, 3, WEST), {}, StartMenu::FULL, {
        game_step(LEAVE_GIFT_SUMMARY, at(ROUTE4_CENTER, 2, 3, WEST), "the Magikarp's summary Gift RNG left open, or HOME"),
    });
    checkpoint.live_start = true;
    return checkpoint;
}

}


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints{
        new_game(), after_sid_helper(), leave_house(), oaks_trigger(), to_starter_ball(), starter(), parcel(), pokedex(), shopping(), forest(), brock(),
        running_shoes(), route3(), magikarp_salesman(), after_gift_rng(),
    };
    return checkpoints;
}



}
}
}

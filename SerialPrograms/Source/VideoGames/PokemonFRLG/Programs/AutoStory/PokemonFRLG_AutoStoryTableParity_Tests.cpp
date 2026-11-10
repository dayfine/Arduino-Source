/*  AutoStory Table Parity Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The C++ rows the story table had before StoryData/, built with the
 *  shared step API, against the checkpoints read from the files: each
 *  loads to the same steps, args (with the fallbacks the reader fills in),
 *  poses, supplies and START menu. Removed with the C++ rows.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryChoice.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr Direction NORTH = Direction::NORTH;
constexpr Direction SOUTH = Direction::SOUTH;
constexpr Direction EAST = Direction::EAST;
constexpr Direction WEST = Direction::WEST;

constexpr std::string_view BEDROOM = "PalletTown_PlayersHouse_2F";
constexpr std::string_view PALLET = "PalletTown";
constexpr std::string_view LAB = "PalletTown_ProfessorOaksLab";
constexpr std::string_view VIRIDIAN = "ViridianCity";
constexpr std::string_view VIRIDIAN_MART = "ViridianCity_Mart";
constexpr std::string_view VIRIDIAN_CENTER = "ViridianCity_PokemonCenter_1F";
constexpr std::string_view FOREST = "ViridianForest";
constexpr std::string_view FOREST_GATE = "Route2_ViridianForest_NorthEntrance";
constexpr std::string_view PEWTER = "PewterCity";
constexpr std::string_view PEWTER_CENTER = "PewterCity_PokemonCenter_1F";
constexpr std::string_view GYM = "PewterCity_Gym";
constexpr std::string_view ROUTE3 = "Route3";
constexpr std::string_view ROUTE4 = "Route4";
constexpr std::string_view ROUTE4_CENTER = "Route4_PokemonCenter_1F";
constexpr std::string_view MT_MOON_1F = "MtMoon_1F";
constexpr std::string_view MT_MOON_B2F = "MtMoon_B2F";
constexpr std::string_view CERULEAN = "CeruleanCity";
constexpr std::string_view CERULEAN_CENTER = "CeruleanCity_PokemonCenter_1F";
constexpr std::string_view CERULEAN_MART = "CeruleanCity_Mart";
constexpr std::string_view CERULEAN_GYM = "CeruleanCity_Gym";
constexpr std::string_view ROUTE24 = "Route24";
constexpr std::string_view ROUTE25 = "Route25";
constexpr std::string_view SEA_COTTAGE = "Route25_SeaCottage";
constexpr std::string_view ROUTE6 = "Route6";
constexpr std::string_view VERMILION = "VermilionCity";
constexpr std::string_view VERMILION_CENTER = "VermilionCity_PokemonCenter_1F";
constexpr std::string_view VERMILION_GYM = "VermilionCity_Gym";
constexpr std::string_view SS_ANNE_1F = "SSAnne_1F_Corridor";
constexpr std::string_view SS_ANNE_2F = "SSAnne_2F_Corridor";
constexpr std::string_view CAPTAINS_OFFICE = "SSAnne_CaptainsOffice";

Pokemon::GridPose at(std::string_view map, int x, int y, std::optional<Direction> facing = std::nullopt){
    return Pokemon::GridPose{std::string(map), {{x, y}}, facing};
}

StoryCheckpoint row(
    std::string id, std::string title, std::optional<Pokemon::GridPose> start,
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

Supplies after_brock(){
    Supplies supplies;
    supplies.lead = story_lead_after_brock();
    return supplies;
}

//  What a walk does about wild Pokemon: runs, or fights them for EXP.
enum class WildBattles{
    RUN,
    FIGHT,
};

StoryStep make(StoryStepKind kind, std::string name, std::string why, Pokemon::StoryActionArgs args = {}){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    step.args = std::move(args);
    return step;
}

StoryStep route(std::string name, Pokemon::GridPose target, std::string why, WildBattles wild = WildBattles::RUN){
    return make(StoryStepKind::ROUTE, std::move(name), std::move(why), {
        {"target", std::move(target)}, {"fight_wild", wild == WildBattles::FIGHT},
    });
}

StoryStep step_into(std::string name, Direction direction, std::string why){
    return make(StoryStepKind::STEP, std::move(name), std::move(why), {{"direction", direction}});
}

StoryStep into_line(std::string name, Direction direction, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.action = TRAINER_LINE_VARIANT;
    return step;
}

StoryStep hop(std::string name, Direction direction, Pokemon::GridPose landing, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.pose_after = std::move(landing);
    return step;
}

StoryStep into_sight(std::string name, Direction direction, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.action = TRAINER_WAIT_VARIANT;
    return step;
}

StoryStep cut_tree(std::string name, Pokemon::GridPose tree, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why), {{"direction", tree.facing.value_or(Direction::NORTH)}});
    step.action = CUT_TREE_VARIANT;
    step.pose_after = std::move(tree);
    return step;
}

StoryStep trash_cans(std::string name, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why));
    step.action = TRASH_CANS_VARIANT;
    return step;
}

StoryStep unless(const char* flag, StoryStep step){
    step.skip_if.emplace_back(flag);
    return step;
}

StoryStep answer(std::string name, bool yes, std::string why){
    return make(StoryStepKind::ANSWER, std::move(name), std::move(why), {{"yes", yes}});
}

StoryStep scene(std::string name, std::optional<Pokemon::GridPose> pose_after, std::string why){
    StoryStep step = make(StoryStepKind::MASH, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep battle(std::string name, Pokemon::GridPose pose_after, bool may_lose, std::string why){
    StoryStep step = make(StoryStepKind::BATTLE, std::move(name), std::move(why), {{"may_lose", may_lose}});
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, WildBattles wild = WildBattles::RUN){
    return make(StoryStepKind::LEGS, std::move(name), std::move(why), {
        {"legs", std::move(legs)}, {"fight_wild", wild == WildBattles::FIGHT},
    });
}

//  `step`, a ROUTE or LEGS, run with B held: from the Running Shoes on.
StoryStep running(StoryStep step){
    step.action = RUN_VARIANT;
    return step;
}

StoryStep new_game_intro(Pokemon::GridPose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::NEW_GAME, "intro", std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}

StoryStep ready(std::string name, Pokemon::GridPose nurse, std::string why){
    return make(StoryStepKind::READY, std::move(name), std::move(why), {{"target", std::move(nurse)}});
}

StoryStep heal(std::string name){
    return make(StoryStepKind::HEAL, std::move(name), "full HP");
}

StoryStep shop(std::string name, std::vector<Pokemon::StoryItem> items, std::string why){
    return make(StoryStepKind::SHOP, std::move(name), std::move(why), {{"items", std::move(items)}});
}

StoryStep game_step(std::string action, Pokemon::GridPose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::GAME, action, std::move(why));
    step.action = std::move(action);
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
    return row("00.10", "Pewter: the Running Shoes from Oak's aide", at(GYM, 6, 6, NORTH), after_brock(), StartMenu::FULL, {
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
    return row("00.11", "Pewter Center, then Route 3's trainers", at(PEWTER, 17, 26, SOUTH), after_brock(), StartMenu::FULL, {
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
    StoryCheckpoint checkpoint = row("00.12", "Route 3 and 4 to the Magikarp salesman", at(ROUTE3, 36, 8, SOUTH), after_brock(), StartMenu::FULL, {
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
    StoryCheckpoint checkpoint = row("00.12b", "after Gift RNG: save with the Magikarp", at(ROUTE4_CENTER, 2, 3, WEST), after_brock(), StartMenu::FULL, {
        game_step(LEAVE_GIFT_SUMMARY, at(ROUTE4_CENTER, 2, 3, WEST), "the Magikarp's summary Gift RNG left open, or HOME"),
    });
    checkpoint.live_start = true;
    return checkpoint;
}

//  The Magikarp goes to BOX 1 at the Center's PC (11, 1), so Mt. Moon is
//  fought with the starter alone (console B, 2026-11-02: done by hand and
//  saved at (11, 2), facing the PC, where this row ends). The wandering
//  boy (4..6, 3..5) is walked around. The Magikarp comes from Gift RNG:
//  with Gift RNG off the party is the starter alone, and the deposit is
//  skipped.
StoryCheckpoint magikarp_to_box(){
    return row("00.12c", "Route 4 Center: the Magikarp into BOX 1", at(ROUTE4_CENTER, 2, 3, WEST), after_brock(), StartMenu::FULL, {
        running(route("pc_front", at(ROUTE4_CENTER, 11, 2, NORTH), "below the PC (11, 1), facing it")),
        pc_step("magikarp_to_box", "DEPOSIT the party's second slot into BOX 1, if it has one", PcTransfer{
            .action = PcAction::DEPOSIT, .party_slot = 1, .box = 0, .if_missing = PcMissing::SKIP,
        }),
    });
}

//  Mt. Moon (pokefirered data/maps/MtMoon_1F, _B1F, _B2F). Every floor tile
//  of the cave has wild encounters, which are run from, so its legs end on
//  walls, ladders or someone standing still, which the walker holds again
//  after a battle; the few open ones are short. The way through: up 1F's
//  middle and west side, its north-west ladder, B1F's west ladder down to
//  B2F's south-east, round B2F's south edge to its west, north past
//  Miguel and the fossils, and the ladder up to B1F's east end, which
//  leads out to Route 4 east of the mountain.
//  - 1F's trainers are walked around: Youngster Josh (13, 17) sees east
//    along row 17 to x 17, so the way west into the west room is down
//    column 18 and up column 11; Lass Miriam walks round (32..33, 4..5),
//    sight 2, so the way east keeps to row 9 and columns 38 and 40; Hiker
//    Marcos wanders (5..9, 8..12) and sees one tile, so the way north is
//    column 11 and row 3.
//  - The Moon Stone ball (3, 2) on 1F (FLAG_HIDE_MT_MOON_1F_MOON_STONE) is
//    a few tiles off the way to the ladder (5, 6).
//  - The Grunt at (37, 21) on B2F looks around (sight 3) over the only way
//    east round B2F. His room is crossed on legs to walls, at one tile of
//    his sight each, (37, 22) and (40, 21); if he looks the player's way,
//    he walks up beside the leg, story_interrupts() fights him, and the
//    leg is held on to its wall.
//  - The Grunt at (12, 20) faces south (sight 2) over (12, 21..22), across
//    rows 21..22, the only way north to Miguel: stepped into from the
//    west, he is next to the player and doesn't walk.
//  - Super Nerd Miguel (13, 11) battles on the step onto (14, 11)
//    (MtMoon_B2F_EventScript_MiguelTrigger, VAR_MAP_SCENE_MT_MOON_B2F 0),
//    then "We'll each take one!"
//  - The fossils: DOME (13, 7), HELIX (14, 7). After the HELIX, Miguel
//    walks up to (13, 8) and takes the DOME, which opens the gap north.
//  The save is on Route 4: the save card's places don't include Mt. Moon.
StoryCheckpoint mt_moon(){
    return row("00.13", "Mt. Moon: a Moon Stone, a Rocket, Miguel and the Helix Fossil", at(ROUTE4_CENTER, 11, 2, NORTH), after_brock(), StartMenu::FULL, {
        running(route("route4_nurse", at(ROUTE4_CENTER, 7, 4, NORTH), "the nurse, below the counter")),
        heal("heal_route4"),
        running(route("mt_moon_entrance", at(MT_MOON_1F, 18, 37, NORTH), "out of the Center, west into Mt. Moon")),
        running(legs("mt_moon_1f", {
            {NORTH, 12}, {EAST, 8}, {NORTH, 12}, {EAST, 3}, {SOUTH, 1}, {EAST, 9},
            {NORTH, 1}, {EAST, 2}, {NORTH, 4}, {WEST, 24}, {EAST, 2}, {SOUTH, 12},
            {WEST, 7}, {NORTH, 18}, {WEST, 9}, {EAST, 1},
        }, "up the middle, east of Miriam, west along row 9, down column 18 past Josh, up column 11 past Marcos, to (3, 3)")),
        running(route("below_moon_stone", at(MT_MOON_1F, 3, 3, NORTH), "facing the Moon Stone (3, 2)")),
        talk("moon_stone_ball", "A on the item ball"),
        //  No reader for the text yet: the scene's end is the overworld.
        scene("found_moon_stone", at(MT_MOON_1F, 3, 3, NORTH), "\"<player> found a MOON STONE!\", then the pocket line; the ball is gone"),
        running(route("b2f_ladder_foot", at(MT_MOON_B2F, 26, 21, EAST), "1F's ladder (5, 6), down B1F's west side to its ladder (22, 18), onto B2F")),
        running(legs("b2f_south", {
            {NORTH, 4}, {EAST, 8}, {WEST, 2}, {SOUTH, 5}, {EAST, 8}, {NORTH, 3}, {WEST, 1}, {NORTH, 5},
            {EAST, 7}, {SOUTH, 14}, {WEST, 9}, {SOUTH, 10}, {WEST, 29}, {EAST, 3}, {NORTH, 17},
        }, "through the Grunt's room at (37, 21), round the south edge, up column 11 to (11, 21)")),
        step_into("grunt_line", EAST, "onto (12, 21), below the Grunt at (12, 20)"),
        battle("rocket_grunt", at(MT_MOON_B2F, 12, 21, EAST), false, "Rattata and Zubat, Lv13"),
        ready("ready_for_miguel", at(ROUTE4_CENTER, 7, 4, NORTH), "items for a status or low HP; the nurse, back through the cave, only if they can't"),
        //  Up column 13 to Miguel, who stops the leg on (13, 12), then east
        //  to the wall: the tile below his trigger.
        running(legs("below_miguel_trigger", {{EAST, 1}, {NORTH, 9}, {EAST, 1}}, "to (14, 12), below Miguel's trigger (14, 11)")),
        step_into("miguel_trigger", NORTH, "onto (14, 11), beside Miguel"),
        battle("super_nerd_miguel", at(MT_MOON_B2F, 14, 11, NORTH), false, "Grimer, Voltorb and Koffing, Lv12; \"We'll each take one!\""),
        running(legs("below_helix_fossil", {{NORTH, 3}}, "up column 14 to the HELIX FOSSIL (14, 7)")),
        fossil_step("helix_fossil", "the HELIX FOSSIL; Miguel takes the DOME FOSSIL", Fossil::HELIX),
        //  The maps keep the fossils, which the game removes after the
        //  choice: the step onto the HELIX's tile is taken on its own.
        step_into("helix_gap", NORTH, "onto (14, 7), where the HELIX FOSSIL lay"),
        running(route("route4_east", at(ROUTE4, 32, 6, SOUTH), "west to B2F's ladder (5, 10), up to B1F, east to its exit onto Route 4")),
    });
}


//  Route 4 east of Mt. Moon (Route4 map.json) joins Cerulean City at its
//  east end (x 107; Route 4's row 3 is the city's row 13). Its upper road
//  (rows 2..8) reaches the city only by water: the way on by land is down
//  the ledge row 9 at x 90..96 to the lower road (rows 10..12), which
//  joins the city's rows 20..22. Lass Crissy (75, 3) faces east, sight 4,
//  over row 3 from x 76 to 79; the way east keeps out of her line and off
//  the grass south of the road (75..87, 11..17).
//
//  On the way, the Mega Kick tutor (Black Belt (50, 3), facing west,
//  Route4_EventScript_MegaKickTutor) stands in a pen (46..51, 2..4) north
//  of the road, beside the Mega Punch tutor (47, 3); the pen opens south
//  only on (48, 5) and (49, 5), the rest of row 5 ledges out of it. From
//  Mt. Moon's exit the walk goes east and up into the pen to (50, 4),
//  below the tutor, and Mega Kick takes Withdraw's slot. Cerulean can't
//  be walked back to Route 4's upper road without Surf, so it is now or
//  much later.
//
//  The open leg east across the city's edge ended a tile short on the
//  console (2026-11-03: on (6, 20), not (7, 20)), and no hold to a wall
//  in the city's west end finds the player from there: the next hold
//  south met the fence on (6, 23) and the walk went on a column off. So
//  the entry ends on (9, 20), and only legs to walls follow: north up
//  column 9 (or 8, a tile short) to row 12, west to (8, 12), east to
//  (30, 12) beside the policeman (31, 12), south to (30, 17), west to
//  (25, 17), and down the ledge row 18 to (25, 19), a few tiles from the
//  door (22, 19). The way keeps out of the boy's (16, 18..24) and the
//  balding man's (8..10, 22..24) wander boxes.
StoryCheckpoint route4_to_cerulean(){
    const TeachPlan mega_kick{.item = std::string(TEACH_TUTOR), .move = "mega-kick", .member = 0, .forget_move = "withdraw"};
    return row("00.14", "Route 4: Mega Kick from its tutor, then east to Cerulean's Pokemon Center", at(ROUTE4, 32, 6, SOUTH), after_brock(), StartMenu::FULL, {
        running(route("mega_kick_tutor", at(ROUTE4, 50, 4, NORTH), "east and up through (48, 5) into the tutors' pen, below the Mega Kick tutor (50, 3)")),
        teach_step("teach_mega_kick", "the tutor's Mega Kick in place of Withdraw, unless the lead knows it", mega_kick),
        running(route("above_ledge", at(ROUTE4, 93, 8, SOUTH), "east along Route 4's upper road to above the ledge row 9")),
        hop("ledge_hop", SOUTH, at(ROUTE4, 93, 10, SOUTH), "down the ledge (93, 9) to the lower road"),
        running(route("cerulean_entry", at(CERULEAN, 9, 20, EAST), "east into Cerulean City, to (9, 20) below column 9")),
        running(legs("above_city_ledge", {{NORTH, 8}, {WEST, 1}, {EAST, 22}, {SOUTH, 5}, {WEST, 5}}, "to walls: (9, 12), (8, 12), (30, 12), (30, 17), (25, 17)")),
        hop("city_ledge_hop", SOUTH, at(CERULEAN, 25, 19, SOUTH), "down the ledge (25, 18)"),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "west along row 20 to the Pokemon Center's door (22, 19)")),
        heal("heal_cerulean"),
    });
}

//  Cerulean Gym (pokefirered CeruleanCity_Gym map.json and scripts.inc):
//  a pool with walkways. Misty (8, 6) stands on a platform (7..9, 6) above
//  row 7. Picnicker Diana (4, 7) faces east, sight 4, over (5..8, 7), so
//  misty_front (8, 7) is in her line; Swimmer Luis (10, 12) in the water
//  faces west, sight 1, over (9, 12), on column 9 north of the door. No
//  gym trainer has to be fought for Misty, so neither is: from the door up
//  column 9 to row 13 (a route there, so the planner's walk to a wall
//  doesn't run on past it through (9, 12)), west to (4, 13), and to walls
//  round the pool's west and north sides, (4, 10), (12, 10), (12, 7), then
//  west to (9, 7), the tile below the platform's east end, out of Diana's
//  line. 00.14c steps up onto (9, 6) and talks to Misty from beside her.
//
//  Misty beat a Lv20 Wartortle (console B, run m51), so first a GRIND to
//  Lv23 on 00.15's patch. Gen 3 numbers (pokefirered: her mons have IV 0;
//  Wartortle taken at IV 15, no EVs): Starmie Lv21 has 56 HP, Atk 36, SpA
//  47, SpD 40. Bite is Dark, special in Gen 3, no STAB, x2 on Psychic: at
//  Lv23 (SpA 39) it does 23..28, three hits (two on top rolls); at Lv20,
//  20..24. Her Swift (physical, 60) does 9..11 to a Lv23 Wartortle (about
//  67 HP, Def 45) and Water Pulse (STAB, resisted) 8..10: 6 or more hits,
//  though m51 saw about 20 a turn. Her Recover (28) and Super Potion undo
//  a Bite, so the margin is in Bite's damage.
//
//  The Mart comes before the GRIND: its wild battles are fought only with
//  2 or more Potions, and 00.14's save has none (m51 met Misty with only
//  the 5 this row bought). 5 Potions (row 3, 300 each), then up to 3 Super
//  Potions (row 2, 700), last so they take the Items pocket's last row;
//  the battle doesn't use them yet. The money box can't be read, so the
//  floor stands for it, and a floor above the money fails the step (the
//  quantity box wraps). Prizes (pokefirered: 4 x last level x the class's
//  factor) from a new game's 3000, less Viridian's 2000 and the Magikarp's
//  500: Liam 220, Brock 1400, Route 3's bug catchers 360, the Mt. Moon
//  grunt 416 and Miguel 288 (the first rival's 80 only if won), at least
//  3184 here. Floor and budget 3000: 5 Potions and 2 Super Potions, 2900.
//  Misty's 2100 (4 x 21 x 25) then leaves 00.15 its floor of 1500. Then the
//  nurse, and the save before Misty, so her battle is its own checkpoint,
//  00.14c, verified and retried from it. 00.15's GRIND then fights
//  until the sim beats the Cerulean rival often enough.
StoryCheckpoint cerulean_gym(){
    const MartPlan supplies{
        .map = std::string(CERULEAN_MART),
        .lines = {{.item = "potion", .quantity = 5}, {.item = "super-potion", .quantity = 3}},
        .budget = 3000,
        .money_floor = 3000,
    };
    const GrindPlan to_lv23{.member = 0, .level = 23, .max_encounters = 100, .pace = WEST, .heal_every = 0};
    return row("00.14b", "Cerulean: Potions, a GRIND to Lv23, then into the Gym, saved below Misty's platform", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("mart_clerk", at(CERULEAN_MART, 4, 3, WEST), "out of the Center to the Mart's clerk")),
        mart_step("buy_potions", "5 Potions for the GRIND's wild battles, then Super Potions as the money allows", supplies),
        running(route("grind_patch", at(ROUTE4, 86, 16, WEST), "west out of the city along Route 4's lower road onto its grass's east edge")),
        grind_step("grind_to_lv23", "wild battles until the lead is Lv23, where Bite takes Starmie in three", to_lv23, at(CERULEAN_CENTER, 7, 4, NORTH)),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "back to the Center: Misty's battle starts at full HP and PP")),
        heal("heal_cerulean"),
        running(route("gym_below_luis", at(CERULEAN_GYM, 9, 13), "into the Gym, up column 9 to row 13, short of Luis's (9, 12)")),
        running(legs("gym_west_walkway", {{WEST, 5}}, "west to the wall (4, 13)")),
        running(legs("below_misty_platform", {{NORTH, 3}, {EAST, 8}, {NORTH, 3}, {WEST, 3}}, "walls (4, 10), (12, 10), (12, 7), then to (9, 7), out of Diana's line")),
    });
}

//  Misty (TRAINER_LEADER_MISTY: Staryu Lv18, Starmie Lv21), talked to from
//  (9, 6), beside her on the platform. Her win text runs on to the CASCADE
//  BADGE, TM03 (Water Pulse) and its explanation, all read with the
//  battle's. Bite is chosen against her party (Starmie is Water/Psychic).
//  Back to the nurse round the pool's west side, out of Luis's line; then
//  Water Pulse in place of Tail Whip, last, at the Center, from the bag's
//  TM CASE. Ends where 00.15 starts.
StoryCheckpoint misty(){
    const TeachPlan water_pulse{.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "tail-whip"};
    return row("00.14c", "Cerulean Gym: Misty, then Water Pulse", at(CERULEAN_GYM, 9, 7, WEST), after_brock(), StartMenu::FULL, {
        running(route("beside_misty", at(CERULEAN_GYM, 9, 6, WEST), "up onto (9, 6), facing Misty (8, 6)")),
        talk("challenge_misty", "Misty's speech"),
        battle("misty", at(CERULEAN_GYM, 9, 6, WEST), false, "Staryu and Starmie, with Bite; CASCADE BADGE and TM03"),
        running(legs("gym_way_out", {{SOUTH, 1}, {EAST, 3}, {SOUTH, 3}, {WEST, 8}, {SOUTH, 3}, {EAST, 5}}, "walls (12, 7), (12, 10), (4, 10), (4, 13), (9, 13), round Luis's (9, 12)")),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "down column 9 out of the Gym, to the Center")),
        heal("heal_cerulean"),
        teach_step("teach_water_pulse", "TM03 Water Pulse in place of Tail Whip", water_pulse),
    });
}

//  Before the rival north of Cerulean (cerulean_rival()), from the
//  Center: first TM03 Water Pulse over Tail Whip, if the lead doesn't know
//  it yet, so the GRIND's sim plays it (a run without TM03 from Misty
//  skips the step). Then 3 Potions at the Mart
//  (CeruleanCity_Mart_Items: POTION is row 3, 300 each; Mt. Moon's
//  trainers leave well over the floor) and 2 Awakenings (row 6, 250 each;
//  pokefirered items.json) for the rival's Bulbasaur's Sleep Powder
//  (console B, m56: asleep at 34/65 HP, it fainted two turns later with
//  15 Potions), 1400 of the floor's 1500. Then a GRIND on Route 4's grass
//  east of Mt. Moon (75..86, 11..17; Spearow, Rattata, Ekans and Mankey
//  around Lv10, about 80 EXP each), healing at the Center, until the
//  battle sim beats the rival 00.15b fights 50% or more: the win chance
//  his battle refuses below. Console B's Wartortle (Lv22, 65 HP, Water
//  Pulse) is about 42% there and 62% at Lv23; Lv28 (about 88%, some 120
//  battles from Lv22) is the cap. Then the nurse, and the save on (23, 7),
//  the tile below his trigger: his battle is its own checkpoint, 00.15b,
//  verified and retried from this save.
StoryCheckpoint cerulean_grind(){
    const TeachPlan water_pulse{.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "tail-whip"};
    const MartPlan potions{
        .map = std::string(CERULEAN_MART),
        .lines = {{.item = "potion", .quantity = 3}, {.item = "awakening", .quantity = 2}},
        .budget = 1500,
        .money_floor = 1500,
    };
    const GrindPlan for_rival{
        .member = 0, .level = 28, .max_encounters = 150, .pace = WEST, .heal_every = 0,
        .trainer = std::string(story_trainer(CERULEAN, "rival_cerulean").value_or("")), .min_chance = MIN_WIN_CHANCE,
    };
    return row("00.15", "Cerulean: Water Pulse, Potions, a GRIND until the rival is winnable, then saved below his trigger", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        teach_step("teach_water_pulse", "TM03 Water Pulse in place of Tail Whip, unless the lead knows it or there is no TM03", water_pulse),
        running(route("mart_clerk", at(CERULEAN_MART, 4, 3, WEST), "out of the Center to the Mart's clerk")),
        mart_step("buy_potions", "3 Potions: the GRIND fights wild battles only with 2 or more; 2 Awakenings for the rival's Sleep Powder", potions),
        running(route("grind_patch", at(ROUTE4, 86, 16, WEST), "west out of the city along Route 4's lower road onto its grass's east edge")),
        grind_step("grind_for_rival", "wild battles until the sim beats the rival 50% or more, Lv28 at most; the nurse when hurt or low on PP", for_rival, at(CERULEAN_CENTER, 7, 4, NORTH)),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "back to the Center: the rival's battle starts at full HP and PP")),
        heal("heal_cerulean"),
        running(route("below_rival_trigger", at(CERULEAN, 23, 7, NORTH), "out of the Center, north up to (23, 7), short of the trigger row 6")),
    });
}

//  The rival north of Cerulean (CeruleanCity scripts.inc): the only way
//  north to Nugget Bridge is columns 22..24, and a step onto row 6 there
//  (CeruleanCity_EventScript_RivalTrigger*, VAR_MAP_SCENE_CERULEAN_CITY_RIVAL
//  0) brings him down to the player. From the middle, (23, 6), he stops
//  at (23, 5); after the battle he walks round the player and off south,
//  and the player, turning to watch him, ends facing south. His FAME
//  CHECKER's text is read to the overworld with the battle's. For a player
//  who took Squirtle he has Pidgeotto Lv17, Abra Lv16, Rattata Lv15 and
//  Bulbasaur Lv18 (sParty_RivalCeruleanBulbasaur), whose Vine Whip and
//  Sleep Powder are the danger. The row starts from 00.15's save below the
//  trigger, and saves after him. First TM03 Water Pulse over Tail Whip (from
//  the bag, where the player stands), if the lead doesn't know it yet:
//  console B's save (2026-11-03) is past Misty's TEACH, which failed there.
//  Where she taught it, the step reads the moves and does nothing.
StoryCheckpoint cerulean_rival(){
    const TeachPlan water_pulse{.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "tail-whip"};
    return row("00.15b", "Cerulean: Water Pulse if not known, then the rival north of the city", at(CERULEAN, 23, 7, NORTH), after_brock(), StartMenu::FULL, {
        teach_step("teach_water_pulse", "TM03 Water Pulse in place of Tail Whip, unless the lead knows it", water_pulse),
        step_into("rival_trigger", NORTH, "onto (23, 6), the middle trigger: \"Yo! <player>!\""),
        battle("rival_cerulean", at(CERULEAN, 23, 6, SOUTH), false, "Pidgeotto, Abra, Rattata and Bulbasaur; then the FAME CHECKER"),
    });
}

//  Nugget Bridge (Route24 map.json) is x 10..12 from row 32 north to row
//  16, the city's columns 22..24 (offset 12). Its five trainers stand on
//  it facing across, sight 2, so each line crosses the middle column 11
//  on the trainer's row: Bug Catcher Cale (12, 31), Lass Ali (10, 28),
//  Youngster Timmy (12, 25), Lass Reli (10, 22) and Camper Ethan (12, 19).
//  Each is fought on the step onto their row, beside them: they don't
//  walk, and the player keeps facing north. Two tiles separate the rows,
//  walked as one short leg; the step onto a row goes on a tile at a time
//  until the trainer sees the player (into_line()), as the open leg onto
//  the bridge ended a tile short twice on console B (2026-11-03), on
//  (11, 33), and one step left the player out of Cale's sight. Then the
//  Rocket (12, 15): the step onto (11, 15)
//  (Route24_EventScript_RocketTriggerRight, VAR_MAP_SCENE_ROUTE24 0) gives
//  the NUGGET, "join Team Rocket", and his battle; he only turns, and the
//  player turns east to him. Camper Shane (5, 21) looks north from the
//  west bank, off the way.
StoryCheckpoint nugget_bridge(){
    return row("00.16", "Nugget Bridge: the nurse, five trainers and the Rocket", at(CERULEAN, 23, 6, SOUTH), after_brock(), StartMenu::FULL, {
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "back to the Center after the rival, before Nugget Bridge")),
        heal("heal_cerulean"),
        running(route("below_cale_line", at(ROUTE24, 11, 32, NORTH), "out of the Center, north past the rival's row onto the bridge, below Cale's row")),
        into_line("cale_line", NORTH, "onto (11, 31), beside Cale"),
        battle("bug_catcher_cale", at(ROUTE24, 11, 31, NORTH), false, "his Bug Pokemon"),
        running(legs("below_ali_line", {{NORTH, 2}}, "up to (11, 29)")),
        into_line("ali_line", NORTH, "onto (11, 28), beside Ali"),
        battle("lass_ali", at(ROUTE24, 11, 28, NORTH), false, "her Pokemon"),
        running(legs("below_timmy_line", {{NORTH, 2}}, "up to (11, 26)")),
        into_line("timmy_line", NORTH, "onto (11, 25), beside Timmy"),
        battle("youngster_timmy", at(ROUTE24, 11, 25, NORTH), false, "his Pokemon"),
        running(legs("below_reli_line", {{NORTH, 2}}, "up to (11, 23)")),
        into_line("reli_line", NORTH, "onto (11, 22), beside Reli"),
        battle("lass_reli", at(ROUTE24, 11, 22, NORTH), false, "her Pokemon"),
        running(legs("below_ethan_line", {{NORTH, 2}}, "up to (11, 20)")),
        into_line("ethan_line", NORTH, "onto (11, 19), beside Ethan"),
        battle("camper_ethan", at(ROUTE24, 11, 19, NORTH), false, "his Pokemon"),
        //  A route, not legs: the nurse may have been needed.
        ready("ready_for_rocket", at(CERULEAN_CENTER, 7, 4, NORTH), "items for a status or low HP; the nurse only if they can't"),
        running(route("below_rocket_trigger", at(ROUTE24, 11, 16, NORTH), "up the bridge to below the Rocket's trigger (11, 15)")),
        step_into("rocket_trigger", NORTH, "onto (11, 15), beside the Rocket"),
        battle("rocket_grunt", at(ROUTE24, 11, 15, EAST), false, "the NUGGET, then Ekans and Zubat, Lv15"),
    });
}

//  Route 24 north of Nugget Bridge (Route24 map.json) joins Route 25 by
//  its rows 7..9; the ledges on Route 25's row 6 (x 0..9) leave the way
//  north at x 12..13. Route 25 (Route25 map.json, scripts.inc) to Bill's
//  Sea Cottage door (51, 4): of its nine trainers, four can't be walked
//  around.
//  - Hiker Franklin (11, 4) faces east, sight 4, over (12..15, 4), and
//    Hiker Wayne (17, 7) east over (18..19, 7): the only ways east past
//    x 15. Franklin is fought, from (15, 5) on the step onto (15, 4), the
//    end of his line: he walks up to (14, 4).
//  - Youngster Joey (18, 2) faces south over (18, 3..4): the way keeps
//    off column 18, south round Wayne to (19, 6) and up to (19, 4).
//  - Youngster Dan (22, 4) looks around, sight 2, over the only way east,
//    (20..21, 4). On (20, 4), a one-tile leg, he may see the player and
//    walk up to (21, 4); so the way goes north from there to row 2, east,
//    and down column 23 past him to (23, 5), off every tile he may walk
//    up to. Where his sight is crossed, the leg ends on a wall or is one
//    tile; if he looks the player's way, story_interrupts() fight him.
//  - Picnicker Kelsey (22, 8) faces east over (23..24, 8), across the
//    only way on: fought on the step onto (24, 8); she walks up to (23, 8).
//  - Hiker Nob (27, 9) faces north over (27, 6..8), Camper Flint (28, 4)
//    south over (28, 5..7): every way east crosses one. Nob is fought on
//    the step onto (27, 8), beside him; the way on keeps off Flint's
//    column, by one-tile legs and legs to walls.
//  - Youngster Chad (36, 4) walks up and down column 36 (rows 2..7), so
//    he may see any tile of it down to row 9. The way crosses his column
//    on row 9, a leg to the water at (42, 9), where he can only walk up
//    beside it.
//  - Lass Haley (42, 5) faces south over (42, 6..8), the only way east of
//    x 42: fought on the step onto (42, 8); she walks up to (42, 7).
//  The row saves inside the Sea Cottage, which the save card names
//  ROUTE 25.
StoryCheckpoint route25(){
    return row("00.17", "Route 24 and 25 to Bill's Sea Cottage: Franklin, Kelsey, Nob and Haley", at(ROUTE24, 11, 15, EAST), after_brock(), StartMenu::FULL, {
        running(route("below_franklin_line", at(ROUTE25, 15, 5, NORTH), "north off Nugget Bridge, east into Route 25, round its ledges to (15, 5), below the end of Franklin's line")),
        into_line("franklin_line", NORTH, "onto (15, 4), at the end of Franklin's line"),
        battle("hiker_franklin", at(ROUTE25, 15, 4, NORTH), false, "Machop and Geodude, Lv15; Franklin stops at (14, 4)"),
        running(legs("west_of_dan", {{EAST, 2}, {SOUTH, 2}, {EAST, 2}, {NORTH, 2}}, "to (17, 4), short of Joey's (18, 4); round Wayne (17, 7) to (19, 6); up to (19, 4)")),
        running(legs("round_dan", {
            {EAST, 1}, {NORTH, 2}, {EAST, 4}, {WEST, 1}, {SOUTH, 3}, {EAST, 1}, {SOUTH, 2},
        }, "onto (20, 4); up to (20, 2), east to (24, 2), down column 23 past Dan to (23, 5), to (24, 7) above Kelsey's line")),
        into_line("kelsey_line", SOUTH, "onto (24, 8), at the end of Kelsey's line"),
        battle("picnicker_kelsey", at(ROUTE25, 24, 8, SOUTH), false, "Nidoran M and Nidoran F, Lv15; Kelsey stops at (23, 8)"),
        running(legs("west_of_nob_line", {{EAST, 2}}, "along row 8 to (26, 8)")),
        into_line("nob_line", EAST, "onto (27, 8), above Nob"),
        battle("hiker_nob", at(ROUTE25, 27, 8, EAST), false, "three Geodude and a Machop, Lv13"),
        running(legs("below_haley_line", {
            {EAST, 1}, {SOUTH, 1}, {EAST, 1}, {NORTH, 3}, {EAST, 5}, {SOUTH, 3}, {EAST, 8},
        }, "below Flint's column to (29, 9), up to (29, 6), east to (34, 6), down to (34, 9), east along row 9 past Chad's column to the water, (42, 9)")),
        into_line("haley_line", NORTH, "onto (42, 8), at the end of Haley's line"),
        battle("lass_haley", at(ROUTE25, 42, 8, NORTH), false, "Oddish, Pidgey and Oddish, Lv13; Haley stops at (42, 7)"),
        running(route("sea_cottage", at(SEA_COTTAGE, 7, 8, NORTH), "east along row 8, up column 51 into the Sea Cottage (51, 4), a step in from its mat")),
    });
}

//  Bill's Sea Cottage (Route25_SeaCottage scripts.inc). Bill is the
//  Clefairy at (10, 6) (FLAG_HELPED_BILL_IN_SEA_COTTAGE unset): talked to
//  from (10, 7), below him, he asks "Help me out!" (YES/NO; NO goes on the
//  same), says to run the Cell Separation on the PC, and walks up into
//  the right teleporter (10, 3); with the player facing north he walks
//  straight up. The PC (4, 5) answers only from the south, (4, 6). Its
//  scene: the teleporters run, Bill comes out of the left one (3, 3) and
//  walks to (7, 6) (BillWalkToMiddleOfRoom); the player isn't moved.
//  Talked to from (6, 6), Bill gives the S.S. TICKET, no YES/NO, and goes
//  on about the S.S. ANNE (FLAG_GOT_SS_TICKET, which also hides Nugget
//  Bridge's Rocket). Out by the mat (7, 9); the door puts the player on
//  (51, 5), facing south. The maps keep Bill at (7, 5), but after his
//  scene he stands on (7, 6), in the way from (6, 6) to the mat (console
//  B, 2026-11-03), so the walk out goes around him to (7, 8) first.
StoryCheckpoint bill(){
    return row("00.18", "Bill's Sea Cottage: the Cell Separator and the S.S. Ticket", at(SEA_COTTAGE, 7, 8, NORTH), after_brock(), StartMenu::FULL, {
        running(route("below_bill_clefairy", at(SEA_COTTAGE, 10, 7, NORTH), "below the Clefairy (10, 6), facing it")),
        talk("bill_clefairy", "\"Hiya! I'm a POKEMON... ...No I'm not!\" ... \"Help me out!\""),
        answer("help_bill", true, "YES: \"run the Cell Separation on the PC\""),
        scene("bill_into_teleporter", at(SEA_COTTAGE, 10, 7, NORTH), "the Clefairy walks up into the right teleporter"),
        running(route("pc_front", at(SEA_COTTAGE, 4, 6, NORTH), "below the PC (4, 5), facing it")),
        talk("cell_separator", "the PC: \"<player> initiated the TELEPORTER's CELL SEPARATOR!\""),
        scene("bill_comes_out", at(SEA_COTTAGE, 4, 6, NORTH), "the teleporters run; Bill comes out of the left one and walks to (7, 6)"),
        running(legs("beside_bill", {{EAST, 2}}, "to (6, 6), west of Bill, facing him")),
        talk("thank_bill", "\"Thanks, bud! ... take this\""),
        scene("ss_ticket", at(SEA_COTTAGE, 6, 6, EAST), "the S.S. TICKET; \"... the S.S. ANNE ... go instead\""),
        running(legs("around_bill", {{SOUTH, 2}, {EAST, 1}}, "to (7, 8), above the mat; Bill stands on (7, 6) after his scene")),
        running(route("out_of_cottage", at(ROUTE25, 51, 5, SOUTH), "down to the mat and out; the door puts the player below it")),
    });
}

//  Back to Cerulean by Route 25's south path (Route25 map.json): from
//  below the cottage down column 51 to row 12, west along row 12 and row
//  11 into Route 24 (x 14..23, row 11), and down its column 14..16 east
//  of Nugget Bridge to Cerulean's north edge (x 26..28, offset 12). No
//  trainer stands on it. Cerulean's row 1 there is a ledge (26..28, 1),
//  hopped south from (27, 0); then on into the city to the Pokemon Center.
StoryCheckpoint back_to_cerulean(){
    return row("00.19", "Route 25's south path back to Cerulean's Pokemon Center", at(ROUTE25, 51, 5, SOUTH), after_brock(), StartMenu::FULL, {
        running(route("above_cerulean_ledge", at(CERULEAN, 27, 0, SOUTH), "Route 25's south path west, Route 24 east of the bridge south, to above Cerulean's ledge (27, 1)")),
        hop("cerulean_ledge_hop", SOUTH, at(CERULEAN, 27, 2, SOUTH), "down the ledge (27, 1)"),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "into the city to the Pokemon Center")),
        heal("heal_cerulean"),
    });
}

//  Stopgap for console B's save, which caught nothing on Route 1. A new
//  playthrough should instead catch a Rattata on Route 1 or 22 early, as
//  its Cut user and spare member, and drop this row (and its walk over
//  Nugget Bridge and back).
//
//  An Oddish for HM01 Cut, and a second member behind the lead
//  (docs/design/AutoStory-FRLG-FutureEvents.md, "Console B run choices").
//  Route 24's grass (Route24 map.json: x 3..5, rows 19..31, west of Nugget
//  Bridge) is the nearest to the Center with Oddish: FireRed's
//  sRoute24_FireRed (pokefirered wild_encounters.json) has it in 25% of
//  battles, Lv12..14, as Route 25's grass; Routes 5 and 6 aren't on the
//  story's maps yet. Up the bridge, whose trainers 00.16 beat, west along
//  rows 10..15 to the cliff (3, 15) and down column 3 to the grass's first
//  row, (3, 19); the step paces east to (4, 19) and back. Camper Shane
//  (5, 21) faces north, sight 5, over (5, 16..20): column 5 is kept off.
//  The leg west ends on the cliff, not open: on console B (m63) an open
//  leg west to (4, 15) ended a tile short, and the walk down column 5
//  came into Shane's sight.
//
//  First 7 Poke Balls at the Mart (row 1, 200 each; the bag isn't read, so
//  the step buys them whatever the bag holds). The floor, from 00.15's
//  1500 less its 1400: the rival's 288 (4 x 18 x 4), the Rocket's 480 (4
//  x 15 x 8), Franklin's 540 and Nob's 468 (hikers, 9) alone make 1876
//  (pokefirered gTrainerMoneyTable). An Oddish at full HP (catch rate 255)
//  stays in a Poke Ball about 1 time in 3 (Gen 3: a = 85, four shakes of
//  0.76), so 7 balls catch it about 94% of the time; the step throws 7 at
//  most, in 30 battles (none an Oddish: 0.02%). Nothing weakens it: the HP
//  bar isn't read on the console yet.
//
//  The Oddish joins the party second. Battles keep the lead in
//  (PokemonFRLG_AutoStoryBattleDecisions.h): the Oddish is sent only when
//  the lead faints in a trainer's battle, which counts as lost. Back to
//  the nurse, and the save in the Center.
StoryCheckpoint oddish(){
    const MartPlan balls{
        .map = std::string(CERULEAN_MART),
        .lines = {{.item = "poke-ball", .quantity = 7}},
        .budget = 1400,
        .money_floor = 1500,
    };
    const CapturePlan catch_oddish{.species = "oddish", .catches = 1, .max_encounters = 30, .max_balls = 7, .pace = EAST};
    return row("00.20", "Route 24's grass: an Oddish for Cut, then the Center", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("mart_clerk", at(CERULEAN_MART, 4, 3, WEST), "out of the Center to the Mart's clerk")),
        mart_step("buy_poke_balls", "7 Poke Balls for the Oddish", balls),
        running(route("oddish_patch", at(ROUTE24, 3, 19), "north over Nugget Bridge, west to the cliff and down column 3 onto the grass's first row, out of Shane's column 5")),
        capture_step("catch_oddish", "wild battles until an Oddish, then Poke Balls; any other species is run from", catch_oddish),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "back over the bridge to the Center")),
        heal("heal_cerulean"),
    });
}


//  Cerulean's only way south without Cut (CeruleanCity map.json,
//  scripts.inc) is through the burgled house (30, 11), whose door the
//  policeman leaves once the S.S. TICKET is had, out of the hole in its
//  back wall into the yard (31, 8). The Rocket there (33, 6) stops the
//  player on (33, 5) or (33, 7) (CeruleanCity_EventScript_GruntTrigger*):
//  from (32, 7), the step east onto (33, 7) turns the player to him;
//  Machop and Drowzee, Lv17, then TM28 Dig, and he is gone. Then east
//  round the yard, down to the city's south edge by the path along its
//  east side (39..40, 20..31), down Route 5's east side to the
//  Underground Path's north house (31, 31), through its tunnel and out of
//  the south house onto Route 6, below its door (19, 14). Route 6:
//  - Bug Catcher Keigo (3, 16) faces east, sight 5, over the middle path
//    (4..8, 16); Bug Catcher Elijah (20, 25) faces west, sight 3, over
//    the east path (17..19, 25). The way goes down column 15, through the
//    grass to the wall (15, 33), out of both lines; the open leg west to
//    it, a tile short, leaves the player on column 16, out of them too.
//  - Camper Ricky and Picnicker Nancy (12..13, 21) battle only when
//    talked to.
//  - Picnicker Isabelle (13, 32) and Camper Jeff (13, 33) face west, sight
//    3, over (10..12, 32) and (10..12, 33); the only way south, (10..12,
//    34), is from Jeff's row. Up to (15, 31), west to the wall (7, 31),
//    down to the wall (7, 33), east: Jeff is fought on the step onto
//    (10, 33) and walks up to (11, 33). Isabelle's row is kept off.
//  Then down into Vermilion City (offset 12) to its Pokemon Center.
StoryCheckpoint to_vermilion(){
    return row("00.21", "The Cerulean Rocket, Route 5, the Underground Path and Route 6 to Vermilion's Pokemon Center: Jeff", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("west_of_rocket", at(CERULEAN, 32, 7, EAST), "through the burgled house and out of the hole in its back wall, west of the Rocket (33, 6)")),
        step_into("rocket_trigger", EAST, "onto (33, 7), below the Rocket"),
        battle("rocket_grunt", at(CERULEAN, 33, 7, NORTH), false, "Machop and Drowzee, Lv17; TM28 Dig"),
        running(route("route6_column15", at(ROUTE6, 15, 14), "round the yard and down Cerulean's east side, Route 5 into the Underground Path, through it onto Route 6, west to column 15")),
        running(legs("round_keigo_and_elijah", {
            {SOUTH, 19}, {NORTH, 1}, {NORTH, 1}, {WEST, 8}, {SOUTH, 2}, {EAST, 2},
        }, "down column 15 to the wall (15, 33); up to (15, 31), west to the wall (7, 31), down to the wall (7, 33), east to (9, 33)")),
        into_line("jeff_line", EAST, "onto (10, 33), in Jeff's line"),
        battle("camper_jeff", at(ROUTE6, 10, 33, EAST), false, "Spearow and Raticate, Lv16; Jeff stops at (11, 33)"),
        running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "down Route 6's south path into Vermilion City, to the Pokemon Center")),
        heal("heal_vermilion"),
    });
}

//  The S.S. Anne (VermilionCity, SSAnne_* scripts.inc). The step onto
//  (23, 33) (VermilionCity_EventScript_CheckTicket) has the sailor (24, 33)
//  ask for a ticket; the S.S. TICKET (00.18) is shown and the player turns
//  east to him. Then onto the pier (23, 34), along the gangway, through
//  1F, by legs off its cabins' doormats on row 10, which planned routes
//  cross, and up its stairs (3, 8) to 2F. The step onto (30, 6), below the
//  Captain's door (30, 2) (SSAnne_2F_Corridor_EventScript_RivalTriggerLeft),
//  brings the rival out and down to (30, 5); the player keeps facing
//  north, and after the battle he walks round the player and off. For a
//  player who took Squirtle: Pidgeotto Lv19, Raticate Lv16, Kadabra Lv18,
//  Ivysaur Lv20. The ship's trainers are in its cabins and on its deck,
//  off the way. The Captain (5, 4) faces up; talked to from (5, 5) he is
//  rubbed on the back and gives HM01 (VAR_MAP_SCENE_VERMILION_CITY 1).
//  Back out through 1F's door (19, 1): on the gangway the ship sails
//  (SSAnne_Exterior_ExitSSAnne), the player is put on (23, 34) and walked
//  up to (23, 32) (VermilionCity_EventScript_ExitSSAnne). Cut is taught
//  later, to the Oddish.
StoryCheckpoint ss_anne(){
    return row("00.22", "The S.S. Anne: the rival, HM01 Cut from the Captain, back to Vermilion's Pokemon Center", at(VERMILION_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("above_ticket_check", at(VERMILION, 23, 32, SOUTH), "out of the Center down to the harbor, above the ticket check (23, 33)")),
        step_into("ticket_check", SOUTH, "onto (23, 33): \"Do you have a ticket?\""),
        scene("show_ss_ticket", at(VERMILION, 23, 33, EAST), "the S.S. TICKET; the player turns to the sailor"),
        running(route("aboard", at(SS_ANNE_1F, 19, 2, SOUTH), "onto the pier, along the gangway and in by 1F's door, a step below it")),
        running(legs("past_the_cabins", {{SOUTH, 8}, {NORTH, 1}, {WEST, 6}, {NORTH, 1}}, "to the wall (19, 10), up a step, west to the worker (12, 9), up a step to (13, 8): off the cabins' mats on row 10")),
        running(route("below_rival_trigger", at(SS_ANNE_2F, 30, 7, NORTH), "west along 1F, up the stairs (3, 8) and along 2F below the Captain's door")),
        step_into("rival_trigger", NORTH, "onto (30, 6): the rival comes out of the Captain's door"),
        battle("rival_ss_anne", at(SS_ANNE_2F, 30, 6, NORTH), false, "Pidgeotto, Raticate, Kadabra and Ivysaur"),
        running(route("behind_captain", at(CAPTAINS_OFFICE, 5, 5, NORTH), "into the Captain's office, behind the Captain (5, 4)")),
        talk("captain", "\"Ooargh... I feel hideous... Urrp! Seasick...\""),
        scene("hm01_cut", at(CAPTAINS_OFFICE, 5, 5, NORTH), "the player rubs his back; HM01, and what Cut does"),
        running(route("off_2f", at(SS_ANNE_1F, 13, 8), "back along 2F, down the stairs and east along 1F's row 8")),
        running(legs("round_the_sailor", {{EAST, 4}, {SOUTH, 1}, {EAST, 2}}, "to the sailor (18, 8), down a step, east to (19, 9): off the cabins' mats on row 10")),
        running(route("below_ship_door", at(SS_ANNE_1F, 19, 2, NORTH), "up below the door to the gangway")),
        step_into("off_the_ship", NORTH, "out onto the gangway"),
        scene("ss_anne_sails", at(VERMILION, 23, 32, NORTH), "the ship sails; the player is walked off the pier to (23, 32)"),
        running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "back to the Pokemon Center")),
        heal("heal_vermilion"),
    });
}


//  HM01 Cut, from the S.S. Anne (00.22), to the Oddish (00.20), second in
//  the party: a wild Oddish of Lv12 to 14 knows 2 or 3 moves (Absorb,
//  Sweet Scent, Poison Powder at 14), so it has room. TM28 Dig, from the
//  Cerulean Rocket (00.21), to the lead in place of Withdraw: Ground, twice
//  as strong on Lt. Surge's three, it takes the sim's chance against him
//  for a Lv28 Wartortle with 14 Potions from 6% to 47%. Vermilion's tree
//  (19, 24) (EventScript_CutTree) is the only way into the gym's yard,
//  row 24 being walls but for it; the gym's door is (14, 25). In the gym,
//  the trainers as above, then the trash cans until the door opens; out,
//  the tree cut again from below, and back to the nurse.
StoryCheckpoint vermilion_gym(){
    const TeachPlan cut{.item = "hm01", .move = "cut", .member = 1};
    const TeachPlan dig{.item = "tm28", .move = "dig", .member = 0, .forget_move = "withdraw"};
    return row("00.23", "Vermilion Gym: Cut and Dig, the tree, Dwayne, Baily and Tucker, the trash cans", at(VERMILION_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        teach_step("teach_cut", "HM01 Cut to the Oddish, second in the party, unless it knows it", cut),
        teach_step("teach_dig", "TM28 Dig in place of Withdraw, for Lt. Surge, unless the lead knows it or there is no TM28", dig),
        running(route("above_gym_tree", at(VERMILION, 19, 23, SOUTH), "out of the Center, south through the city to above the tree (19, 24)")),
        cut_tree("cut_gym_tree", at(VERMILION, 19, 24, SOUTH), "the Oddish cuts the tree; a step onto its tile"),
        running(route("gym_door", at(VERMILION_GYM, 5, 19, NORTH), "down into the yard and in by the gym's door (14, 25)")),
        running(legs("west_of_dwayne_line", {{NORTH, 4}, {EAST, 2}}, "up column 5 to the can (5, 14), east to (7, 15): out of Dwayne's and Baily's sight")),
        into_sight("dwayne_sight", EAST, "onto (8, 15), two below Dwayne (8, 13): he turns to the player and walks up to (8, 14)"),
        battle("sailor_dwayne", at(VERMILION_GYM, 8, 15, NORTH), false, "two Pikachu, Lv21"),
        running(legs("east_of_baily_line", {{WEST, 4}, {NORTH, 2}, {WEST, 1}}, "west along row 15 to (4, 15), up to (4, 13), west to (3, 13): out of Baily's sight")),
        into_sight("baily_sight", WEST, "onto (2, 13), two below Baily (2, 11): he turns to the player and walks up to (2, 12)"),
        battle("engineer_baily", at(VERMILION_GYM, 2, 13, NORTH), false, "Voltorb and Magnemite, Lv21"),
        running(legs("below_tucker_line", {{EAST, 2}, {NORTH, 4}, {EAST, 1}}, "east to (4, 13), up column 4 past Baily's row to (4, 9), east to (5, 9): below Tucker's line, not on it")),
        into_line("tucker_line", NORTH, "onto (5, 8), in Tucker's line: he walks up to (6, 8)"),
        battle("gentleman_tucker", at(VERMILION_GYM, 5, 8, EAST), false, "Pikachu, Lv23"),
        trash_cans("trash_cans", "the cans until both locks open; a wrong second can resets them"),
        running(route("below_gym_tree", at(VERMILION, 19, 25, NORTH), "out of the gym, east to below the tree, grown back")),
        cut_tree("cut_tree_out", at(VERMILION, 19, 24, NORTH), "the tree again; a step onto its tile"),
        running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "north through the city to the Pokemon Center")),
        heal("heal_vermilion"),
    });
}

//  Wild battles on Route 6 until the sim beats Lt. Surge half the time:
//  with Dig, 47% at Lv28 and 51% at Lv29, so a lead the gym's trainers
//  have brought to Lv29 passes at once, and a Lv28 one after a level. The grass nearest the city is Route 6's
//  south patch (13..16, 30..33); Picnicker Isabelle (13, 32) faces west
//  over (10..12, 32), on the way north from the city, so the walk goes
//  along Jeff's row 33 (he is beaten) to the wall (7, 33), up to row 31
//  and east along it onto the patch's west edge, (13, 31), above
//  Isabelle, out of her line. Back to the nurse the same way.
//  All that is skipped when the lead, read in the Center, passes already.
//  Then the tree, the gym, whose trainers stand where the map has them,
//  beaten, and whose beams stay off (FLAG_FOUND_BOTH_VERMILION_GYM_SWITCHES):
//  up column 5 to below Lt. Surge, and saved there.
StoryCheckpoint to_lt_surge(){
    constexpr const char* SURGE_WINNABLE = "surge_winnable";
    const GrindPlan for_surge{
        .member = 0, .level = 32, .max_encounters = 150, .pace = EAST, .heal_every = 0,
        .trainer = std::string(story_trainer(VERMILION_GYM, "lt_surge").value_or("")), .min_chance = MIN_WIN_CHANCE,
    };
    return row("00.24", "Route 6's grass until Lt. Surge is winnable, then through the open beams to him", at(VERMILION_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        grind_check_step(SURGE_WINNABLE, "the lead as it is: if the sim beats Lt. Surge 50% or more already, no trip to Route 6", for_surge),
        unless(SURGE_WINNABLE, running(route("route6_south_path", at(ROUTE6, 11, 34, NORTH), "out of the Center, north out of the city onto Route 6's path, below Jeff's row"))),
        unless(SURGE_WINNABLE, running(legs("round_isabelle_line", {{NORTH, 1}, {WEST, 4}, {NORTH, 2}, {EAST, 5}, {EAST, 1}}, "onto Jeff's row (11, 33), west to the wall (7, 33), up to (7, 31), east along row 31 to (12, 31) and a step onto the grass, (13, 31): out of Isabelle's line"))),
        unless(SURGE_WINNABLE, grind_step("grind_for_surge", "wild battles until the sim beats Lt. Surge 50% or more, Lv32 at most; the nurse when hurt or low on PP", for_surge, at(VERMILION_CENTER, 7, 4, NORTH))),
        unless(SURGE_WINNABLE, running(route("west_on_row_31", at(ROUTE6, 7, 31), "west along row 31, out of the grass, to the wall (7, 31)"))),
        unless(SURGE_WINNABLE, running(legs("down_to_jeff_row", {{SOUTH, 2}, {EAST, 4}}, "down to the wall (7, 33), east along Jeff's row to (11, 33): out of Isabelle's line"))),
        unless(SURGE_WINNABLE, running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "south into the city to the Pokemon Center"))),
        unless(SURGE_WINNABLE, heal("heal_vermilion")),
        running(route("above_gym_tree", at(VERMILION, 19, 23, SOUTH), "out of the Center, south through the city to above the tree (19, 24)")),
        cut_tree("cut_gym_tree", at(VERMILION, 19, 24, SOUTH), "the tree; a step onto its tile"),
        running(route("below_lt_surge", at(VERMILION_GYM, 5, 3, NORTH), "into the gym, up round the cans and through the beams, off, to below Surge (5, 2)")),
    });
}

//  Lt. Surge (5, 2), talked to from below, from the save in front of him:
//  Voltorb Lv21, Pikachu Lv18 and Raichu Lv24, Shock Wave and Thunder
//  Wave, a Super Potion and a Full Heal (sParty_LeaderLtSurge); then the
//  THUNDERBADGE and TM34, read to the overworld with the battle's text.
//  Out, the tree, and the nurse. Surge's win also puts Oak's aide on
//  Vermilion City's (25, 7) (VermilionCity_Gym_EventScript_ShowOaksAide),
//  where he stays until talked to; the story's maps don't have him. The
//  way to the nurse along row 7 runs into him (console B, 2026-11-03: the
//  leg west stopped beside him and the step into the Center's door went
//  into a house), so it goes along row 10 to the water instead, clear of
//  him, and up column 14 from there.
StoryCheckpoint lt_surge(){
    return row("00.25", "Vermilion Gym: Lt. Surge, the THUNDERBADGE, then the Center", at(VERMILION_GYM, 5, 3, NORTH), after_brock(), StartMenu::FULL, {
        talk("challenge_lt_surge", "\"Hey, kid! What do you think you're doing here?\""),
        battle("lt_surge", at(VERMILION_GYM, 5, 3, NORTH), false, "Voltorb, Pikachu and Raichu; THUNDERBADGE and TM34"),
        running(route("below_gym_tree", at(VERMILION, 19, 25, NORTH), "down through the beams, out of the gym, east to below the tree, grown back")),
        cut_tree("cut_tree_out", at(VERMILION, 19, 24, NORTH), "the tree; a step onto its tile"),
        running(route("below_oaks_aide", at(VERMILION, 14, 10), "north through the city and west along row 10 to the water, (14, 10): off Oak's aide's row 7")),
        running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "up to the Pokemon Center")),
        heal("heal_vermilion"),
    });
}


std::vector<StoryCheckpoint> cpp_rows(){
    return {
        new_game(), after_sid_helper(), leave_house(), oaks_trigger(), to_starter_ball(), starter(), parcel(), pokedex(), shopping(), forest(), brock(),
        running_shoes(), route3(), magikarp_salesman(), after_gift_rng(), magikarp_to_box(), mt_moon(),
        route4_to_cerulean(), cerulean_gym(), misty(), cerulean_grind(), cerulean_rival(), nugget_bridge(),
        route25(), bill(), back_to_cerulean(), oddish(), to_vermilion(), ss_anne(),
        vermilion_gym(), to_lt_surge(), lt_surge(),
    };
}

//  Why `loaded` differs from `row`, or empty. Steps compare as their
//  shared JSON, which writes every field.
std::string checkpoint_difference(const StoryCheckpoint& row, const StoryCheckpoint& loaded){
    const std::string row_json = Pokemon::story_checkpoint_json(row).dump();
    const std::string loaded_json = Pokemon::story_checkpoint_json(loaded).dump();
    if (row_json != loaded_json){
        return std::format("{}: the file loads to\n{}\nbut the C++ row is\n{}", row.id, loaded_json, row_json);
    }
    if (row.handoff != loaded.handoff || row.live_start != loaded.live_start){
        return row.id + ": a different handoff or live start.";
    }
    for (size_t c = 0; c < row.steps.size() && c < loaded.steps.size(); c++){
        if (row.steps[c].once_per_save != loaded.steps[c].once_per_save){
            return row.id + "/" + row.steps[c].name + ": a different once_per_save.";
        }
    }
    if (row.menu != loaded.menu){
        return row.id + ": a different START menu.";
    }
    const Supplies& a = row.supplies;
    const Supplies& b = loaded.supplies;
    if (a.potions != b.potions || a.antidotes != b.antidotes || a.parlyz_heals != b.parlyz_heals){
        return row.id + ": different supplies.";
    }
    if (a.lead.species != b.lead.species || a.lead.level != b.lead.level || a.lead.moves.size() != b.lead.moves.size()){
        return row.id + ": a different lead.";
    }
    for (size_t c = 0; c < a.lead.moves.size(); c++){
        if (a.lead.moves[c].move != b.lead.moves[c].move || a.lead.moves[c].pp != b.lead.moves[c].pp){
            return row.id + ": a different lead move " + std::to_string(c + 1) + ".";
        }
    }
    return "";
}

class Test_AutoStoryTableParity : public UnitTest{
public:
    Test_AutoStoryTableParity()
        : UnitTest("PokemonFRLG::AutoStory - the story files load to the C++ rows")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            const std::vector<StoryCheckpoint> rows = cpp_rows();
            const std::vector<StoryCheckpoint>& loaded = story_checkpoints();
            if (rows.size() != loaded.size()){
                return std::format("{} C++ rows, {} files.", rows.size(), loaded.size());
            }
            for (size_t c = 0; c < rows.size(); c++){
                const std::string difference = checkpoint_difference(rows[c], loaded[c]);
                if (!difference.empty()){
                    return difference;
                }
            }
            return true;
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }
};

} // namespace


void add_tests_AutoStoryTableParity(UnitTestDatabase& database){
    database.add<Test_AutoStoryTableParity>();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

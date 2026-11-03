/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryChoice.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"
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
const char* const ROUTE4 = "Route4";
const char* const ROUTE4_CENTER = "Route4_PokemonCenter_1F";
const char* const MT_MOON_1F = "MtMoon_1F";
const char* const MT_MOON_B2F = "MtMoon_B2F";
const char* const CERULEAN = "CeruleanCity";
const char* const CERULEAN_CENTER = "CeruleanCity_PokemonCenter_1F";
const char* const ROUTE24 = "Route24";

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

//  At the rows after Brock: no items counted, the starter with the moves
//  the story gave it by Lv13. A run that loads a save reads the moves off
//  the lead's summary instead (read_lead_moves()), so a save whose moves
//  differ from these (console B, 2026-11-02: Water Gun where Tackle was)
//  is fought with the moves it has.
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
//  east end (x 107; Route 4's row 3 is the city's row 13). Lass Crissy
//  (75, 3) faces east, sight 4, over row 3 from x 76 to 79; the way east
//  keeps out of her line and off the grass south of the road (75..87,
//  11..17).
StoryCheckpoint route4_to_cerulean(){
    return row("00.14", "Route 4 east to Cerulean's Pokemon Center", at(ROUTE4, 32, 6, SOUTH), after_brock(), StartMenu::FULL, {
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "east over Route 4's ledges into Cerulean City and its Pokemon Center")),
        heal("heal_cerulean"),
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
//  Sleep Powder are the danger.
StoryCheckpoint cerulean_rival(){
    return row("00.15", "Cerulean: the rival north of the city", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("below_rival_trigger", at(CERULEAN, 23, 7, NORTH), "out of the Center, north up to (23, 7), short of the trigger row 6")),
        step_into("rival_trigger", NORTH, "onto (23, 6), the middle trigger: \"Yo! <player>!\""),
        battle("rival_cerulean", at(CERULEAN, 23, 6, SOUTH), false, "Pidgeotto, Abra, Rattata and Bulbasaur; then the FAME CHECKER"),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "back to the Center before Nugget Bridge")),
        heal("heal_cerulean"),
    });
}

//  Nugget Bridge (Route24 map.json) is x 10..12 from row 32 north to row
//  16, the city's columns 22..24 (offset 12). Its five trainers stand on
//  it facing across, sight 2, so each line crosses the middle column 11
//  on the trainer's row: Bug Catcher Cale (12, 31), Lass Ali (10, 28),
//  Youngster Timmy (12, 25), Lass Reli (10, 22) and Camper Ethan (12, 19).
//  Each is fought on the step onto their row, beside them: they don't
//  walk, and the player keeps facing north. Two tiles separate the rows,
//  walked as one short leg. Then the Rocket (12, 15): the step onto
//  (11, 15) (Route24_EventScript_RocketTriggerRight, VAR_MAP_SCENE_ROUTE24
//  0) gives the NUGGET, "join Team Rocket", and his battle; he only turns,
//  and the player turns east to him. Camper Shane (5, 21) looks north
//  from the west bank, off the way.
StoryCheckpoint nugget_bridge(){
    return row("00.16", "Nugget Bridge: five trainers and the Rocket", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("below_cale_line", at(ROUTE24, 11, 32, NORTH), "out of the Center, north past the rival's row onto the bridge, below Cale's row")),
        step_into("cale_line", NORTH, "onto (11, 31), beside Cale"),
        battle("bug_catcher_cale", at(ROUTE24, 11, 31, NORTH), false, "his Bug Pokemon"),
        running(legs("below_ali_line", {{NORTH, 2}}, "up to (11, 29)")),
        step_into("ali_line", NORTH, "onto (11, 28), beside Ali"),
        battle("lass_ali", at(ROUTE24, 11, 28, NORTH), false, "her Pokemon"),
        running(legs("below_timmy_line", {{NORTH, 2}}, "up to (11, 26)")),
        step_into("timmy_line", NORTH, "onto (11, 25), beside Timmy"),
        battle("youngster_timmy", at(ROUTE24, 11, 25, NORTH), false, "his Pokemon"),
        running(legs("below_reli_line", {{NORTH, 2}}, "up to (11, 23)")),
        step_into("reli_line", NORTH, "onto (11, 22), beside Reli"),
        battle("lass_reli", at(ROUTE24, 11, 22, NORTH), false, "her Pokemon"),
        running(legs("below_ethan_line", {{NORTH, 2}}, "up to (11, 20)")),
        step_into("ethan_line", NORTH, "onto (11, 19), beside Ethan"),
        battle("camper_ethan", at(ROUTE24, 11, 19, NORTH), false, "his Pokemon"),
        //  A route, not legs: the nurse may have been needed.
        ready("ready_for_rocket", at(CERULEAN_CENTER, 7, 4, NORTH), "items for a status or low HP; the nurse only if they can't"),
        running(route("below_rocket_trigger", at(ROUTE24, 11, 16, NORTH), "up the bridge to below the Rocket's trigger (11, 15)")),
        step_into("rocket_trigger", NORTH, "onto (11, 15), beside the Rocket"),
        battle("rocket_grunt", at(ROUTE24, 11, 15, EAST), false, "the NUGGET, then Ekans and Zubat, Lv15"),
    });
}

}


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints{
        new_game(), after_sid_helper(), leave_house(), oaks_trigger(), to_starter_ball(), starter(), parcel(), pokedex(), shopping(), forest(), brock(),
        running_shoes(), route3(), magikarp_salesman(), after_gift_rng(), magikarp_to_box(), mt_moon(),
        route4_to_cerulean(), cerulean_rival(), nugget_bridge(),
    };
    return checkpoints;
}



}
}
}

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
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryTable.h"

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
    });
}


std::vector<StoryCheckpoint> cpp_rows(){
    return {
        new_game(), after_sid_helper(), leave_house(), oaks_trigger(), to_starter_ball(), starter(), parcel(), pokedex(), shopping(), forest(), brock(),
        running_shoes(),
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

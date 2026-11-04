/*  AutoStory Table: Vermilion Gym
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 00.23 to 00.25: HM01 Cut to the Oddish, the tree before Vermilion
 *  Gym, its three trainers and its trash cans, then Lt. Surge from a save
 *  in front of him.
 *
 *  The gym (VermilionCity_Gym map.json, scripts.inc), 11 x 20, entered on
 *  (5, 19): the cans on rows 10, 12 and 14 at x 1, 3, 5, 7, 9
 *  (PokemonFRLG_AutoStoryTrashCans.h), the beams across rows 6 and 7 that
 *  the two switches turn off, Lt. Surge (5, 2) facing down above them. Its
 *  trainers, sight 3:
 *  - Sailor Dwayne (8, 13) and Engineer Baily (2, 11) look around
 *    (MOVEMENT_TYPE_LOOK_AROUND): between them they see across rows 11 and
 *    13 and down columns 2 and 8, so no way from the door to the cans
 *    keeps out of their sight. Each is fought from the second tile of his
 *    line, stepped onto from beside it, out of sight, and waited on there
 *    until he turns to the player (TRAINER_WAIT_VARIANT): Dwayne from
 *    (8, 15), stepped onto from (7, 15), Baily from (2, 13), from (3, 13).
 *    Each walks up a tile, Dwayne to (8, 14), Baily to (2, 12), and stays
 *    there while the player is in the gym.
 *  - Gentleman Tucker (7, 8) faces west over (4..6, 8): fought on the step
 *    up from (5, 9) onto (5, 8); he walks up to (6, 8).
 *  The cans' stands, on rows 9, 11 and 13 above each, are clear of where
 *  the three stand after their battles. Leaving the gym puts them back
 *  where the map has them, beaten, and grows the tree back.
 *
 */

#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


namespace{

//  The search for the switches (PokemonFRLG_AutoStoryTrashCans.h). It
//  ends at the last can's stand, which the step's Outcome says.
StoryStep trash_cans(std::string name, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why));
    step.variant = TRASH_CANS_VARIANT;
    return step;
}

//  `step`, skipped when the run flag `flag` is set.
StoryStep unless(const char* flag, StoryStep step){
    step.skip_if.emplace_back(flag);
    return step;
}

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
//  Out, the tree, and the nurse.
StoryCheckpoint lt_surge(){
    return row("00.25", "Vermilion Gym: Lt. Surge, the THUNDERBADGE, then the Center", at(VERMILION_GYM, 5, 3, NORTH), after_brock(), StartMenu::FULL, {
        talk("challenge_lt_surge", "\"Hey, kid! What do you think you're doing here?\""),
        battle("lt_surge", at(VERMILION_GYM, 5, 3, NORTH), false, "Voltorb, Pikachu and Raichu; THUNDERBADGE and TM34"),
        running(route("below_gym_tree", at(VERMILION, 19, 25, NORTH), "down through the beams, out of the gym, east to below the tree, grown back")),
        cut_tree("cut_tree_out", at(VERMILION, 19, 24, NORTH), "the tree; a step onto its tile"),
        running(route("vermilion_nurse", at(VERMILION_CENTER, 7, 4, NORTH), "north through the city to the Pokemon Center")),
        heal("heal_vermilion"),
    });
}


}
}
}
}

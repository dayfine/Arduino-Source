/*  AutoStory Table: to Celadon
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows from 00.26 on, after the THUNDERBADGE: the BICYCLE, and the way
 *  east and south to Celadon City.
 *
 */

#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


//  The BIKE VOUCHER and the BICYCLE, from Vermilion's Center to
//  Cerulean's (pokeguy 43:50 to 45:00).
//  - The Pokemon Fan Club (VermilionCity_PokemonFanClub map.json,
//    scripts.inc), in by Vermilion's (12, 17): the chairman (5, 4) sits
//    above a table (5..6, 5..6) with the worker (6, 4) east of him, so he
//    is talked to from (4, 4), west of him. "Did you come to hear about
//    my POKEMON?" YES: his story, then the BIKE VOUCHER
//    (FLAG_GOT_BIKE_VOUCHER). Oak's aide stands on Vermilion's (25, 7)
//    since Lt. Surge, where the story's maps don't have him: the way
//    planned from the Center's door goes east along his row 7, so the
//    walk goes down to the water (14, 10) first, as 00.25 came up from
//    it. From the Fan Club, up to (22, 10) first, so that the way on to
//    Route 6 goes up column 22, west of him, not round by column 27.
//  - Route 6 north, the trainers as 00.21 left them: Jeff (13, 33)
//    beaten, Isabelle (13, 32) and Elijah (20, 25) not. As in 00.24, onto
//    Jeff's row, west to the wall (7, 33) and up to row 31, above
//    Isabelle's line; then east along row 31 to the wall (17, 31) and up
//    column 16, west of Elijah's line (17..19, 25) and east of Keigo's
//    (4..8, 16), to the ledge (16, 11). The first three legs are 00.24's,
//    walked on console B; the two after end on walls, which the walker
//    holds again after a wild battle in the grass.
//  - The Underground Path north, Route 5's sides, which have no trainer,
//    and Cerulean's south edge, whose way into the city without the
//    burgled house is the tree (26, 32) (pokeguy 44:42).
//  - The Bike Shop (CeruleanCity_BikeShop), in by (13, 28): the clerk
//    (9, 3) is across the counter (8, 3), talked to from (7, 3). With the
//    voucher he gives the BICYCLE (FLAG_GOT_BICYCLE) with no YES/NO; it
//    isn't registered or ridden yet.
//  - The Mart: 5 Repels (row 9, 350 each) and 2 Escape Ropes (row 8,
//    550 each; pokefirered items.json), 2850 in all. Console B had about
//    9000 after Lt. Surge (2026-11-03); the floor of 6000 leaves 3000 or
//    more after them.
//  - The nurse, and the save in Cerulean's Center.
StoryCheckpoint bike_voucher(){
    const MartPlan repels{
        .map = CERULEAN_MART,
        .lines = {{.item = "repel", .quantity = 5}, {.item = "escape-rope", .quantity = 2}},
        .budget = 2850,
        .money_floor = 6000,
    };
    return row("00.26", "The BIKE VOUCHER from the Fan Club, back north to Cerulean, the BICYCLE, Repels and Escape Ropes", at(VERMILION_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("below_center", at(VERMILION, 14, 10), "out of the Center, down to the water (14, 10): off Oak's aide's row 7")),
        running(route("beside_chairman", at(FAN_CLUB, 4, 4, EAST), "east along row 10, down to the Fan Club's door (12, 17), in, and up its west side beside the chairman (5, 4)")),
        talk("chairman", "\"I chair the POKEMON Fan Club!\" ... \"Did you come to hear about my POKEMON?\""),
        answer("hear_story", true, "YES: his story"),
        scene("bike_voucher", at(FAN_CLUB, 4, 4, EAST), "the story of his Rapidash; the BIKE VOUCHER"),
        running(route("west_of_oaks_aide", at(VERMILION, 22, 10), "out of the Fan Club, east along row 18 and up column 22 to (22, 10)")),
        running(route("route6_south_path", at(ROUTE6, 11, 34, NORTH), "up column 22 west of Oak's aide (25, 7), out of the city onto Route 6's path, below Jeff's row")),
        running(legs("round_isabelle_and_elijah", {{NORTH, 1}, {WEST, 4}, {NORTH, 2}, {EAST, 9}, {NORTH, 19}}, "onto Jeff's row (11, 33), west to the wall (7, 33), up to (7, 31), east along row 31 to the wall (16, 31), up column 16 to the ledge (16, 12): out of Isabelle's and Elijah's lines")),
        running(route("below_cerulean_tree", at(CERULEAN, 26, 33, NORTH), "the Underground Path north, up Route 5's side into Cerulean, below the tree (26, 32)")),
        cut_tree("cut_cerulean_tree", at(CERULEAN, 26, 32, NORTH), "the tree; a step onto its tile"),
        running(route("bike_clerk", at(BIKE_SHOP, 7, 3, EAST), "west into the Bike Shop (13, 28), across the counter from the clerk (9, 3)")),
        talk("talk_bike_clerk", "\"Oh, that's... A BIKE VOUCHER!\""),
        scene("bicycle", at(BIKE_SHOP, 7, 3, EAST), "the voucher exchanged for a BICYCLE"),
        running(route("mart_clerk", at(CERULEAN_MART, 4, 3, WEST), "out of the Bike Shop to the Mart's clerk")),
        mart_step("buy_repels", "5 Repels and 2 Escape Ropes, 2850 at most", repels),
        running(route("cerulean_nurse", at(CERULEAN_CENTER, 7, 4, NORTH), "out of the Mart to the Center")),
        heal("heal_cerulean"),
    });
}

//  East out of Cerulean over Route 9 to Route 10's Pokemon Center
//  (pokeguy 45:12 to 47:27), on foot: the Bicycle isn't registered yet.
//  - Cerulean (CeruleanCity map.json): the way east, column 39..40 up
//    from row 33 to rows 18..19 and out of the city's east edge, is
//    reached from the city only through the tree (26, 32), grown back
//    since 00.26 (its flag is a FLAG_TEMP), or the burgled house. Cut
//    from above. Route 9 joins the city's rows 18..19 by its rows 8..9
//    (offset -10); its tree (2, 8) (FLAG_TEMP_12) closes them, (2, 9)
//    being rock: cut from (1, 8), west of it.
//  - Route 9 (Route9 map.json, scripts.inc), its ledges all facing south.
//    Of its nine trainers, three can't be walked around, as in pokeguy's
//    run, where Jeremy happened to look away:
//    - Picnicker Alicia (14, 10) faces west, sight 3, over (11..13, 10),
//      the only tiles above the ledge row 11 the way west goes down by:
//      fought on the step onto (11, 10); she walks up to (12, 10). Then
//      down the ledge (11, 11).
//    - Hiker Jeremy (20, 17) looks north and west (FACE_UP_AND_LEFT),
//      sight 4: north over (20, 13..16), every way east of column 19
//      below the rocks of row 12. Fought on (20, 13), waiting for him to
//      look north (TRAINER_WAIT_VARIANT); he walks up to (20, 14).
//    - Bug Catcher Conner (49, 7) faces east, sight 3, over (50..52, 7):
//      the way north from row 13 is column 52 alone (the ledges of row
//      11 face south), and row 7 is the way onto the ridge. Fought on the
//      step onto (52, 7); he walks up to (51, 7).
//    - Hiker Brice wanders row 2 (50..56), looking west and east, sight
//      1, over (49..57, 2), and the way up from the ledges of row 4 is
//      (50..51, 4) below him. The leg up column 51 ends on row 3 if he
//      stands at (51, 2), else it goes on to (51, 1) past him, and he may
//      see the player there; either way the legs to walls after it meet
//      on (48, 1): west to the rock (48, 3) or (48, 1), up to (48, 1).
//      Row 1 is clear of his sight; east along it to the rock (64, 1)
//      and down column 64 past the hidden Rare Candy (63, 2), not picked
//      up. A held leg down to (64, 3) would hop the ledge (64, 4) and go
//      on, so the last tile is one step, then the hop.
//    - Picnicker Caitlin (60, 9) faces east over (61..64, 9): the way to
//      Route 10 goes along row 8, above her line, by legs to (70, 8), a
//      tile short of the edge. Camper Chris (26, 7),
//      Bug Catcher Brent (26, 1), Camper Drew (36, 6) and Hiker Alan (61,
//      16) are off the way; TM40 (12, 17) is left.
//  - Route 10 (Route10 map.json) joins Route 9's rows 8..10 by its own;
//    its north part, down to the Center's door (13, 20), has no trainer
//    on the way and its grass is walked round. The trees west of the
//    door (10, 20..26) stay. The walk there ends its open legs where a
//    tile of error can't reach a blocker, and finds its place again on
//    walls (console B, 2026-11-09, two runs "no fade" at the door): the
//    step onto Route 10 is not always held up, so the player may stand a
//    tile further east, and the leg along row 11 holds to the wall
//    (17, 11) instead of ending on (16, 11) by count. The 12 tiles down
//    column 16 may end a tile short or long: from rows 22..29 a tile west
//    and up to the Center's wall (15, 20) all end on (15, 21). West along
//    row 21 to the wall (9, 21) ends on (10, 21), three west of the
//    door's column: the only count no wall after it checks.
//  - The nurse, and the save in Route 10's Center: the save card names
//    ROUTE 10.
StoryCheckpoint route10_center(){
    return row("00.27", "East over Route 9 to Route 10's Pokemon Center: the two trees, Alicia, Jeremy and Conner", at(CERULEAN_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("above_cerulean_tree", at(CERULEAN, 26, 31, SOUTH), "out of the Center, south through the city to above the tree (26, 32), grown back")),
        cut_tree("cut_cerulean_tree", at(CERULEAN, 26, 32, SOUTH), "the tree; a step onto its tile"),
        running(route("west_of_route9_tree", at(ROUTE9, 1, 8, EAST), "down to row 33, east and up column 39 or 40 to row 18, east out of the city onto Route 9, west of its tree (2, 8)")),
        cut_tree("cut_route9_tree", at(ROUTE9, 2, 8, EAST), "Route 9's tree; a step onto its tile"),
        running(route("above_alicia_line", at(ROUTE9, 11, 9, SOUTH), "east to (11, 9), above the west end of Alicia's line")),
        into_line("alicia_line", SOUTH, "onto (11, 10), in Alicia's line: she walks up to (12, 10)"),
        battle("picnicker_alicia", at(ROUTE9, 11, 10, EAST), false, "Oddish, Bellsprout, Oddish and Bellsprout, Lv18"),
        hop("alicia_ledge_hop", SOUTH, at(ROUTE9, 11, 12, SOUTH), "down the ledge (11, 11)"),
        running(route("west_of_jeremy_sight", at(ROUTE9, 19, 13, EAST), "down to row 13 and east along it to (19, 13), west of Jeremy's north line")),
        into_sight("jeremy_sight", EAST, "onto (20, 13), four above Jeremy (20, 17): when he looks north he walks up to (20, 14)"),
        battle("hiker_jeremy", at(ROUTE9, 20, 13, SOUTH), false, "Machop and Onix, Lv20"),
        running(route("below_conner_line", at(ROUTE9, 52, 8, NORTH), "east along row 13 and up column 52 to (52, 8), below the east end of Conner's line")),
        into_line("conner_line", NORTH, "onto (52, 7), the end of Conner's line: he walks up to (51, 7)"),
        battle("bug_catcher_conner", at(ROUTE9, 52, 7, WEST), false, "Caterpie, Weedle and Venonat, Lv20"),
        running(legs("round_brice", {{NORTH, 2}, {WEST, 1}, {NORTH, 2}, {WEST, 3}, {NORTH, 2}, {EAST, 16}, {SOUTH, 1}}, "up to the ledge (52, 4), up column 51 to Brice's row, west to the rock (48, 3) (or (48, 1)), up to (48, 1), east along row 1 to the rock (64, 1), down to (64, 2)")),
        step_onto("above_east_ledge", SOUTH, at(ROUTE9, 64, 3, SOUTH), "onto (64, 3), above the ledge (64, 4)"),
        hop("east_ledge_hop", SOUTH, at(ROUTE9, 64, 5, SOUTH), "down the ledge (64, 4)"),
        running(legs("above_caitlin_line", {{SOUTH, 3}, {EAST, 6}}, "down column 64 to row 8, east along it to (70, 8): above Caitlin's line (61..64, 9)")),
        running(route("route10_ledge_end", at(ROUTE10, 17, 11, EAST), "east onto Route 10, down to row 11 west of the grass and east along it to the wall (17, 11), above the fence post (17, 12)")),
        running(legs("below_center_door", {{WEST, 1}, {SOUTH, 12}, {WEST, 1}, {NORTH, 2}, {WEST, 5}, {EAST, 3}}, "into the ledge's gap (16, 11), down column 16 to row 23, a tile west, up to the Center's wall (15, 21), west along row 21 to the wall (10, 21), east to (13, 21), below the door")),
        running(route("route10_nurse", at(ROUTE10_CENTER, 7, 4, NORTH), "up onto the Center's door (13, 20), to the nurse")),
        heal("heal_route10"),
    });
}


} // namespace StoryTable
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

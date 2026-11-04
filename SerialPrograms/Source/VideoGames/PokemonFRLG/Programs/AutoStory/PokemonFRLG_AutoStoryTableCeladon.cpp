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


}
}
}
}

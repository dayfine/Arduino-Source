/*  AutoStory Table: to Lavender Town
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows from 00.28 on: through Rock Tunnel, without Flash, and down Route
 *  10 to Lavender Town's Pokemon Center.
 *
 */

#include "PokemonFRLG_AutoStoryTableSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


//  From Route 10's Pokemon Center through Rock Tunnel (pokefirered
//  RockTunnel_1F, RockTunnel_B1F map.json and scripts.inc) to Route 10
//  south of it. The save is on Route 10 by the tunnel's south mouth: the
//  save card's places don't include Rock Tunnel.
//  - The tunnel's north mouth (8, 19) is reached from the Center through
//    the tree (10, 20), cut from below; Heidi (9, 27) sees rows 28..29
//    below the trees and is left. Two single steps west then put the
//    player below the mouth on (8, 20).
//  - Every floor tile of the tunnel has wild encounters, which are run
//    from (the table has no step that uses a Repel), so the legs end on
//    walls, ladders or someone standing still, which the walker holds
//    again after a battle. An open leg (two tiles or more, not to a wall)
//    is followed only by legs a tile of error in it can't send astray.
//  - Nine trainers stand where the way can't go round them. The others
//    (1F Lucas, Oliver, Ariana, Leah; B1F Eric, Steve) are walked round.
//    - 1F Ashton (27, 10) looks west over (24..26, 10), the only way south
//      from the mouth's room: stepped into from (24, 9).
//    - B1F Winston (29, 35) looks south over the south passage (29,
//      36..38): stepped into from (32, 37), after the leg down column 33
//      to the wall and a tile back.
//    - B1F Martha (15, 33) looks east over (16..17, 33), the corridor
//      north: stepped into from (17, 34), the wall east of row 34.
//    - B1F Allen (36, 11) looks south over (36, 12..14), the way west to
//      the ladder (33, 3): stepped into from (38, 12), below the wall.
//    - 1F Lenny (7, 5) looks south over (7, 6..9), the way east from the
//      ladder (4, 2): stepped into from (6, 9), the end of column 6.
//      The way on to the ladder (20, 13) keeps west of Lucas's line (16..19,
//      17): down column 11, up column 14 to row 16.
//    - B1F Sofia (11, 15) looks south over (11, 16..19), the way west:
//      stepped into along row 16 from (12..14, 16).
//    - B1F Dudley (5, 11) looks south over (5, 12..15): stepped into from
//      (8, 15) along row 15.
//    - B1F Cooper (2, 6) looks around (sight 2) by the ladder (2, 3): the
//      leg up column 1 to the wall (1, 2) passes him on (1, 6); if he
//      looks west, story_interrupts() fights him and the leg is held on
//      to its wall, as Mt. Moon's Grunt in 00.13.
//    - 1F Dana (26, 28) looks south over (26, 29..31), the only way west
//      to the south mouth: stepped into along row 31 from (27..29, 31).
//  - Unverified on the console: Rock Tunnel is dark without Flash, and
//    the overworld, fade, dialog and battle detectors haven't been
//    measured under its darkness (~/FRLGLab/notes/2026-11-09-0028-dark-
//    measure.md). Every step from rock_tunnel_1f to route10_south depends
//    on them.
StoryCheckpoint rock_tunnel(){
    return row("00.28", "Through Rock Tunnel: Ashton, Winston, Martha, Allen, Lenny, Sofia, Dudley, Cooper and Dana", at(ROUTE10_CENTER, 7, 4, NORTH), after_brock(), StartMenu::FULL, {
        running(route("below_route10_tree", at(ROUTE10, 10, 21, NORTH), "out of the Center (13, 20), west along row 21 to the wall (9, 21), below the tree (10, 20)")),
        cut_tree("cut_route10_tree", at(ROUTE10, 10, 20, NORTH), "the tree; a step onto its tile"),
        step_onto("west_of_tree", WEST, at(ROUTE10, 9, 20, WEST), "a step west along row 20"),
        step_onto("below_rock_tunnel", WEST, at(ROUTE10, 8, 20, WEST), "a step west, below the mouth (8, 19)"),
        running(route("rock_tunnel_1f", at(ROCK_TUNNEL_1F, 17, 2, NORTH), "north into Rock Tunnel")),
        running(legs("above_ashton_line", {{SOUTH, 6}, {EAST, 8}, {SOUTH, 1}, {WEST, 1}}, "down column 17 to the wall (17, 8), east along row 8 to (25, 8), onto row 9 and west to the wall (24, 9)")),
        into_line("ashton_line", SOUTH, "onto (24, 10), in Ashton's line: he walks up to (25, 10)"),
        battle("pokemaniac_ashton", at(ROCK_TUNNEL_1F, 24, 10, EAST), false, "Slowpoke, Slowpoke and Slowpoke"),
        running(route("b1f_south_ladder", at(ROCK_TUNNEL_B1F, 38, 28, NORTH), "east along row 12 and up column 45 to the ladder (45, 2), down to B1F")),
        running(legs("winston_line_end", {{WEST, 5}, {SOUTH, 10}, {NORTH, 1}, {WEST, 1}}, "west to the wall (33, 28), down column 33 to the wall (33, 38), up to (33, 37) and west to (32, 37)")),
        into_line("winston_line", WEST, "along row 37 onto (29, 37), the end of Winston's line: he walks down to (29, 36)"),
        battle("pokemaniac_winston", at(ROCK_TUNNEL_B1F, 29, 37, NORTH), false, "Slowpoke and Slowpoke"),
        running(legs("below_martha_line", {{WEST, 16}, {NORTH, 2}, {EAST, 6}, {WEST, 3}}, "west along row 37 to (13, 37), up to the wall (13, 35), east to the wall (19, 35), west to (16, 35)")),
        into_line("martha_line", NORTH, "up column 16 onto (16, 33), beside Martha; from (17, 35), a tile short, up column 17 onto (17, 33), the end of her line"),
        battle("picnicker_martha", at(ROCK_TUNNEL_B1F, 16, 33, WEST), false, "Oddish and Bulbasaur"),
        running(legs("below_allen_line", {{EAST, 1}, {NORTH, 5}, {EAST, 12}, {NORTH, 9}, {EAST, 11}, {NORTH, 2}, {WEST, 2}, {NORTH, 5}}, "east to the wall (17, 33), up column 17 to the wall (17, 28), east to the wall (29, 28), up column 29 past Steve's line to the wall (29, 19), east to (40, 19), up to row 17, west to the wall (38, 17) and up to the wall (38, 12)")),
        into_line("allen_line", WEST, "along row 12 onto (36, 12), below Allen"),
        battle("hiker_allen", at(ROCK_TUNNEL_B1F, 36, 12, NORTH), false, "Geodude, Geodude and Graveler"),
        running(legs("west_of_b1f_ladder", {{WEST, 4}, {NORTH, 10}}, "west along row 12 to the wall (32, 12), up column 32 to the wall (32, 2)")),
        running(route("1f_west_ladder", at(ROCK_TUNNEL_1F, 4, 2, NORTH), "onto the ladder (33, 3), up to 1F")),
        running(legs("above_lenny_line", {{EAST, 2}, {SOUTH, 7}}, "east to the wall (6, 2), down column 6 to the wall (6, 9)")),
        into_line("lenny_line", EAST, "onto (7, 9), the end of Lenny's line: he walks down to (7, 8)"),
        battle("hiker_lenny", at(ROCK_TUNNEL_1F, 7, 9, NORTH), false, "Machop and Onix"),
        running(legs("west_of_lucas_line", {{EAST, 4}, {SOUTH, 15}, {EAST, 7}, {WEST, 4}, {NORTH, 8}, {EAST, 6}}, "east to the wall (11, 9), down column 11 to the wall (11, 24), east to the wall (18, 24), west to (14, 24), up column 14 to the wall (14, 16), east to the wall (20, 16)")),
        running(route("b1f_middle_ladder", at(ROCK_TUNNEL_B1F, 27, 12, NORTH), "up column 20 onto the ladder (20, 13), down to B1F")),
        running(legs("east_of_sofia_line", {{SOUTH, 3}, {WEST, 12}, {SOUTH, 1}, {WEST, 2}}, "down to the wall (27, 15), west along row 15 to the wall (15, 15), down to row 16 and west to (13, 16)")),
        into_line("sofia_line", WEST, "along row 16 onto (11, 16), below Sofia"),
        battle("picnicker_sofia", at(ROCK_TUNNEL_B1F, 11, 16, NORTH), false, "Pidgey, Pidgey and Pidgeotto"),
        running(legs("east_of_dudley_line", {{WEST, 3}, {NORTH, 1}}, "west to the wall (8, 16), up to (8, 15)")),
        into_line("dudley_line", WEST, "along row 15 onto (5, 15), the end of Dudley's line: he walks down to (5, 14)"),
        battle("hiker_dudley", at(ROCK_TUNNEL_B1F, 5, 15, NORTH), false, "Geodude, Geodude and Graveler"),
        running(legs("past_cooper", {{WEST, 4}, {NORTH, 13}, {SOUTH, 1}}, "west to the wall (1, 15), up column 1 past Cooper (2, 6) to the wall (1, 2), back to (1, 3)")),
        running(route("1f_east_ladder", at(ROCK_TUNNEL_1F, 45, 21, NORTH), "east onto the ladder (2, 3), up to 1F")),
        running(legs("above_dana_line", {
            {WEST, 6}, {SOUTH, 5}, {EAST, 6}, {WEST, 1}, {SOUTH, 4}, {WEST, 5}, {SOUTH, 5},
            {WEST, 2}, {NORTH, 3}, {WEST, 6}, {NORTH, 3}, {WEST, 3}, {SOUTH, 2},
        }, "west to the wall (39, 21), down column 39 to Ariana (39, 27), east to the wall (45, 26), down column 44 to the wall (44, 30), west to the wall (39, 30), down to the wall (39, 35), west to (37, 35), up to the wall (37, 32), west to the wall (31, 32), up to (31, 29), west to (28, 29) and down to the wall (28, 31)")),
        into_line("dana_line", WEST, "along row 31 onto (26, 31), the end of Dana's line: she walks down to (26, 30)"),
        battle("picnicker_dana", at(ROCK_TUNNEL_1F, 26, 31, NORTH), false, "Meowth, Oddish and Pidgey"),
        running(legs("above_south_mouth", {{WEST, 6}, {SOUTH, 1}, {WEST, 8}, {SOUTH, 5}, {EAST, 2}, {NORTH, 1}, {EAST, 5}, {WEST, 1}}, "west along row 31 to (20, 31), onto row 32 and west to the wall (12, 32), down column 12 to the wall (12, 37), east to the wall (14, 37), up to (14, 36), east to the wall (19, 36), back to (18, 36)")),
        running(route("route10_south", at(ROUTE10, 8, 58, SOUTH), "onto the mouth (18, 37), out onto Route 10")),
    });
}

//  Route 10 south of Rock Tunnel (Route10 map.json) to Lavender Town's
//  Pokemon Center. It has no grass. Carol (7, 60) and Clark (4, 62) look
//  around, Trent (4, 68) looks west and Herman wanders row 70 (13..17),
//  looking west and east (sight 2, to x 19): the way keeps to the east
//  side, columns 17..21, and crosses row 70 on column 21. The ledges of
//  row 62 leave a gap at (18, 62); row 66's leaves (21, 66). Row 74's
//  ledge (11, 74) is hopped, and the hold south goes on over the
//  connection into Lavender Town to the wall (11, 9). West of the
//  Center's building, the wall (8, 9) sets the door's column.
StoryCheckpoint lavender_center(){
    return row("00.29", "Route 10 south to Lavender Town's Pokemon Center", at(ROUTE10, 8, 58, SOUTH), after_brock(), StartMenu::FULL, {
        running(legs("route10_east_side", {{EAST, 10}, {NORTH, 2}, {WEST, 1}, {EAST, 1}, {SOUTH, 8}, {EAST, 3}, {SOUTH, 9}, {WEST, 10}}, "east along row 58 to (18, 58), up to the wall (18, 56), west to the wall (17, 56), back to (18, 56), down through the ledges' gap to (18, 64), east to the wall (21, 64), down column 21 to the wall (21, 73), west along row 73 to the wall (11, 73)")),
        hop("route10_ledge_hop", SOUTH, at(ROUTE10, 11, 75, SOUTH), "down the ledge (11, 74)"),
        running(route("lavender_north_wall", at(LAVENDER, 11, 8, SOUTH), "down column 11 over the connection into Lavender Town, to the wall (11, 9)")),
        running(legs("below_center_door", {{WEST, 6}, {SOUTH, 1}, {EAST, 2}, {WEST, 1}}, "west along row 8 to (5, 8), onto row 9, east to the wall (7, 9), west to (6, 9)")),
        running(route("lavender_nurse", at(LAVENDER_CENTER, 7, 4, NORTH), "up column 6 onto the Center's door (6, 5), to the nurse")),
        heal("heal_lavender"),
    });
}


} // namespace StoryTable
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  AutoStory Trash Cans
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Vermilion Gym's door switches (pokefirered data/maps/VermilionCity_Gym/
 *  scripts.inc, src/field_specials.c SetVermilionTrashCans()). Fifteen
 *  cans stand in three rows of five, numbered 1-5 on row 10, 6-10 on row
 *  12 and 11-15 on row 14, at x 1, 3, 5, 7, 9. Each time the player
 *  enters the gym the first switch goes under a can drawn at random, and
 *  the second under one of the cans beside it in the rows (left, right,
 *  above, below), each as likely. A can answers A from any side:
 *
 *  - Before the first switch: "Nope! There's only trash here." (one page),
 *    or "Hey! There's a switch under the trash! Turn it on!" and "The first
 *    electric lock opened!" (two pages).
 *  - After it: "The second electric lock opened! The motorized door
 *    opened!" (one page): the beams across rows 6 and 7 go off for good
 *    (FLAG_FOUND_BOTH_VERMILION_GYM_SWITCHES). Or "Nope! There's only
 *    trash here." and "Hey! The electric locks were reset!" (two pages):
 *    both switches are drawn again, the first anywhere. The can that had
 *    the first switch counts as a wrong one.
 *
 *  So in either state the number of pages tells the two answers apart,
 *  with no text read. Leaving the gym draws the switches again too; a
 *  battle doesn't (the first switch is a FLAG_TEMP, kept until the map
 *  changes).
 *
 *  The search: the cans in a snake along the rows until one has two
 *  pages; then one guess beside it. A wrong guess draws both switches
 *  again, so the search goes on from the can the player stands at, which
 *  is as likely as any to have the new first switch. A guess is right
 *  1 time in 2 to 4 (about 36% over the 15 cans), so the gym opens after
 *  about 3 rounds of about 9 cans; TRASH_CAN_MAX_CHECKS gives up after
 *  about 20 rounds (left 1 time in 10000).
 *
 *  The decisions take their game step as a CheckCan, so they run in tests
 *  against a fake gym; on the console the step walks to the can
 *  and counts its pages (PokemonFRLG_AutoStoryEvents.cpp).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrashCans_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrashCans_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that opens Vermilion Gym's door.
inline const std::string TRASH_CANS_VARIANT = "trash_cans";

//  Total number of trash cans in Vermilion Gym.
inline constexpr int TRASH_CAN_COUNT = 15;
//  Maximum number of trash can inspections before aborting.
inline constexpr size_t TRASH_CAN_MAX_CHECKS = 200;

//  Can `can`'s tile (1-15).
Pokemon::TilePos trash_can_tile(int can);

//  The cans the second switch may be under when the first is under `can`
//  (SetVermilionTrashCans()'s cases).
std::vector<int> trash_can_neighbors(int can);

//  Where the player stands to check `can`, facing it: the row above it.
//  Rows 9, 11 and 13 run the gym's width, clear of the trainers where
//  their battles leave them (PokemonFRLG_AutoStoryTableVermilionGym.cpp).
struct TrashCanStand{
    Pokemon::TilePos tile;
    Pokemon::Direction facing = Pokemon::Direction::SOUTH;
};
//  Position and facing to inspect a trash can.
TrashCanStand trash_can_stand(int can);

//  `maps` with Vermilion Gym's trainers where their battles in 00.23 leave
//  them, beaten (PokemonFRLG_AutoStoryTableVermilionGym.cpp): Sailor
//  Dwayne on (8, 14), Engineer Baily on (2, 12), Gentleman Tucker on
//  (6, 8). The search plans its walks between the cans on these, not on
//  the maps' tiles for them, which are free once they have walked up.
std::vector<Pokemon::MapData> trash_can_maps(const std::vector<Pokemon::MapData>& maps);

//  The order the search checks the cans: along row 10 east, row 12 west,
//  row 14 east.
const std::vector<int>& trash_can_order();


//  What a can's message came to.
enum class CanPages{
    ONE,
    TWO,
    //  The walk to the can or its message failed.
    FAILED,
};

//  The game step the search takes, injected so its decisions run in
//  tests: from the overworld in the gym, to `can`'s stand, A on the can,
//  its message to the overworld.
using CheckCan = std::function<CanPages(int can)>;

//  Termination reason for trash can search.
enum class TrashCanEnd{
    //  The second lock opened.
    OPENED,
    //  TRASH_CAN_MAX_CHECKS checks without it.
    GAVE_UP,
    FAILED,
};
//  String name for trash can search termination reason.
const char* trash_can_end_name(TrashCanEnd end);

//  Result summary of trash can search run.
struct TrashCanResult{
    TrashCanEnd end = TrashCanEnd::FAILED;
    size_t checks = 0;
    //  Wrong guesses: the locks reset.
    size_t resets = 0;
    //  The can checked last, where the player stands.
    int last_can = 0;
};

//  Searches the cans as above until the second lock opens, from the
//  overworld in the gym with neither switch found since the player came
//  in.
TrashCanResult run_trash_cans(Logger& logger, const CheckCan& check, size_t max_checks = TRASH_CAN_MAX_CHECKS);

//  Registers Vermilion Gym trash can puzzle unit tests.
void add_tests_AutoStoryTrashCans(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

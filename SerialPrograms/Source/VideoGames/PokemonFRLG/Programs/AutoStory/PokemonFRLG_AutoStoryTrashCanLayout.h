/*  AutoStory Trash Can Layout
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where Vermilion Gym's fifteen trash cans stand, which are beside each
 *  other, and where the player stands to check one
 *  (PokemonFRLG_AutoStoryTrashCans.h has the search).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrashCanLayout_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrashCanLayout_H

#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that opens Vermilion Gym's door.
inline const std::string TRASH_CANS_VARIANT = "trash_cans";

//  Total number of trash cans in Vermilion Gym.
inline constexpr int TRASH_CAN_COUNT = 15;

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

//  `maps` with Vermilion Gym's trainers where their battles in 06.02 leave
//  them, beaten (PokemonFRLG_AutoStoryTableVermilionGym.cpp): Sailor
//  Dwayne on (8, 14), Engineer Baily on (2, 12), Gentleman Tucker on
//  (6, 8). The search plans its walks between the cans on these, not on
//  the maps' tiles for them, which are free once they have walked up.
std::vector<Pokemon::MapData> trash_can_maps(const std::vector<Pokemon::MapData>& maps);

//  The order the search checks the cans: along row 10 east, row 12 west,
//  row 14 east.
const std::vector<int>& trash_can_order();



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Overworld Bicycle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How the overworld walker rides the Bicycle: which legs it taps out,
 *  and getting on with SELECT.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldBike_H
#define PokemonAutomation_PokemonFRLG_OverworldBike_H

#include <functional>
#include <optional>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The speed walked at instead of `speed` while off the Bicycle (a door
//  took the player off): a run.
MoveSpeed on_foot(MoveSpeed speed);

namespace OverworldWalkSteps{

//  Whether a WALK `step` at `speed` starts with a turn tap, for a player
//  facing `facing`: on foot facing another way (or an unknown way), so the
//  hold starts facing the right way. Never on the Bicycle, where a tap
//  takes a step (MovementMechanics::BIKE_TAP_ONLY_TURNS).
bool turns_first(const RouteStep& step, MoveSpeed speed, std::optional<Direction> facing);

//  Whether `step` at `speed` is walked a tap at a time, each a tile
//  (count_bike_taps()): an open leg on the Bicycle, outdoors, not onto a
//  warp or a spinner, and not over a map connection, where the game holds
//  the player up a tile (a tap there isn't measured). Legs to a wall are
//  held: the wall ends them.
bool counts_bike_taps(const RouteStep& step, MoveSpeed speed);

//  Whether, walking a route at `speed` and riding the Bicycle or not
//  (`riding`), the player gets on with SELECT before `step`: a WALK where
//  the Bicycle may be ridden, after a door took them off.
bool gets_on_bike(const RouteStep& step, MoveSpeed speed, bool riding);

//  Whether a warp onto `map` leaves a rider on the Bicycle: the Cycling
//  Road's gates (allow_cycling in pokefirered's map.json) and the routes
//  their doors open onto. Every other door takes the player off (console
//  B, 2026-11-12 01:05: through Route 16's north gate the player still
//  rode, and a run-paced open leg after it overshot the Fly house's door).
bool warp_keeps_bike(const std::string& map);

//  The console side of select_onto_bike(): presses SELECT and waits it
//  out; reads whether the player rides.
struct BikeSelectCalls{
    std::function<void()> press_select;
    std::function<bool()> riding;
};

//  Gets on the registered Bicycle with SELECT, checks the player rides,
//  and presses again once if not. Returns whether they ride.
bool select_onto_bike(const BikeSelectCalls& calls);

//  Whether the player rides the Bicycle, from the screen: the overworld
//  is up, and no standing player's sprite shows where the player stands
//  (read_player_facing()). False under a message or a menu.
bool screen_shows_riding(const ImageViewRGB32& screen);

//  select_onto_bike() on the console: SELECT is the Switch's MINUS.
//  Throws OperationFailedException if the player doesn't ride after two
//  presses.
void get_on_bike(ConsoleHandle& console, ProControllerContext& context);

} // namespace OverworldWalkSteps


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

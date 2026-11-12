// code-health: allow header_symbols -- story event dispatch table and per-area event population functions
/*  BDSP AutoStory - Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's steps that only BDSP has, by name: a step of the table whose
 *  variant names one of these runs it (StoryExecutor::run()). Some are the
 *  scripted scenes (the Azure Flute, the lake's briefcase, the starter,
 *  Dawn's catching tutorial and the catch); the rest do what the shared
 *  steps cannot, as they ran on the console: the Poke Mart lady's Potions
 *  and the stairs into Jubilife (00.04c, 00.04d), a clown who wanders
 *  (00.05b and 00.05c), the Poketch check (00.05c, 01.01), the Gym's
 *  tries from the Pokemon Center (01.04) and Rock Smash
 *  (01.09). Each fails by throwing, with a screenshot; the executor turns
 *  that into a failed step.
 *
 *  They are in files by area: _Events_Lake (00.00 to 00.04), _Sandgem
 *  (00.04c, 00.04d), _Jubilife (00.05b), _Oreburgh (01.04) and _Route204
 *  (01.09).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Events_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Events_H

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Runs a step on the console. Returns where it left the player when it
//  knows; throws OperationFailedException when it fails.
using StoryEvent = std::function<Pokemon::Outcome(
    StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep& step
)>;
//  Map of story event names to their execution functions.
using StoryEvents = std::map<std::string, StoryEvent>;

//  Every event of the story, by name.
const StoryEvents& story_events();

//  An event that only acts and says nothing of where it leaves us.
StoryEvent console_event(std::function<void(VideoStream& stream, ProControllerContext& context)> action);

//  Populates lake-area story event handlers.
void add_lake_events(StoryEvents& events);
//  Populates Sandgem-area story event handlers.
void add_sandgem_events(StoryEvents& events);
//  Populates Jubilife-area story event handlers.
void add_jubilife_events(StoryEvents& events);
//  Populates Oreburgh-area story event handlers.
void add_oreburgh_events(StoryEvents& events);
//  Populates Route 204-area story event handlers.
void add_route204_events(StoryEvents& events);

//  A one-tile step along the row below a wanderer (00.05b's clowns), `east`
//  +1 or -1, from its walk: the tiles it moved east (0 when he or a wall
//  blocked it), or none when it left the row: through a door, into a
//  dialogue. 00.05c (2026-11-11): a step's fade went unread and the search
//  went on indoors, so the next checkpoint saved in the Poketch Company.
std::optional<int> wander_row_step(const WalkResult& result, int east);
//  Whether asking north from `stand` on `map` would talk to a sign or to
//  someone who stands still, not the wanderer whose home is
//  `wanderer_home` (00.05b, lab-B 2026-11-11: the Poketch Company clown's
//  talk was "answered" in round 5 by another line).
bool asks_someone_else(const Pokemon::MapData& map, Pokemon::TilePos stand, Pokemon::TilePos wanderer_home);
//  The box the Mart clown wanders, round (55, 33): PokemonSimulation's
//  data/games/bdsp/maps/jubilife_city.json (decomp range 1). The story map
//  leaves him out of its wanderers (the decomp hides him behind a flag).
Pokemon::TileBox jubilife_mart_clown_box();
//  A Jubilife clown's coupon (00.05b, 00.05c): his talk step, the Key
//  Item's Bag slug, and the run flag read_coupons sets when it is held.
struct ClownCoupon{
    std::string_view step;
    std::string_view slug;
    std::string_view flag;
};
//  The three clowns' coupons: the Mart's 1, the company's 2, the TV's 3.
std::span<const ClownCoupon> jubilife_clown_coupons();
//  The row the Poketch Company's clown wanders, round (15, 18), as the
//  Mart clown's box.
Pokemon::TileBox jubilife_company_clown_box();
//  The tiles 00.05c may start on: against the Poketch Company's sign
//  (00.05b's end), and on the street east (its own end, 01.01's start).
std::pair<Pokemon::TilePos, Pokemon::TilePos> jubilife_start_tiles();



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Overworld Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG maps the overworld routes know about so far: every map from a
 *  new game in Pallet Town through Viridian City, Route 2 and Viridian
 *  Forest to Brock's gym in Pewter City, Route 3, Mt. Moon and Route 4;
 *  and Cerulean City, its Pokemon Center, Mart and Gym, Route 24 (Nugget
 *  Bridge) and Route 25 to Bill's Sea Cottage; the burgled house, Route 5,
 *  the Underground Path and Route 6 to Vermilion City and its Pokemon
 *  Center, and the S.S. Anne's corridors to the Captain's office; and the
 *  three floors of Cerulean Cave, where Mewtwo waits on B1F.
 *
 *  Each map is a pinned PokemonSimulation JSON resource, prepared under
 *  Packages/Resources/Pokemon/Maps/frlg and built into the program by CMake
 *  (pa_embed_text_files() in cmake/EmbedTextFiles.cmake), so the program and
 *  its tests read the same copy. The schema is Pokemon::MapData's
 *  (Pokemon/Navigation/Pokemon_MapData.h). A warp leads to point
 *  "warp_<n>" of its map, the tile of that map's warp n.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldMaps_H
#define PokemonAutomation_PokemonFRLG_OverworldMaps_H

#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Reads a map from its JSON text (Pokemon::parse_map()). `source` names it
//  in errors. Throws ParseException, naming the map and the field, if a field
//  is missing or doesn't fit the schema.
MapData parse_map(const std::string& json, const std::string& source);

//  (name, JSON text) of each file in Maps/, sorted by name. Generated at
//  build time.
//  TODO: move the maps to PokemonAutomation/Packages (Resources/PokemonFRLG/Maps) when ready.
const std::vector<std::pair<std::string, std::string>>& overworld_map_json();

//  Reads map `name` from overworld_map_json(). Throws InternalProgramError if
//  it isn't built in, and ParseException as parse_map().
MapData load_map(const std::string& name);

//  Every map, by its decompilation name ("CeruleanCity", "CeruleanCave_B1F", ...),
//  with the objects the given event flags hide removed. The maps are parsed
//  once, on the first call.
std::vector<MapData> overworld_maps(const std::set<std::string>& set_flags = {});

//  `maps` with the tiles and gates of each map in GateMaps/: the floors of
//  Silph Co. with their card-key doors shut, as a new game has them, until the story opens
//  them. Throws ParseException as parse_map(), or InternalProgramError if a
//  map of the fixtures has other dimensions than the same map of `maps`.
std::vector<MapData> with_gate_maps(std::vector<MapData> maps);

//  Point `point` of map `map` among `maps`, e.g. ("CeruleanCave_B1F",
//  "mewtwo"). Throws InternalProgramError if there is none.
TilePos map_point(const std::vector<MapData>& maps, const std::string& map, const std::string& point);

//  The flags in flag set `name` of every map among `maps`, e.g.
//  "after_hall_of_fame": the flags the Hall of Fame sets, which clear the way
//  to Cerulean Cave.
std::set<std::string> map_flag_set(const std::vector<MapData>& maps, const std::string& name);


//  Registers overworld map unit tests.
//  Whether a warp onto `map` leaves a rider on the Bicycle: the Cycling
//  Road's gates (allow_cycling in pokefirered's map.json), the routes
//  their doors open onto, and the Safari Zone's areas, whose edge warps
//  lead from one to the next (not measured). Every other door takes the player off (console
//  B, 2026-11-12 01:05: through Route 16's north gate the player still
//  rode, and a run-paced open leg after it overshot the Fly house's door).
bool warp_keeps_bike(const std::string& map);

void add_tests_OverworldMaps(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

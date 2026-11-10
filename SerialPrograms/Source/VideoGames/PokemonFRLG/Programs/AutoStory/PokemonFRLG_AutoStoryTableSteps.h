/*  AutoStory Table Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story table (PokemonFRLG_AutoStoryTable.h) is written in: the
 *  maps' names and the functions that make its rows and steps, shared by
 *  the files its rows are in.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTableSteps_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTableSteps_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


//  Cardinal direction: North.
inline constexpr Direction NORTH = Direction::NORTH;
//  Cardinal direction: South.
inline constexpr Direction SOUTH = Direction::SOUTH;
//  Cardinal direction: East.
inline constexpr Direction EAST = Direction::EAST;
//  Cardinal direction: West.
inline constexpr Direction WEST = Direction::WEST;

//  Pallet Town player's bedroom map.
inline constexpr const char* BEDROOM = "PalletTown_PlayersHouse_2F";
//  Pallet Town overworld map.
inline constexpr const char* PALLET = "PalletTown";
//  Professor Oak's lab map.
inline constexpr const char* LAB = "PalletTown_ProfessorOaksLab";
//  Viridian City overworld map.
inline constexpr const char* VIRIDIAN = "ViridianCity";
//  Viridian City Mart map.
inline constexpr const char* VIRIDIAN_MART = "ViridianCity_Mart";
//  Viridian City Pokemon Center map.
inline constexpr const char* VIRIDIAN_CENTER = "ViridianCity_PokemonCenter_1F";
//  Viridian Forest overworld map.
inline constexpr const char* FOREST = "ViridianForest";
//  Viridian Forest North Entrance gate map.
inline constexpr const char* FOREST_GATE = "Route2_ViridianForest_NorthEntrance";
//  Pewter City overworld map.
inline constexpr const char* PEWTER = "PewterCity";
//  Pewter City Pokemon Center map.
inline constexpr const char* PEWTER_CENTER = "PewterCity_PokemonCenter_1F";
//  Pewter City Gym map.
inline constexpr const char* GYM = "PewterCity_Gym";
//  Route 3 overworld map.
inline constexpr const char* ROUTE3 = "Route3";
//  Route 4 overworld map.
inline constexpr const char* ROUTE4 = "Route4";
//  Route 4 Pokemon Center map.
inline constexpr const char* ROUTE4_CENTER = "Route4_PokemonCenter_1F";
//  Mt. Moon 1F map.
inline constexpr const char* MT_MOON_1F = "MtMoon_1F";
//  Mt. Moon B2F map.
inline constexpr const char* MT_MOON_B2F = "MtMoon_B2F";
//  Cerulean City overworld map.
inline constexpr const char* CERULEAN = "CeruleanCity";
//  Cerulean City Pokemon Center map.
inline constexpr const char* CERULEAN_CENTER = "CeruleanCity_PokemonCenter_1F";
//  Cerulean City Mart map.
inline constexpr const char* CERULEAN_MART = "CeruleanCity_Mart";
//  Cerulean City Gym map.
inline constexpr const char* CERULEAN_GYM = "CeruleanCity_Gym";
//  Route 24 overworld map.
inline constexpr const char* ROUTE24 = "Route24";
//  Route 25 overworld map.
inline constexpr const char* ROUTE25 = "Route25";
//  Bill's Sea Cottage map.
inline constexpr const char* SEA_COTTAGE = "Route25_SeaCottage";
//  Route 6 overworld map.
inline constexpr const char* ROUTE6 = "Route6";
//  Vermilion City overworld map.
inline constexpr const char* VERMILION = "VermilionCity";
//  Vermilion City Pokemon Center map.
inline constexpr const char* VERMILION_CENTER = "VermilionCity_PokemonCenter_1F";
//  Vermilion City Gym map.
inline constexpr const char* VERMILION_GYM = "VermilionCity_Gym";
//  S.S. Anne 1F corridor map.
inline constexpr const char* SS_ANNE_1F = "SSAnne_1F_Corridor";
//  S.S. Anne 2F corridor map.
inline constexpr const char* SS_ANNE_2F = "SSAnne_2F_Corridor";
//  Captain's Office map.
inline constexpr const char* CAPTAINS_OFFICE = "SSAnne_CaptainsOffice";
//  Vermilion City Pokemon Fan Club map.
inline constexpr const char* FAN_CLUB = "VermilionCity_PokemonFanClub";
//  Cerulean City Bike Shop map.
inline constexpr const char* BIKE_SHOP = "CeruleanCity_BikeShop";
//  Route 9 overworld map.
inline constexpr const char* ROUTE9 = "Route9";
//  Route 10 overworld map.
inline constexpr const char* ROUTE10 = "Route10";
//  Route 10 Pokemon Center map.
inline constexpr const char* ROUTE10_CENTER = "Route10_PokemonCenter_1F";
//  Rock Tunnel 1F map.
inline constexpr const char* ROCK_TUNNEL_1F = "RockTunnel_1F";
//  Rock Tunnel B1F map.
inline constexpr const char* ROCK_TUNNEL_B1F = "RockTunnel_B1F";
//  Lavender Town overworld map.
inline constexpr const char* LAVENDER = "LavenderTown";
//  Lavender Town Pokemon Center map.
inline constexpr const char* LAVENDER_CENTER = "LavenderTown_PokemonCenter_1F";

//  Constructs pose at map coordinates.
Pokemon::Pose at(const char* map, int x, int y, std::optional<Direction> facing = std::nullopt);

//  Constructs story checkpoint row.
StoryCheckpoint row(
    std::string id, std::string title, std::optional<Pokemon::Pose> start,
    Supplies supplies, StartMenu menu, std::vector<StoryStep> steps
);

//  At the rows after Brock: no items counted, the starter with the moves
//  the story gave it by Lv13. A run that loads a save reads the moves off
//  the lead's summary instead (read_lead_moves()), so a save whose moves
//  differ from these (console B, 2026-11-02: Water Gun where Tackle was)
//  is fought with the moves it has.
Supplies after_brock();

//  What a walk does about wild Pokemon: runs, or fights them for EXP.
enum class WildBattles{
    RUN,
    FIGHT,
};

//  Constructs a story step of given kind.
StoryStep make(StoryStepKind kind, std::string name, std::string why);
//  Constructs a route navigation step.
StoryStep route(std::string name, Pokemon::Pose target, std::string why, WildBattles wild = WildBattles::RUN);
//  Constructs a single directional movement step.
StoryStep step_into(std::string name, Direction direction, std::string why);
//  A step `direction` into a trainer's line, a tile at a time until the
//  trainer sees the player (TRAINER_LINE_VARIANT): the battle after it
//  starts whether the walk before ended on its tile or a tile short.
StoryStep into_line(std::string name, Direction direction, std::string why);
//  A step `direction` into the line of a trainer who looks around
//  (TRAINER_WAIT_VARIANT), then a wait until they turn to the player: the
//  battle after it starts on that tile.
StoryStep into_sight(std::string name, Direction direction, std::string why);
//  A step the way a ledge faces, from the tile before it: the player hops
//  it and lands on `landing`, two tiles on. Planned routes never hop a
//  ledge (they hold into walls, and a held ledge hops), so where the only
//  way on is down a ledge the table takes it as this one step.
StoryStep hop(std::string name, Direction direction, Pokemon::Pose landing, std::string why);
//  One step `direction` onto `tile`, the next one, where a walk's hold
//  would go on: onto the tile before a ledge, which a held leg hops.
StoryStep step_onto(std::string name, Direction direction, Pokemon::Pose tile, std::string why);
//  Constructs a dialog answer step.
StoryStep answer(std::string name, bool yes, std::string why);
//  Constructs an in-game cutscene or dialog interaction step.
StoryStep scene(std::string name, std::optional<Pokemon::Pose> pose_after, std::string why);
//  Constructs a trainer or boss battle step.
StoryStep battle(std::string name, Pokemon::Pose pose_after, bool may_lose, std::string why);
//  Constructs a fixed multi-leg route step.
StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, WildBattles wild = WildBattles::RUN);
//  `step`, a ROUTE or LEGS, run with B held: from the Running Shoes on.
//  Indoors, where the game doesn't let the player run, it walks.
StoryStep running(StoryStep step);
//  Constructs new game introduction sequence step.
StoryStep new_game_intro(Pokemon::Pose pose_after, std::string why);
//  Constructs an NPC dialog interaction step.
StoryStep talk(std::string name, std::string why);
//  Constructs checkpoint preparation and recovery step.
StoryStep ready(std::string name, Pokemon::Pose nurse, std::string why);
//  Constructs a Pokemon Center healing sequence step.
StoryStep heal(std::string name);
//  Constructs a Poke Mart shopping step.
StoryStep shop(std::string name, std::vector<Pokemon::StoryItem> items, std::string why);
//  Constructs an internal engine gameplay automation step.
StoryStep game_step(std::string variant, Pokemon::Pose pose_after, std::string why);
//  Constructs game settings configuration step.
StoryStep options_step();
//  Cuts the tree the player faces (CUT_TREE_VARIANT) and steps onto its
//  tile, `tree`, facing the way the step goes.
StoryStep cut_tree(std::string name, Pokemon::Pose tree, std::string why);
//  Pushes the switch behind the Game Corner's poster the player faces
//  (POSTER_SWITCH_VARIANT).
StoryStep poster_switch(std::string name, std::string why);
//  Rides the Rocket Hideout's lift from the floor it stands at to `to`
//  (lift_floor_variant()): "B1F", "B2F", "B4F". The player starts below
//  `from`'s lift door, facing north, and ends on `to`'s, facing south.
StoryStep lift_floor(std::string name, std::string_view from, std::string_view to, std::string why);
//  Into the Pokemon Center `center` (a map name) from below its door,
//  facing north, to below the nurse's counter (CENTER_ENTER_VARIANT): the
//  same script in every Center, with no map of its rooms.
StoryStep center_enter(std::string name, const char* center, std::string why);
//  Out of the Center from below the nurse's counter (CENTER_EXIT_VARIANT)
//  to `below_door`, the tile below its door outside, facing south.
StoryStep center_exit(std::string name, Pokemon::Pose below_door, std::string why);
//  Uses `item` from the bag on the overworld (use_item_variant()): a
//  Repel, or an Escape Rope, which leaves the player on `pose_after`.
StoryStep use_item(std::string name, std::string_view item, std::string why, std::optional<Pokemon::Pose> pose_after = std::nullopt);
//  Gets on the Bicycle (RIDE_BIKE_VARIANT).
StoryStep ride_bike(std::string name, std::string why);


} // namespace StoryTable
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

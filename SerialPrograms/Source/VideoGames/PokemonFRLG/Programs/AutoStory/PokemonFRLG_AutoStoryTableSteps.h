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
#include <vector>
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


inline constexpr Direction NORTH = Direction::NORTH;
inline constexpr Direction SOUTH = Direction::SOUTH;
inline constexpr Direction EAST = Direction::EAST;
inline constexpr Direction WEST = Direction::WEST;

inline constexpr const char* BEDROOM = "PalletTown_PlayersHouse_2F";
inline constexpr const char* PALLET = "PalletTown";
inline constexpr const char* LAB = "PalletTown_ProfessorOaksLab";
inline constexpr const char* VIRIDIAN = "ViridianCity";
inline constexpr const char* VIRIDIAN_MART = "ViridianCity_Mart";
inline constexpr const char* VIRIDIAN_CENTER = "ViridianCity_PokemonCenter_1F";
inline constexpr const char* FOREST = "ViridianForest";
inline constexpr const char* FOREST_GATE = "Route2_ViridianForest_NorthEntrance";
inline constexpr const char* PEWTER = "PewterCity";
inline constexpr const char* PEWTER_CENTER = "PewterCity_PokemonCenter_1F";
inline constexpr const char* GYM = "PewterCity_Gym";
inline constexpr const char* ROUTE3 = "Route3";
inline constexpr const char* ROUTE4 = "Route4";
inline constexpr const char* ROUTE4_CENTER = "Route4_PokemonCenter_1F";
inline constexpr const char* MT_MOON_1F = "MtMoon_1F";
inline constexpr const char* MT_MOON_B2F = "MtMoon_B2F";
inline constexpr const char* CERULEAN = "CeruleanCity";
inline constexpr const char* CERULEAN_CENTER = "CeruleanCity_PokemonCenter_1F";
inline constexpr const char* CERULEAN_MART = "CeruleanCity_Mart";
inline constexpr const char* CERULEAN_GYM = "CeruleanCity_Gym";
inline constexpr const char* ROUTE24 = "Route24";
inline constexpr const char* ROUTE25 = "Route25";
inline constexpr const char* SEA_COTTAGE = "Route25_SeaCottage";
inline constexpr const char* ROUTE6 = "Route6";
inline constexpr const char* VERMILION = "VermilionCity";
inline constexpr const char* VERMILION_CENTER = "VermilionCity_PokemonCenter_1F";
inline constexpr const char* VERMILION_GYM = "VermilionCity_Gym";
inline constexpr const char* SS_ANNE_1F = "SSAnne_1F_Corridor";
inline constexpr const char* SS_ANNE_2F = "SSAnne_2F_Corridor";
inline constexpr const char* CAPTAINS_OFFICE = "SSAnne_CaptainsOffice";

Pokemon::Pose at(const char* map, int x, int y, std::optional<Direction> facing = std::nullopt);

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

StoryStep make(StoryStepKind kind, std::string name, std::string why);
StoryStep route(std::string name, Pokemon::Pose target, std::string why, WildBattles wild = WildBattles::RUN);
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
StoryStep answer(std::string name, bool yes, std::string why);
StoryStep scene(std::string name, std::optional<Pokemon::Pose> pose_after, std::string why);
StoryStep battle(std::string name, Pokemon::Pose pose_after, bool may_lose, std::string why);
StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, WildBattles wild = WildBattles::RUN);
//  `step`, a ROUTE or LEGS, run with B held: from the Running Shoes on.
//  Indoors, where the game doesn't let the player run, it walks.
StoryStep running(StoryStep step);
StoryStep new_game_intro(Pokemon::Pose pose_after, std::string why);
StoryStep talk(std::string name, std::string why);
StoryStep ready(std::string name, Pokemon::Pose nurse, std::string why);
StoryStep heal(std::string name);
StoryStep shop(std::string name, std::vector<Pokemon::StoryItem> items, std::string why);
StoryStep game_step(std::string variant, Pokemon::Pose pose_after, std::string why);
StoryStep options_step();
//  Cuts the tree the player faces (CUT_TREE_VARIANT) and steps onto its
//  tile, `tree`, facing the way the step goes.
StoryStep cut_tree(std::string name, Pokemon::Pose tree, std::string why);


//  Rows in files of their own (PokemonFRLG_AutoStoryTableVermilionGym.cpp).
StoryCheckpoint vermilion_gym();
StoryCheckpoint to_lt_surge();
StoryCheckpoint lt_surge();



}
}
}
}
#endif

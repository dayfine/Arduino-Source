/*  AutoStory Checkpoints
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story checkpoint, and the fixed routes, wall turns and mart
 *  orders its rows are built from.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStorySaveChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A shared checkpoint with what FRLG adds to it.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
};
//  `legs` from `start` as route steps for the walker, each checked against
//  the map: nothing if a step is blocked or leaves the map. A leg ends "to
//  wall" if the next step its way is blocked.
std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
);

//  A way the player at `tile` can turn to without stepping: a wall there,
//  with no one standing in it. West and east first: walking north or south
//  into a sign would read it.
std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile);

//  `items` as orders at the Viridian Mart before the Boulder Badge: the
//  rows of POKE BALL, POTION, ANTIDOTE, PARLYZ HEAL. Nothing if it does
//  not sell one of them.
std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

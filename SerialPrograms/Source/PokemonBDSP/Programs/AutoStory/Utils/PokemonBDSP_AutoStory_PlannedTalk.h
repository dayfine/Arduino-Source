/*  BDSP AutoStory - Planned talk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walking up to someone on the story maps and talking to them.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PlannedTalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PlannedTalk_H

#include <string>
#include <vector>
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whom talk_to_npc() talks to: the NPC on world tile `npc`, from the sides
//  in `sides` (the side of the NPC to stand on), in order.
struct NpcTalk{
    Pokemon::TilePos npc;
    std::vector<Pokemon::Direction> sides;
    std::string label;
};

//  Walks next to `talk.npc`, faces them and presses A, trying each side
//  until a dialogue opens. `at` is where the player may be, and is updated.
//  Fails with a screenshot if no side starts a dialogue. A press from a
//  diagonal tile, or from a tile a row off, talks to nobody, so each side is
//  tried in turn.
void talk_to_npc(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<Pokemon::TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
);
//  talk_to_npc() through `io`.
void talk_to_npc(
    RouteIO& io, const StoryWorld& maps, std::vector<Pokemon::TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

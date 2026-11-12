/*  BDSP AutoStory - Planned talk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP_AutoStory_PlannedTalk.h"
#include "PokemonBDSP_AutoStory_RouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


void talk_to_npc(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    const StoryWorld& maps, std::vector<TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
){
    ConsoleRouteIO io(stream, context, escape);
    talk_to_npc(io, maps, at, trainers, talk);
}

void talk_to_npc(
    RouteIO& io, const StoryWorld& maps, std::vector<TilePos>& at,
    RouteTrainers& trainers, const NpcTalk& talk
){
    for (Direction side : talk.sides){
        const TilePos stand = talk.npc.next(side);
        if (!standable(maps, stand)){
            continue;
        }
        at = walk_route(io, maps, at, trainers, {
            .goal = [stand](const TilePos& pos){ return pos == stand; },
            .label = talk.label,
            .avoid = {},
            .costs = route_plan_costs(),
        });
        //  Face the NPC: the opposite of the side we stand on.
        const Direction face =
            side == Direction::NORTH ? Direction::SOUTH :
            side == Direction::SOUTH ? Direction::NORTH :
            side == Direction::EAST ? Direction::WEST : Direction::EAST;
        turn(io, face);
        io.press(press_a(RouteInputEnd::DONE));
        if (io.dialog_within(2000ms, COLOR_CYAN)){
            io.note(talk.label + ": talking from " + describe(maps, stand) + ".", RouteNote::STATUS);
            return;
        }
        io.note(talk.label + ": nobody answered from " + describe(maps, stand) + "; trying another side.", RouteNote::STATUS);
    }
    fail(io, talk.label + ": no side of " + describe(maps, talk.npc) + " started a dialogue.");
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

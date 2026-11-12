/*  BDSP AutoStory - Wait beside a wanderer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_WanderWait.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TilePos;


namespace{

//  Walks the column from `at` to `to`, a tile a step. A blocked step asks
//  that way while `talked` is false, and ends the walk if the talk counts.
//  False if a step stayed blocked for the plan's asks, or `to` is off the
//  column.
bool walk_column(WanderWaitBody& body, TilePos& at, TilePos to, const WanderWaitPlan& plan, bool* talked){
    if (at.x != to.x){
        return false;
    }
    while (at != to){
        const Direction direction = to.y < at.y ? Direction::NORTH : Direction::SOUTH;
        size_t tries = 0;
        while (!body.step(direction)){
            if (talked != nullptr && !*talked){
                body.face(direction);
                if (body.ask()){
                    *talked = true;
                    return true;
                }
            }
            if (++tries >= plan.asks){
                return false;
            }
            body.wait(plan.blocked_wait);
        }
        at = at.next(direction);
    }
    return true;
}

//  Asks from where the player stands, facing the plan's way.
bool ask_from_stand(WanderWaitBody& body, const WanderWaitPlan& plan){
    body.face(plan.facing);
    for (size_t ask = 0; ask < plan.asks; ask++){
        if (body.ask()){
            return true;
        }
        body.wait(plan.between_asks);
    }
    return false;
}

} // namespace


WanderWaitResult wait_beside_wanderer(WanderWaitBody& body, TilePos at, const WanderWaitPlan& plan){
    bool talked = false;
    TilePos talked_at = at;
    for (size_t round = 0; round < plan.rounds && !talked; round++){
        for (const TilePos& stand : plan.stands){
            if (!walk_column(body, at, stand, plan, &talked)){
                return {.end = WanderWaitEnd::STUCK, .at = at, .talked = at};
            }
            if (!talked){
                talked = ask_from_stand(body, plan);
            }
            if (talked){
                talked_at = at;
                break;
            }
        }
    }
    if (!talked){
        return {.end = WanderWaitEnd::NO_TALK, .at = at, .talked = at};
    }
    if (!walk_column(body, at, plan.exit, plan, nullptr)){
        return {.end = WanderWaitEnd::STUCK, .at = at, .talked = talked_at};
    }
    for (Direction wall : plan.exit_walls){
        if (body.step(wall)){
            return {.end = WanderWaitEnd::LOST, .at = at.next(wall), .talked = talked_at};
        }
    }
    return {.end = WanderWaitEnd::TALKED, .at = at, .talked = talked_at};
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BDSP AutoStory - Wait beside a wanderer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <optional>
#include "PokemonBDSP_AutoStory_WanderWait.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::TilePos;


namespace{

//  Walks a straight line from `at` to `to`, a tile a step. A blocked step
//  asks that way while `talked` is false, and ends the walk if the talk
//  counts. False if a step stayed blocked for `tries` steps, or `to` is
//  not on a line from `at`.
bool walk_line(WanderWaitBody& body, TilePos& at, TilePos to, const WanderWaitPlan& plan, size_t tries, bool* talked){
    if (at.x != to.x && at.y != to.y){
        return false;
    }
    while (at != to){
        const Direction direction = at.x != to.x
            ? (to.x < at.x ? Direction::WEST : Direction::EAST)
            : (to.y < at.y ? Direction::NORTH : Direction::SOUTH);
        size_t blocked = 0;
        while (!body.step(direction)){
            if (talked != nullptr && !*talked){
                body.face(direction);
                if (body.ask()){
                    *talked = true;
                    return true;
                }
            }
            if (++blocked >= tries){
                return false;
            }
            body.wait(plan.blocked_wait);
        }
        at = at.next(direction);
    }
    return true;
}

//  The row to cross between columns from `at` to `to`: the one above the
//  stands or the one below, whichever walk is shorter.
int crossing_row(const WanderWaitPlan& plan, const TilePos& at, const TilePos& to){
    std::optional<int> top;
    std::optional<int> bottom;
    for (const WanderWaitSide& side : plan.sides){
        for (const TilePos& stand : side.stands){
            top = top ? std::min(*top, stand.y) : stand.y;
            bottom = bottom ? std::max(*bottom, stand.y) : stand.y;
        }
    }
    const int above = top.value_or(at.y) - 1;
    const int below = bottom.value_or(at.y) + 1;
    const int up = std::abs(at.y - above) + std::abs(to.y - above);
    const int down = std::abs(at.y - below) + std::abs(to.y - below);
    return up <= down ? above : below;
}

//  Walks from `at` to `to`: down the column, or between columns along a
//  crossing row. Asks a blocked step in a column as walk_line() does.
bool walk_to(WanderWaitBody& body, TilePos& at, TilePos to, const WanderWaitPlan& plan, size_t tries, bool* talked){
    if (at.x == to.x){
        return walk_line(body, at, to, plan, tries, talked);
    }
    const int row = crossing_row(plan, at, to);
    if (!walk_line(body, at, {at.x, row}, plan, tries, talked)){
        return false;
    }
    if (talked != nullptr && *talked){
        return true;
    }
    return walk_line(body, at, {to.x, row}, plan, tries, nullptr)
        && walk_line(body, at, to, plan, tries, talked);
}

//  Asks from where the player stands, facing `facing`.
bool ask_from_stand(WanderWaitBody& body, const WanderWaitPlan& plan, Direction facing){
    body.face(facing);
    for (size_t ask = 0; ask < plan.asks; ask++){
        if (body.ask()){
            return true;
        }
        body.wait(plan.between_asks);
    }
    return false;
}

//  The way into the box from the side whose column `at` is on.
std::optional<Direction> inward_from(const WanderWaitPlan& plan, const TilePos& at){
    for (const WanderWaitSide& side : plan.sides){
        if (!side.stands.empty() && side.stands[0].x == at.x){
            return side.facing;
        }
    }
    return std::nullopt;
}

//  Walks from where the talk counted to the exit. If he stays in the way
//  on the column, steps into the box beside him and goes round.
bool walk_out(WanderWaitBody& body, TilePos& at, const WanderWaitPlan& plan){
    if (walk_to(body, at, plan.exit, plan, plan.exit_tries, nullptr)){
        return true;
    }
    const std::optional<Direction> inward = inward_from(plan, at);
    if (!inward || at.x != plan.exit.x || !body.step(*inward)){
        return walk_to(body, at, plan.exit, plan, plan.blocked_tries, nullptr);
    }
    at = at.next(*inward);
    return walk_to(body, at, plan.exit, plan, plan.blocked_tries, nullptr);
}

//  Walks `side`'s stands the plan's passes, asking from each, until the
//  talk counts (`talked`, on `at`). False if a step stayed blocked.
bool wait_at_side(WanderWaitBody& body, TilePos& at, const WanderWaitPlan& plan, const WanderWaitSide& side, bool& talked){
    for (size_t pass = 0; pass < plan.passes; pass++){
        for (const TilePos& stand : side.stands){
            if (!walk_to(body, at, stand, plan, plan.blocked_tries, &talked)){
                return false;
            }
            if (!talked){
                talked = ask_from_stand(body, plan, side.facing);
            }
            if (talked){
                return true;
            }
        }
    }
    return true;
}

} // namespace


WanderWaitResult wait_beside_wanderer(WanderWaitBody& body, TilePos at, const WanderWaitPlan& plan){
    bool talked = false;
    TilePos talked_at = at;
    for (size_t round = 0; round < plan.rounds && !talked; round++){
        for (size_t s = 0; s < plan.sides.size() && !talked; s++){
            if (!wait_at_side(body, at, plan, plan.sides[s], talked)){
                return {.end = WanderWaitEnd::STUCK, .at = at, .talked = at};
            }
            talked_at = at;
        }
    }
    if (!talked){
        return {.end = WanderWaitEnd::NO_TALK, .at = at, .talked = at};
    }
    if (!walk_out(body, at, plan)){
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

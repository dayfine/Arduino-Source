/*  Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Find wall anchors without choosing one uncertain starting position.
 *  A hold means keeping one movement direction pressed until a wall stops
 *  the player. Its descriptor belongs to the game; it is not a duration or
 *  a count of taps. Results predict convergence, not confirmed execution.
 *
 *  MovementPolicy defines Hypothesis, Hold and Position, and static operations:
 *      Position position(const Hypothesis&);
 *      optional<WallRecoveryStep<Hypothesis, Hold>> hold(context, positions, direction);
 *      int tiles(const Hold&);
 *  The policy type selects game rules at compile time. Runtime Context data
 *  (map, walking mode, etc.) is passed separately; no policy instance or global
 *  map is needed. hold() receives Context and a const vector of hypotheses,
 *  rejecting any direction unsafe for any hypothesis, including NPCs, warps
 *  and unknown edges. Hypotheses are copyable; Hypothesis/Position support ==.
 */

#ifndef PokemonAutomation_Pokemon_WallRecovery_H
#define PokemonAutomation_Pokemon_WallRecovery_H

#include <algorithm>
#include <optional>
#include <span>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{

//  Bound the number of directional wall holds in a recovery plan.
struct WallRecoveryLimits{
    size_t max_holds = 3;
};

//  One safe directional hold and its resulting hypotheses, supplied by the policy.
template <typename Hypothesis, typename Hold>
struct WallRecoveryStep{
    std::vector<Hypothesis> positions;
    Hold hold;
};

//  A sequence of safe holds that brings every hypothesis to the same position.
template <typename Position, typename Hold>
struct WallAnchor{
    std::vector<Hold> holds;
    Position at;
};

//  Return minimum-hold anchors, ordered by held tiles, then ALL_DIRECTIONS.
//  Empty input has no anchor; already-converged input needs no holds even at
//  max_holds=0. The borrowed span is not retained: search branches own their
//  hypothesis vectors, including any distinct object-world alternatives.
template <typename MovementPolicy, typename Context>
std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>> find_wall_anchors(
    std::span<const typename MovementPolicy::Hypothesis> positions, const Context& context,
    const WallRecoveryLimits& limits = WallRecoveryLimits()
);

namespace WallRecoveryInternal{

//  Whether a nonempty belief set represents one physical position.
template <typename MovementPolicy>
bool converged(std::span<const typename MovementPolicy::Hypothesis> positions){
    return !positions.empty() && std::all_of(positions.begin(), positions.end(), [&](const auto& position){
        return MovementPolicy::position(position) == MovementPolicy::position(positions.front());
    });
}

//  Explore exactly depth more holds, retaining only common safe anchors.
template <typename MovementPolicy, typename Context>
void search(
    const std::vector<typename MovementPolicy::Hypothesis>& positions, const Context& context,
    size_t depth, std::vector<typename MovementPolicy::Hold>& holds,
    std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>>& found
){
    for (Direction direction : ALL_DIRECTIONS){
        auto next = MovementPolicy::hold(context, positions, direction);
        if (!next || next->positions.empty() || next->positions == positions || MovementPolicy::tiles(next->hold) == 0){
            continue;
        }
        holds.push_back(next->hold);
        if (converged<MovementPolicy>(next->positions)){
            if (depth == 1){
                found.push_back({holds, MovementPolicy::position(next->positions.front())});
            }
        }else if (depth > 1){
            search<MovementPolicy>(next->positions, context, depth - 1, holds, found);
        }
        holds.pop_back();
    }
}

//  Sum held-tile distances for stable ordering among equal-length plans.
template <typename MovementPolicy>
int total_tiles(std::span<const typename MovementPolicy::Hold> holds){
    int total = 0;
    for (const auto& hold : holds){
        total += MovementPolicy::tiles(hold);
    }
    return total;
}

} // namespace WallRecoveryInternal

template <typename MovementPolicy, typename Context>
std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>> find_wall_anchors(
    std::span<const typename MovementPolicy::Hypothesis> positions, const Context& context,
    const WallRecoveryLimits& limits
){
    using Anchor = WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>;
    if (WallRecoveryInternal::converged<MovementPolicy>(positions)){
        return {Anchor{{}, MovementPolicy::position(positions.front())}};
    }
    std::vector<Anchor> found;
    if (positions.empty()){
        return found;
    }
    const std::vector<typename MovementPolicy::Hypothesis> starts(positions.begin(), positions.end());
    for (size_t depth = 1; found.empty() && depth <= limits.max_holds; depth++){
        std::vector<typename MovementPolicy::Hold> holds;
        WallRecoveryInternal::search<MovementPolicy>(starts, context, depth, holds, found);
        std::stable_sort(found.begin(), found.end(), [&](const Anchor& a, const Anchor& b){
            return WallRecoveryInternal::total_tiles<MovementPolicy>(a.holds) < WallRecoveryInternal::total_tiles<MovementPolicy>(b.holds);
        });
    }
    return found;
}

} // namespace Pokemon
} // namespace PokemonAutomation
#endif

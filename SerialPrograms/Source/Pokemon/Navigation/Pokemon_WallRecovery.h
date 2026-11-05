/*  Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Find wall anchors without choosing one uncertain starting position.
 *  A hold means keeping one movement direction pressed until a wall stops
 *  the player. Its descriptor belongs to the game; it is not a duration or
 *  a count of taps. Results predict convergence, not confirmed execution.
 *
 *  MovementPolicy is a compile-time policy, supplied as a stateful object
 *  so it can refer to the game's map and trainer/object-world hypotheses.
 *  It defines Hypothesis, Hold and Position, and provides:
 *      Position position(const Hypothesis&) const;
 *      optional<WallRecoveryStep<Hypothesis, Hold>> hold(positions, direction) const;
 *      int tiles(const Hold&) const;
 *  hold() receives a const vector of hypotheses and rejects any direction
 *  unsafe for any hypothesis, including NPCs, warps and unknown map edges.
 *  Hypothesis and Position support ==. No default movement policy exists:
 *  choosing the game's collision rules is part of the caller's contract.
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
template <typename MovementPolicy>
std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>> find_wall_anchors(
    const MovementPolicy& policy, std::span<const typename MovementPolicy::Hypothesis> positions,
    const WallRecoveryLimits& limits = WallRecoveryLimits()
);

namespace WallRecoveryInternal{

//  Whether a nonempty belief set represents one physical position.
template <typename MovementPolicy>
bool converged(const MovementPolicy& policy, std::span<const typename MovementPolicy::Hypothesis> positions);

//  Explore exactly depth more holds, retaining only common safe anchors.
template <typename MovementPolicy>
void search(
    const MovementPolicy& policy, const std::vector<typename MovementPolicy::Hypothesis>& positions,
    size_t depth, std::vector<typename MovementPolicy::Hold>& holds,
    std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>>& found
);

//  Sum held-tile distances for stable ordering among equal-length plans.
template <typename MovementPolicy>
int total_tiles(const MovementPolicy& policy, std::span<const typename MovementPolicy::Hold> holds);

template <typename MovementPolicy>
bool converged(const MovementPolicy& policy, std::span<const typename MovementPolicy::Hypothesis> positions){
    return !positions.empty() && std::all_of(positions.begin(), positions.end(), [&](const auto& position){
        return policy.position(position) == policy.position(positions.front());
    });
}

template <typename MovementPolicy>
void search(
    const MovementPolicy& policy, const std::vector<typename MovementPolicy::Hypothesis>& positions,
    size_t depth, std::vector<typename MovementPolicy::Hold>& holds,
    std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>>& found
){
    for (Direction direction : ALL_DIRECTIONS){
        auto next = policy.hold(positions, direction);
        if (!next || next->positions.empty() || next->positions == positions || policy.tiles(next->hold) == 0){
            continue;
        }
        holds.push_back(next->hold);
        if (converged(policy, next->positions)){
            if (depth == 1){
                found.push_back({holds, policy.position(next->positions.front())});
            }
        }else if (depth > 1){
            search(policy, next->positions, depth - 1, holds, found);
        }
        holds.pop_back();
    }
}

template <typename MovementPolicy>
int total_tiles(const MovementPolicy& policy, std::span<const typename MovementPolicy::Hold> holds){
    int total = 0;
    for (const auto& hold : holds){
        total += policy.tiles(hold);
    }
    return total;
}

} // namespace WallRecoveryInternal

template <typename MovementPolicy>
std::vector<WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>> find_wall_anchors(
    const MovementPolicy& policy, std::span<const typename MovementPolicy::Hypothesis> positions,
    const WallRecoveryLimits& limits
){
    using Anchor = WallAnchor<typename MovementPolicy::Position, typename MovementPolicy::Hold>;
    std::vector<Anchor> found;
    if (WallRecoveryInternal::converged(policy, positions)){
        return {Anchor{{}, policy.position(positions.front())}};
    }
    if (positions.empty()){
        return found;
    }
    const std::vector<typename MovementPolicy::Hypothesis> starts(positions.begin(), positions.end());
    for (size_t depth = 1; found.empty() && depth <= limits.max_holds; depth++){
        std::vector<typename MovementPolicy::Hold> holds;
        WallRecoveryInternal::search(policy, starts, depth, holds, found);
        std::stable_sort(found.begin(), found.end(), [&](const Anchor& a, const Anchor& b){
            return WallRecoveryInternal::total_tiles(policy, a.holds) < WallRecoveryInternal::total_tiles(policy, b.holds);
        });
    }
    return found;
}

} // namespace Pokemon
} // namespace PokemonAutomation
#endif

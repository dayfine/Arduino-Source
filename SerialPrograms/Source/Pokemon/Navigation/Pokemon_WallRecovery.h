/*  Wall Recovery
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Find common wall holds without choosing one uncertain starting position.
 *  The game supplies legal holds and maps hypotheses to physical positions.
 *  Different hypotheses may include different trainer/object worlds. A hold
 *  is legal only if safe from every hypothesis; doors, NPCs and unknown edges
 *  must not be reported as walls by the adapter.
 *
 *  Model defines Hypothesis, Hold and Position, and provides:
 *      Position position(const Hypothesis&) const;
 *      optional<WallRecoveryStep<Hypothesis, Hold>> hold(positions, direction) const;
 *      int tiles(const Hold&) const;
 *  Hypothesis and Position must support ==. Holds remain game-owned values.
 *  Results predict convergence, not detector-confirmed execution.
 */

#ifndef PokemonAutomation_Pokemon_WallRecovery_H
#define PokemonAutomation_Pokemon_WallRecovery_H

#include <algorithm>
#include <optional>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{

struct WallRecoveryLimits{
    size_t max_holds = 3;
};

template <typename Hypothesis, typename Hold>
struct WallRecoveryStep{
    std::vector<Hypothesis> positions;
    Hold hold;
};

template <typename Position, typename Hold>
struct WallAnchor{
    std::vector<Hold> holds;
    Position at;
};

namespace WallRecoveryInternal{

template <typename Model>
bool converged(const Model& model, const std::vector<typename Model::Hypothesis>& positions){
    return !positions.empty() && std::all_of(positions.begin(), positions.end(), [&](const auto& position){
        return model.position(position) == model.position(positions.front());
    });
}

template <typename Model>
void search(
    const Model& model, const std::vector<typename Model::Hypothesis>& positions,
    size_t depth, std::vector<typename Model::Hold>& holds,
    std::vector<WallAnchor<typename Model::Position, typename Model::Hold>>& found
){
    for (Direction direction : ALL_DIRECTIONS){
        auto next = model.hold(positions, direction);
        if (!next || next->positions.empty() || next->positions == positions || model.tiles(next->hold) == 0){
            continue;
        }
        holds.push_back(next->hold);
        if (converged(model, next->positions)){
            if (depth == 1){
                found.push_back({holds, model.position(next->positions.front())});
            }
        }else if (depth > 1){
            search(model, next->positions, depth - 1, holds, found);
        }
        holds.pop_back();
    }
}

template <typename Model>
int total_tiles(const Model& model, const std::vector<typename Model::Hold>& holds){
    int total = 0;
    for (const auto& hold : holds){
        total += model.tiles(hold);
    }
    return total;
}

}

//  Fewest holds first, then fewest tiles; ties keep ALL_DIRECTIONS order.
//  All results have the minimum hold count. Empty input has no anchor;
//  already-converged hypotheses need no holds, including at max_holds=0.
template <typename Model>
std::vector<WallAnchor<typename Model::Position, typename Model::Hold>> find_wall_anchors(
    const Model& model, const std::vector<typename Model::Hypothesis>& positions,
    const WallRecoveryLimits& limits = WallRecoveryLimits()
){
    using Anchor = WallAnchor<typename Model::Position, typename Model::Hold>;
    std::vector<Anchor> found;
    if (WallRecoveryInternal::converged(model, positions)){
        return {Anchor{{}, model.position(positions.front())}};
    }
    if (positions.empty()){
        return found;
    }
    for (size_t depth = 1; found.empty() && depth <= limits.max_holds; depth++){
        std::vector<typename Model::Hold> holds;
        WallRecoveryInternal::search(model, positions, depth, holds, found);
        std::stable_sort(found.begin(), found.end(), [&](const Anchor& a, const Anchor& b){
            return WallRecoveryInternal::total_tiles(model, a.holds) < WallRecoveryInternal::total_tiles(model, b.holds);
        });
    }
    return found;
}

}
}
#endif

/*  AutoStory Boulders
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a Strength boulder push and a Cut do to the story's state, for
 *  the console's push_boulder and cut_tree steps and the map-level sim
 *  alike.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoulders_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BoulderPushReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Updates `state` for a completed push_boulder step; returns nullopt on
//  success or a StoryFailure.
std::optional<Pokemon::StoryFailure> boulder_pushed(
    FrlgRunState& state, const StoryStep& step, const std::vector<MapData>& maps
);

enum class BoulderInterrupt{
    FAILED,
    CLEARED,
    WILD_BATTLE,
};

class BoulderPresses{
public:
    virtual ~BoulderPresses() = default;
    virtual BoulderPressRead press(Direction direction) = 0;
    virtual BoulderInterrupt clear_interrupt() = 0;
};

struct BoulderPush{
    int64_t walked = 0;
    std::optional<std::string> failure = std::nullopt;
};

BoulderPush push_boulder_tiles(Direction direction, int64_t tiles, BoulderPresses& presses);

std::optional<Pokemon::StoryFailure> boulder_push_outcome(
    const StoryStep& step, const std::vector<MapData>& maps, const BoulderPush& push, FrlgRunState& state
);

std::optional<Pokemon::StoryFailure> tree_cut(
    FrlgRunState& state, const StoryStep& step, const std::vector<MapData>& maps
);

std::optional<Pokemon::StoryFailure> regrow_on_warp(
    const std::vector<MapData>& maps,
    const std::function<std::optional<Pokemon::StoryFailure>()>& run,
    FrlgRunState& state
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

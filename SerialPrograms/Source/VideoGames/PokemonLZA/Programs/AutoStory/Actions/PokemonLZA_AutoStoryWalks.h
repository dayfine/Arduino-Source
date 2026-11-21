/*  PokemonLZA AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure timed leg execution and walk screen-check actions.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWalks_H

#include <optional>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  One leg of an overworld walk: a turn followed by a timed walk.
struct StoryWalkLeg{
    double turn = 0;
    Milliseconds walk = Milliseconds(0);
};

//  How a StoryMove's legs are executed.
enum class StoryWalkStyle{
    TURN_THEN_WALK,
    HELD,
};

//  Sequence of timed walk legs with an optional pre-walk wait.
struct StoryMove{
    std::vector<StoryWalkLeg> legs = {};
    StoryWalkStyle style = StoryWalkStyle::TURN_THEN_WALK;
    Milliseconds wait_first = Milliseconds(0);
};

//  Executes all legs of `move`.
void run_story_move(PlzaBackend& backend, const StoryMove& move);

//  Waits until the player has sustained free control before starting a walk.
//  Fails as "walk_not_free" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk);

//  Observes once after a walk completes so queued inputs have finished before the step ends.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Stopping options for walk_until().
struct WalkStopOptions{
    std::optional<ScreenEvent> end = std::nullopt;
    Milliseconds end_after = Milliseconds(0);
};

//  Walks `move`, slicing the final leg until `options.end` (after `options.end_after`) or a scene/fade occurs.
//  Fails as "walk_no_end", "walk_no_scene", or "game_not_running".
std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const WalkStopOptions& options
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

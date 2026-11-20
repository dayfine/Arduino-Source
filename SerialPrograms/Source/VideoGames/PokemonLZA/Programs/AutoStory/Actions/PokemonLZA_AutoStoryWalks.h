/*  PokemonLZA AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure overworld turning, timed leg execution, and walk screen-check actions.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWalks_H

#include <cstddef>
#include <functional>
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


//  Turns the player `degrees` clockwise (negative = left) relative to the camera.
void story_turn(PlzaBackend& backend, double degrees);

//  Reads the minimap arrow heading in degrees (0 = north/up, 90 = east/right).
using MinimapArrowReader = std::function<double()>;

//  Turns the player toward `heading` using `read_arrow` feedback, then recenters the camera.
//  Fails as "heading_missed" if the target heading is not reached.
std::optional<Pokemon::StoryFailure> face_heading(PlzaBackend& backend, const MinimapArrowReader& read_arrow, double heading);

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

//  Executes all legs of `move`. Returns "game_not_running" if HOME is detected.
std::optional<Pokemon::StoryFailure> run_story_move(PlzaBackend& backend, const StoryMove& move);

//  Executes all legs of `move` except the final leg's walk (played via hold_last_leg()).
std::optional<Pokemon::StoryFailure> run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move);

//  Holds one `slice` of `move`'s final leg walk.
void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice);

//  Waits until the player has sustained free control before starting a walk.
//  Fails as "walk_not_free" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> check_can_walk(PlzaBackend& backend, std::string_view walk);

//  Observes once after a walk completes so queued inputs have finished before the step ends.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, slicing the final leg until `end` (after `end_after`) or a scene/fade occurs.
//  Fails as "walk_no_end", "walk_no_scene", or "game_not_running".
std::optional<Pokemon::StoryFailure> walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, std::optional<ScreenEvent> end, Milliseconds end_after
);

//  Walks `move` and presses A on the interact prompt to enter a scene.
//  When `after_legs` is true, ignores prompts until all legs finish.
//  Fails as "walk_no_end", "talk_no_scene", or "game_not_running".
std::optional<Pokemon::StoryFailure> walk_into_talk(PlzaBackend& backend, std::string_view walk, const StoryMove& move, bool after_legs);

//  Steps sideways along a wall while facing `facing_degrees` until an interact prompt appears.
//  Returns the number of steps taken, or std::nullopt if no prompt was found.
std::optional<size_t> step_along_until_prompt(PlzaBackend& backend, double facing_degrees);

//  Presses A through an interact prompt until a fade, text box, or choice box opens.
void press_through_prompt(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

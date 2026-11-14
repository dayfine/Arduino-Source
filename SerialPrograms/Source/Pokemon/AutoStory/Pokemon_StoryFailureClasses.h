/*  Story Failure Classes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One list of the ways an AutoStory step fails, for every game: what the
 *  run log and run records name a failure by, and what the fault sweeps
 *  inject. Each class is a row of data: its id, what it means, how a run
 *  sees it, which sim faults inject it, and what a retry should do.
 *
 *  An Outcome's failure name ("expect_failed") is finer than a class: a
 *  class covers the names that mean the same to a retry. The engine's names
 *  are mapped here; a game's own names are mapped when it adopts the list.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFailureClasses_H
#define PokemonAutomation_Pokemon_StoryFailureClasses_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Simulation/Pokemon_SimFaults.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A class of story failure, in story_failure_classes()'s order.
enum class StoryFailureClass{
    GAME_NOT_RUNNING,
    LAUNCH_FAILED,
    WRONG_START,
    RELOAD_FAILED,
    SAVE_FAILED,
    STORY_ERROR,
    LOST_TAP,
    UNSEEN_STEP,
    WALK_SHORT,
    BLOCKED,
    MOUNT_LOST,
    POSE_LOST,
    WARP_NO_OP,
    LATE_FADE,
    SLOW_FADE_IN,
    DARK_MAP,
    SCENE_END_POSE,
    SCENE_EARLY,
    BATTLE_OPENING,
    WILD_BATTLES,
    INTERRUPT_FAILED,
    SCREEN_UNREAD,
    MENU_FAILED,
    BATTLE_LOST,
    STATE_CHECK_FAILED,
    OPERATION_FAILED,
    SIM_MISMATCH,
};

//  One class of failure.
struct StoryFailureClassInfo{
    //  What a retry should do after a failure of the class: a hint, the
    //  program decides.
    enum class Retry{
        //  Find where the player is and run the step again.
        RETRY_STEP,
        //  Reload the checkpoint's save and run it again.
        RELOAD,
        //  Stop the run: no retry fixes it, a person or a code change must.
        STOP,
    };

    StoryFailureClass failure_class;
    //  The class in logs and run records: "warp_no_op".
    std::string_view id;
    //  What went wrong, in one line.
    std::string_view meaning;
    //  How a run sees it.
    std::string_view detected_by;
    //  The sim faults that inject it; none when no sim can yet.
    std::vector<FaultClass> injected_by;
    //  The engine's Outcome failure names of this class.
    std::vector<std::string_view> outcomes;
    Retry retry;
};

//  Every class, in the enum's order.
const std::vector<StoryFailureClassInfo>& story_failure_classes();

//  The row of `failure_class`.
const StoryFailureClassInfo& story_failure_class_info(StoryFailureClass failure_class);

//  The class a sim fault injects. Every FaultClass has one (the tests
//  check); nothing only for one added since without a row.
std::optional<StoryFailureClass> story_failure_class_of(FaultClass fault);

//  The class of an engine Outcome failure name ("expect_failed"); nothing
//  for a name no class lists yet, like a game's own.
std::optional<StoryFailureClass> story_failure_class_of_outcome(std::string_view failure);


//  The class as a log line or run record writes it: "class=warp_no_op".
std::string story_failure_class_tag(StoryFailureClass failure_class);

//  The class tagged in `text`, a log line or run record field that holds a
//  story_failure_class_tag(); nothing when it holds none or an unknown id.
std::optional<StoryFailureClass> read_story_failure_class_tag(std::string_view text);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

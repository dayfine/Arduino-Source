/*  Story Failure Classes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstddef>
#include <format>
#include <iterator>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryFailureClasses.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr std::string_view TAG_PREFIX = "class=";

using C = StoryFailureClass;
using R = StoryFailureClassInfo::Retry;

//  The run and the console.
std::vector<StoryFailureClassInfo> run_rows(){
    return {
        {
            .failure_class = C::GAME_NOT_RUNNING, .id = "game_not_running",
            .meaning = "the Switch HOME menu shows instead of the game",
            .detected_by = "the HOME menu read at a step's start or after a failure",
            .outcomes = {"game_not_running"}, .retry = R::STOP,
        },
        {
            .failure_class = C::LAUNCH_FAILED, .id = "launch_failed",
            .meaning = "getting from HOME to the game's overworld failed",
            .detected_by = "the launch driver: no game tile, a screen that ignores its press, no overworld in time",
            .outcomes = {"launch_failed"}, .retry = R::STOP,
        },
        {
            .failure_class = C::WRONG_START, .id = "wrong_start",
            .meaning = "the game is not at the checkpoint's start: another save, an older one, another place",
            .detected_by = "the checkpoint's start checks and the preflight",
            .outcomes = {"start_failed", "wrong_save", "precondition_failed"}, .retry = R::STOP,
        },
        {
            .failure_class = C::RELOAD_FAILED, .id = "reload_failed",
            .meaning = "reloading the checkpoint's save failed",
            .detected_by = "the game's reload hook",
            .outcomes = {"reload_failed"}, .retry = R::STOP,
        },
        {
            .failure_class = C::SAVE_FAILED, .id = "save_failed",
            .meaning = "saving at the checkpoint's end failed",
            .detected_by = "the game's save hook",
            .outcomes = {"save_failed"}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::STORY_ERROR, .id = "story_error",
            .meaning = "the story's data or code is wrong: a step, range or action the program cannot run",
            .detected_by = "the engine's lint and lookups, before or as a step starts",
            .outcomes = {
                "unsupported_step", "unknown_checkpoint", "bad_range", "empty_segment",
                "bad_verify_policy", "lint_failed", "no_save_hook", "console_only",
            },
            .retry = R::STOP,
        },
    };
}

//  Walking.
std::vector<StoryFailureClassInfo> walk_rows(){
    return {
        {
            .failure_class = C::LOST_TAP, .id = "lost_tap",
            .meaning = "a tap the game drops",
            .detected_by = "the walker reads the player where the tap started",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::UNSEEN_STEP, .id = "unseen_step",
            .meaning = "a step tap that moved the player, read as not moved",
            .detected_by = "the walker finds the player a tile past where it read",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::WALK_SHORT, .id = "walk_short",
            .meaning = "a walk or hold ended short of its tile",
            .detected_by = "the walker's position check at a wall or landmark",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::BLOCKED, .id = "blocked",
            .meaning = "something on the way stops the walk: a person, a wall the map does not have",
            .detected_by = "the walker's progress check: no tile gained",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::MOUNT_LOST, .id = "mount_lost",
            .meaning = "off the Bicycle (or another mount) at a leg that rides",
            .detected_by = "the walker's tiles per tap",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::POSE_LOST, .id = "pose_lost",
            .meaning = "where the player is or faces is unknown or not what the map says",
            .detected_by = "the localizer: no pose, another map, another facing",
            .outcomes = {}, .retry = R::RELOAD,
        },
    };
}

//  Warps and fades.
std::vector<StoryFailureClassInfo> warp_rows(){
    return {
        {
            .failure_class = C::WARP_NO_OP, .id = "warp_no_op",
            .meaning = "a step onto a warp shows the fade's black but does not warp",
            .detected_by = "the map or tile after the fade is the one before it",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::LATE_FADE, .id = "late_fade",
            .meaning = "a warp's fade shows late or is missed",
            .detected_by = "the fade watcher: the warp unconfirmed, the player on a map the walk did not plan",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::SLOW_FADE_IN, .id = "slow_fade_in",
            .meaning = "a warp's fade-in runs long: a move right after it goes nowhere",
            .detected_by = "the first move after a fade does not move the player",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::DARK_MAP, .id = "dark_map",
            .meaning = "a dark map (a cave without Flash) the readers cannot see",
            .detected_by = "the floor reads dim; the walker's and the scene's reads fail there",
            .outcomes = {}, .retry = R::STOP,
        },
    };
}

//  Scenes, talks and battles.
std::vector<StoryFailureClassInfo> scene_rows(){
    return {
        {
            .failure_class = C::SCENE_END_POSE, .id = "scene_end_pose",
            .meaning = "a step whose end the game sets leaves the player on a tile next to the expected one",
            .detected_by = "the next walk starts or ends a tile off",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::SCENE_EARLY, .id = "scene_early",
            .meaning = "a scene starts on the last tiles of the walk before it",
            .detected_by = "the walk is interrupted by the scene's dialogue",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::BATTLE_OPENING, .id = "battle_opening",
            .meaning = "a battle's opening is still playing when a step starts",
            .detected_by = "the step's first read is not the overworld or battle menu it expects",
            .outcomes = {}, .retry = R::RETRY_STEP,
        },
        {
            .failure_class = C::WILD_BATTLES, .id = "wild_battles",
            .meaning = "more wild battles than a clean run meets",
            .detected_by = "the interrupt handler clears each; too many in a step fail it",
            .outcomes = {"too_many_interrupts"}, .retry = R::RETRY_STEP,
        },
    };
}

//  Interrupts, reads, menus and checks.
std::vector<StoryFailureClassInfo> check_rows(){
    return {
        {
            .failure_class = C::INTERRUPT_FAILED, .id = "interrupt_failed",
            .meaning = "something interrupted a step and was not cleared",
            .detected_by = "the interrupt handler: no handler, a failed clear, the screen not settled",
            .outcomes = {"unhandled_interrupt", "interrupt_failed", "interrupt_unsettled", "interrupted"}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::SCREEN_UNREAD, .id = "screen_unread",
            .meaning = "a screen the step waits for (dialogue, banner, overworld, prompt) is not seen",
            .detected_by = "the step's detector times out",
            .outcomes = {"talk_no_text", "mash_no_overworld"}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::MENU_FAILED, .id = "menu_failed",
            .meaning = "a menu action (mart, PC, item, TM, Fly) did not do what it was to",
            .detected_by = "the menu's readers: the item, count or choice not as planned",
            .outcomes = {}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::BATTLE_LOST, .id = "battle_lost",
            .meaning = "a battle lost or not won: a blackout",
            .detected_by = "the battle's end screen or the blackout's heal spot",
            .outcomes = {}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::STATE_CHECK_FAILED, .id = "state_check_failed",
            .meaning = "a check of the game's state failed: an event's expects, the checkpoint's end",
            .detected_by = "the event's expects and requires, the checkpoint's end and postconditions",
            .outcomes = {"expect_failed", "require_failed", "end_failed", "postconditions_failed"}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::OPERATION_FAILED, .id = "operation_failed",
            .meaning = "a game's action threw, with no class of its own",
            .detected_by = "an OperationFailedException out of a step",
            .outcomes = {"operation_failed"}, .retry = R::RELOAD,
        },
        {
            .failure_class = C::SIM_MISMATCH, .id = "sim_mismatch",
            .meaning = "a simulated run left the story: the sim and the table disagree",
            .detected_by = "the story sim's end checks (sim runs only)",
            .outcomes = {}, .retry = R::STOP,
        },
    };
}

//  Every class, in the enum's order. Function-local, so a static
//  initializer in another file may read it.
const std::vector<StoryFailureClassInfo>& class_rows(){
    static const std::vector<StoryFailureClassInfo> CLASSES = []{
        std::vector<StoryFailureClassInfo> rows;
        const auto append = [&rows](std::vector<StoryFailureClassInfo> group){
            rows.insert(rows.end(), std::make_move_iterator(group.begin()), std::make_move_iterator(group.end()));
        };
        append(run_rows());
        append(walk_rows());
        append(warp_rows());
        append(scene_rows());
        append(check_rows());
        return rows;
    }();
    return CLASSES;
}

//  The class whose id is `id`; nothing for an unknown id.
std::optional<StoryFailureClass> find_story_failure_class(std::string_view id){
    for (const StoryFailureClassInfo& info : class_rows()){
        if (info.id == id){
            return info.failure_class;
        }
    }
    return std::nullopt;
}

} // namespace


const std::vector<StoryFailureClassInfo>& story_failure_classes(){
    return class_rows();
}

const StoryFailureClassInfo& story_failure_class_info(StoryFailureClass failure_class){
    const std::vector<StoryFailureClassInfo>& classes = class_rows();
    const size_t index = static_cast<size_t>(failure_class);
    if (index >= classes.size() || classes[index].failure_class != failure_class){
        throw InternalProgramError(
            nullptr, PA_CURRENT_FUNCTION, std::format("Story failure class {} has no row in its place.", index)
        );
    }
    return classes[index];
}

std::optional<StoryFailureClass> story_failure_class_of_outcome(std::string_view failure){
    for (const StoryFailureClassInfo& info : class_rows()){
        if (std::ranges::find(info.outcomes, failure) != info.outcomes.end()){
            return info.failure_class;
        }
    }
    return std::nullopt;
}


std::string story_failure_class_tag(StoryFailureClass failure_class){
    return std::format("{}{}", TAG_PREFIX, story_failure_class_info(failure_class).id);
}

std::optional<StoryFailureClass> read_story_failure_class_tag(std::string_view text){
    const size_t start = text.find(TAG_PREFIX);
    if (start == std::string_view::npos){
        return std::nullopt;
    }
    const std::string_view rest = text.substr(start + TAG_PREFIX.size());
    const size_t end = rest.find_first_not_of("abcdefghijklmnopqrstuvwxyz_");
    return find_story_failure_class(rest.substr(0, end));
}



} // namespace Pokemon
} // namespace PokemonAutomation

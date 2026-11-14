/*  Story Failure Classes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstddef>
#include <format>
#include "Pokemon_StoryFailureClasses.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

constexpr std::string_view TAG_PREFIX = "class=";

using C = StoryFailureClass;
using R = StoryFailureClassInfo::Retry;

const std::vector<StoryFailureClassInfo> CLASSES{
    //  The run and the console.
    {
        C::GAME_NOT_RUNNING, "game_not_running",
        "the Switch HOME menu shows instead of the game",
        "the HOME menu read at a step's start or after a failure",
        {}, {"game_not_running"}, R::STOP,
    },
    {
        C::LAUNCH_FAILED, "launch_failed",
        "getting from HOME to the game's overworld failed",
        "the launch driver: no game tile, a screen that ignores its press, no overworld in time",
        {}, {"launch_failed"}, R::STOP,
    },
    {
        C::WRONG_START, "wrong_start",
        "the game is not at the checkpoint's start: another save, an older one, another place",
        "the checkpoint's start checks and the preflight",
        {}, {"start_failed", "wrong_save", "precondition_failed"}, R::STOP,
    },
    {
        C::RELOAD_FAILED, "reload_failed",
        "reloading the checkpoint's save failed",
        "the game's reload hook",
        {}, {"reload_failed"}, R::STOP,
    },
    {
        C::SAVE_FAILED, "save_failed",
        "saving at the checkpoint's end failed",
        "the game's save hook",
        {}, {"save_failed"}, R::RELOAD,
    },
    {
        C::STORY_ERROR, "story_error",
        "the story's data or code is wrong: a step, range or action the program cannot run",
        "the engine's lint and lookups, before or as a step starts",
        {},
        {
            "unsupported_step", "unknown_checkpoint", "bad_range", "empty_segment",
            "bad_verify_policy", "lint_failed", "no_save_hook", "console_only",
        },
        R::STOP,
    },

    //  Walking.
    {
        C::LOST_TAP, "lost_tap",
        "a tap the game drops",
        "the walker reads the player where the tap started",
        {FaultClass::LOST_TAP}, {}, R::RETRY_STEP,
    },
    {
        C::UNSEEN_STEP, "unseen_step",
        "a step tap that moved the player, read as not moved",
        "the walker finds the player a tile past where it read",
        {FaultClass::UNSEEN_STEP}, {}, R::RETRY_STEP,
    },
    {
        C::WALK_SHORT, "walk_short",
        "a walk or hold ended short of its tile",
        "the walker's position check at a wall or landmark",
        {}, {}, R::RETRY_STEP,
    },
    {
        C::BLOCKED, "blocked",
        "something on the way stops the walk: a person, a wall the map does not have",
        "the walker's progress check: no tile gained",
        {}, {}, R::RETRY_STEP,
    },
    {
        C::MOUNT_LOST, "mount_lost",
        "off the Bicycle (or another mount) at a leg that rides",
        "the walker's tiles per tap",
        {FaultClass::MOUNT_LOST}, {}, R::RETRY_STEP,
    },
    {
        C::POSE_LOST, "pose_lost",
        "where the player is or faces is unknown or not what the map says",
        "the localizer: no pose, another map, another facing",
        {}, {}, R::RELOAD,
    },

    //  Warps and fades.
    {
        C::WARP_NO_OP, "warp_no_op",
        "a step onto a warp shows the fade's black but does not warp",
        "the map or tile after the fade is the one before it",
        {FaultClass::WARP_NO_OP}, {}, R::RETRY_STEP,
    },
    {
        C::LATE_FADE, "late_fade",
        "a warp's fade shows late or is missed",
        "the fade watcher: the warp unconfirmed, the player on a map the walk did not plan",
        {FaultClass::LATE_FADE}, {}, R::RETRY_STEP,
    },
    {
        C::SLOW_FADE_IN, "slow_fade_in",
        "a warp's fade-in runs long: a move right after it goes nowhere",
        "the first move after a fade does not move the player",
        {FaultClass::SLOW_FADE_IN}, {}, R::RETRY_STEP,
    },
    {
        C::DARK_MAP, "dark_map",
        "a dark map (a cave without Flash) the readers cannot see",
        "the floor reads dim; the walker's and the scene's reads fail there",
        {FaultClass::DARK_CAVE}, {}, R::STOP,
    },

    //  Scenes, talks and battles.
    {
        C::SCENE_END_POSE, "scene_end_pose",
        "a step whose end the game sets leaves the player on a tile next to the expected one",
        "the next walk starts or ends a tile off",
        {FaultClass::SCENE_END_POSE}, {}, R::RETRY_STEP,
    },
    {
        C::SCENE_EARLY, "scene_early",
        "a scene starts on the last tiles of the walk before it",
        "the walk is interrupted by the scene's dialogue",
        {FaultClass::SCENE_EARLY}, {}, R::RETRY_STEP,
    },
    {
        C::BATTLE_OPENING, "battle_opening",
        "a battle's opening is still playing when a step starts",
        "the step's first read is not the overworld or battle menu it expects",
        {FaultClass::BATTLE_OPENING}, {}, R::RETRY_STEP,
    },
    {
        C::WILD_BATTLES, "wild_battles",
        "more wild battles than a clean run meets",
        "the interrupt handler clears each; too many in a step fail it",
        {FaultClass::WILD_BATTLES}, {"too_many_interrupts"}, R::RETRY_STEP,
    },
    {
        C::INTERRUPT_FAILED, "interrupt_failed",
        "something interrupted a step and was not cleared",
        "the interrupt handler: no handler, a failed clear, the screen not settled",
        {}, {"unhandled_interrupt", "interrupt_failed", "interrupt_unsettled", "interrupted"}, R::RELOAD,
    },
    {
        C::SCREEN_UNREAD, "screen_unread",
        "a screen the step waits for (dialogue, banner, overworld, prompt) is not seen",
        "the step's detector times out",
        {}, {"talk_no_text", "mash_no_overworld"}, R::RELOAD,
    },
    {
        C::MENU_FAILED, "menu_failed",
        "a menu action (mart, PC, item, TM, Fly) did not do what it was to",
        "the menu's readers: the item, count or choice not as planned",
        {}, {}, R::RELOAD,
    },
    {
        C::BATTLE_LOST, "battle_lost",
        "a battle lost or not won: a blackout",
        "the battle's end screen or the blackout's heal spot",
        {}, {}, R::RELOAD,
    },
    {
        C::STATE_CHECK_FAILED, "state_check_failed",
        "a check of the game's state failed: an event's expects, the checkpoint's end",
        "the event's expects and requires, the checkpoint's end and postconditions",
        {}, {"expect_failed", "require_failed", "end_failed", "postconditions_failed"}, R::RELOAD,
    },
    {
        C::OPERATION_FAILED, "operation_failed",
        "a game's action threw, with no class of its own",
        "an OperationFailedException out of a step",
        {}, {"operation_failed"}, R::RELOAD,
    },
    {
        C::SIM_MISMATCH, "sim_mismatch",
        "a simulated run left the story: the sim and the table disagree",
        "the story sim's end checks (sim runs only)",
        {}, {}, R::STOP,
    },
};

//  The class whose id is `id`; nothing for an unknown id.
std::optional<StoryFailureClass> find_story_failure_class(std::string_view id){
    for (const StoryFailureClassInfo& info : CLASSES){
        if (info.id == id){
            return info.failure_class;
        }
    }
    return std::nullopt;
}

} // namespace


const std::vector<StoryFailureClassInfo>& story_failure_classes(){
    return CLASSES;
}

const StoryFailureClassInfo& story_failure_class_info(StoryFailureClass failure_class){
    return CLASSES[static_cast<size_t>(failure_class)];
}

std::optional<StoryFailureClass> story_failure_class_of(FaultClass fault){
    for (const StoryFailureClassInfo& info : CLASSES){
        if (std::ranges::find(info.injected_by, fault) != info.injected_by.end()){
            return info.failure_class;
        }
    }
    return std::nullopt;
}

std::optional<StoryFailureClass> story_failure_class_of_outcome(std::string_view failure){
    for (const StoryFailureClassInfo& info : CLASSES){
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

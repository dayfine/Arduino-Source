/*  PokemonLZA AutoStory - Action table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryActionNames.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryMenuSave.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryPlatformWait.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "PokemonLZA_AutoStoryActionTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::RunState;
using Pokemon::StoryActionParser;
using Pokemon::StoryActions;
using Pokemon::StoryFailure;
using Pokemon::StoryStepRun;
using Pokemon::reject_unknown_keys;


namespace{

//  A console action run with no console: every attempt would fail the same way.
constexpr std::string_view CONSOLE_ONLY = "console_only";

//  A walk "end" that holds the last leg until a scene takes control.
constexpr std::string_view WALK_UNTIL_SCENE = "scene";

//  The other walk "end"s: held until this event.
constexpr std::pair<std::string_view, ScreenEvent> WALK_ENDS[] = {
    {"fade_white", ScreenEvent::FADE_WHITE},
    {"fade_black", ScreenEvent::FADE_BLACK},
    {"dialog", ScreenEvent::DIALOG_BOX},
    {"prompt", ScreenEvent::INTERACT_PROMPT},
};

//  The event a walk's "end" arg names; none for "scene" or an unknown word.
std::optional<ScreenEvent> walk_end_event(std::string_view end){
    for (const auto& [word, event] : WALK_ENDS){
        if (word == end){
            return event;
        }
    }
    return std::nullopt;
}


std::optional<Milliseconds> optional_ms(const JsonObject& args, const std::string& key){
    const JsonValue* value = args.get_value(key);
    if (value == nullptr){
        return std::nullopt;
    }
    return Milliseconds(value->to_integer_throw());
}

Milliseconds ms_or(const JsonObject& args, const std::string& key, Milliseconds fallback){
    return optional_ms(args, key).value_or(fallback);
}

bool flag(const JsonObject& args, const std::string& key){
    const JsonValue* value = args.get_value(key);
    return value != nullptr && value->to_boolean_throw();
}

//  An action with no args: `run` as the step's run.
template <typename Run>
Pokemon::StoryActionParser no_args(Run run){
    return [run](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {});
        return run;
    };
}


Pokemon::StoryActionParser platform_action(PlatformWait wait, Milliseconds fallback){
    return [wait, fallback](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"timeout_ms"});
        const Milliseconds timeout = ms_or(args, "timeout_ms", fallback);
        return [wait, timeout](PlzaBackend& backend, RunState&){
            return wait_for_platform(backend, timeout, wait);
        };
    };
}



//  A walk step's args.
struct WalkArgs{
    StoryMove move = {};
    //  None: the legs alone. Set: held until this event, or a scene for none.
    std::optional<std::optional<ScreenEvent>> end = std::nullopt;
    Milliseconds end_after = 0ms;
    std::optional<double> heading = std::nullopt;
    //  The first leg turns toward the chosen starter.
    bool turn_to_starter = false;
};

//  [[turn degrees, walk ms], ...]
std::vector<StoryWalkLeg> read_legs(const JsonArray& rows){
    std::vector<StoryWalkLeg> legs;
    for (const JsonValue& row : rows){
        const JsonArray& pair = row.to_array_throw();
        if (pair.size() != 2){
            throw ParseException("each leg is [turn degrees, walk ms]");
        }
        legs.push_back({.turn = (double)pair[0].to_integer_throw(), .walk = Milliseconds(pair[1].to_integer_throw())});
    }
    return legs;
}

WalkArgs read_walk_args(const JsonObject& args){
    reject_unknown_keys(args, {"legs", "end", "held", "wait_ms", "end_after_ms", "heading", "turn_to_starter"});
    WalkArgs walk;
    walk.move.legs = read_legs(args.get_array_throw("legs"));
    walk.turn_to_starter = flag(args, "turn_to_starter");
    if (walk.turn_to_starter && walk.move.legs.empty()){
        throw ParseException("\"turn_to_starter\" needs a leg to turn");
    }
    if (flag(args, "held")){
        walk.move.style = StoryWalkStyle::HELD;
    }
    walk.move.wait_first = ms_or(args, "wait_ms", 0ms);
    if (const JsonValue* end = args.get_value("end")){
        const std::string& word = end->to_string_throw();
        const std::optional<ScreenEvent> event = walk_end_event(word);
        if (word != WALK_UNTIL_SCENE && !event){
            throw ParseException("\"end\" must be \"fade_white\", \"fade_black\", \"dialog\", \"prompt\" or \"scene\"");
        }
        walk.end = event;
    }
    if (const std::optional<Milliseconds> end_after = optional_ms(args, "end_after_ms")){
        if (!walk.end){
            throw ParseException("\"end_after_ms\" needs an \"end\"");
        }
        walk.end_after = *end_after;
    }
    if (const JsonValue* heading = args.get_value("heading")){
        walk.heading = (double)heading->to_integer_throw();
    }
    return walk;
}

//  With "turn_to_starter", the first leg turns to hooks.starter_turn.
WalkArgs aimed_at_starter(PlzaConsoleHooks& hooks, WalkArgs walk){
    if (walk.turn_to_starter){
        walk.move.legs.front().turn = hooks.starter_turn;
    }
    return walk;
}

//  Starts only with free control. "heading" needs the arrow, else CONSOLE_ONLY.
std::optional<StoryFailure> walk_step(PlzaConsoleHooks& hooks, const WalkArgs& walk, PlzaBackend& backend, std::string_view name){
    if (walk.move.wait_first > 0ms){
        backend.wait(walk.move.wait_first);
    }
    if (std::optional<StoryFailure> busy = check_can_walk(backend, name)){
        return busy;
    }
    if (walk.heading){
        if (!hooks.arrow){
            return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = std::format("{}: its heading needs the minimap arrow", name), .repeats = true};
        }
        if (std::optional<StoryFailure> faced = face_heading(backend, *hooks.arrow, *walk.heading)){
            return faced;
        }
    }
    if (walk.end){
        return walk_until(backend, name, walk.move, *walk.end, walk.end_after);
    }
    std::optional<StoryFailure> walked = run_story_move(backend, walk.move);
    if (!walked){
        finish_walk(backend, name);
    }
    return walked;
}


//  01.01's arrival takes ~240 s, hence 360 s. No blind A: one at the end of
//  a talk restarts it.
Pokemon::StoryActionParser move_hints_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {
            "min_length_ms", "quiet_ms", "press_a_first", "quiet_is_control", "end_after_black_fades",
            "gap_press_ms", "advance_with_a",
        });
        SceneMachineConfig config;
        config.quiet = ms_or(args, "quiet_ms", 60s);
        config.quiet_is_control = flag(args, "quiet_is_control");
        config.min_length = ms_or(args, "min_length_ms", 0ms);
        if (flag(args, "advance_with_a")){
            config.text_button = BUTTON_A;
        }
        if (const std::optional<Milliseconds> gap = optional_ms(args, "gap_press_ms")){
            config.gap_press = *gap;
        }
        if (const JsonValue* fades = args.get_value("end_after_black_fades")){
            config.end_after_black_fades = (size_t)fades->to_integer_throw();
        }
        return [press_a_first = flag(args, "press_a_first"), config](PlzaBackend& backend, RunState& state){
            if (press_a_first){
                press_through_prompt(backend);
            }
            return run_scene_machine(backend, "mash_to_move_hints", config, 360s, state.step);
        };
    };
}

//  B, not A: an A as the text ends could land on the choice. 01.02's Pancham
//  scene takes ~60 s, hence 120 s.
Pokemon::StoryActionParser text_to_choice_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"advance_with_a"});
        SceneMachineConfig config;
        config.stop_at_choice = true;
        config.text_button = flag(args, "advance_with_a") ? BUTTON_A : BUTTON_B;
        return [config](PlzaBackend& backend, RunState& state){
            return run_scene_machine(backend, "text_to_choice", config, /*timeout=*/120s, state.step);
        };
    };
}


//  B through the text (B cannot talk again once it ends). Fails as
//  "scene_never_started" when the HUD shows 45 s with nothing else: the
//  player is not at the scene's trigger.
std::optional<StoryFailure> scene_step(PlzaBackend& backend, RunState& state){
    SceneMachineConfig config;
    config.start_window = 45s;
    return run_scene_machine(backend, "scene", config, /*timeout=*/120s, state.step);
}

//  B on every other look closes the menus a talk opens (Mable's mission and
//  reward screens), which mash_to_move_hints sits on.
std::optional<StoryFailure> mash_scene_step(PlzaBackend& backend, RunState& state){
    SceneMachineConfig config;
    config.blind_press_after = 0ms;
    config.blind_press_every = 0ms;
    config.blind_button = BUTTON_B;
    return run_scene_machine(backend, "scene", config, /*timeout=*/120s, state.step);
}


//  As long as the start's lock-on holds ZL.
constexpr Milliseconds BATTLE_LOCK_ON_HOLD = 2s;
constexpr Milliseconds BATTLE_START_POLL = 250ms;
//  Fails the step in seconds, not after 2 min of looks on the overworld.
constexpr Seconds BATTLE_START_WAIT = 10s;

//  A start that locked on and let go of ZL can leave the HUD down until ZL
//  is held again, so `lock_on` also counts the move symbols under ZL.
bool battle_on_within(PlzaBackend& backend, const std::optional<LockOnWatch>& lock_on, Milliseconds within){
    for (Milliseconds waited = 0ms; ; ){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::BATTLE_STARTED)){
            return true;
        }
        if (waited >= within){
            return false;
        }
        if (!lock_on){
            backend.wait(BATTLE_START_POLL);
            waited += BATTLE_START_POLL;
            continue;
        }
        if ((*lock_on)(BATTLE_LOCK_ON_HOLD)){
            backend.log("[PLZA] story_battle: no battle HUD; ZL held: the move symbols showed.");
            return true;
        }
        waited += BATTLE_LOCK_ON_HOLD;
    }
}

std::optional<StoryFailure> battle_step(PlzaConsoleHooks& hooks, PlzaBackend& backend){
    if (!battle_on_within(backend, hooks.lock_on, BATTLE_START_WAIT)){
        return StoryFailure{
            .code = "battle_never_started",
            .detail = std::format(
                "no battle HUD{} within {} s", hooks.lock_on ? " and no move symbols under ZL" : "", BATTLE_START_WAIT.count()
            ),
        };
    }
    if (!hooks.battle){
        return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = "no battle code off a console", .repeats = true};
    }
    return hooks.battle(backend);
}


//  Blind B: no detector reads the Pokedex screens yet, and only B closes
//  them (A plays the entry's motions).
std::optional<StoryFailure> pokedex_step(PlzaBackend& backend, RunState& state){
    SceneMachineConfig config;
    config.text_button = BUTTON_B;
    config.blind_press_after = 0ms;
    config.blind_button = BUTTON_B;
    return run_scene_machine(backend, "talk_to_pokedex_screen", config, /*timeout=*/90s, state.step);
}


//  A console action: the console's code when `hooks` has it, else
//  CONSOLE_ONLY. `timeout`: the step's "timeout_ms" fallback; none: no args.
StoryActionParser console_action(PlzaConsoleHooks& hooks, std::string_view name, std::optional<Milliseconds> timeout = std::nullopt){
    return [&hooks, name = std::string(name), timeout](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, timeout ? std::initializer_list<std::string_view>{"timeout_ms"} : std::initializer_list<std::string_view>{});
        const Milliseconds limit = ms_or(args, "timeout_ms", timeout.value_or(0ms));
        return [&hooks, name, limit](PlzaBackend& backend, RunState&) -> std::optional<StoryFailure>{
            const auto action = hooks.console.find(name);
            if (action == hooks.console.end()){
                return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = std::format("{} runs only on a console", name), .repeats = true};
            }
            return action->second(backend, limit);
        };
    };
}

StoryActionParser walk_action(PlzaConsoleHooks& hooks){
    return [&hooks](const JsonObject& args) -> StoryStepRun{
        return [&hooks, walk = read_walk_args(args)](PlzaBackend& backend, RunState&){
            return walk_step(hooks, aimed_at_starter(hooks, walk), backend, "walk");
        };
    };
}

std::optional<StoryFailure> story_battle_start_step(PlzaConsoleHooks& hooks, PlzaBackend& backend, RunState& state){
    SceneMachineConfig config;
    config.stop_at_battle = true;
    config.lock_on = hooks.lock_on;
    return run_scene_machine(backend, "story_battle_start", config, /*timeout=*/90s, state.step);
}

std::optional<StoryFailure> menu_save_step(PlzaConsoleHooks& hooks, PlzaBackend& backend){
    std::optional<StoryFailure> failure = save_from_menu(backend);
    if (!failure && hooks.sightings != nullptr){
        hooks.sightings->saved(backend);
    }
    return failure;
}

//  The actions that need no console.
void add_backend_actions(StoryActions& actions, PlzaConsoleHooks& hooks){
    std::pair<std::string_view, StoryActionParser> table[] = {
        {action_name(PlzaAction::WAIT_FOR_PLATFORM), platform_action(PlatformWait::PLATFORM_ONLY, /*fallback=*/180s)},
        {action_name(PlzaAction::OPENING_TO_PLATFORM), platform_action(PlatformWait::AFTER_AUTOSAVE, /*fallback=*/240s)},
        {action_name(PlzaAction::WALK), walk_action(hooks)},
        {action_name(PlzaAction::TALK), no_args([](PlzaBackend& backend, RunState&){ return talk_at_prompt(backend); })},
        {action_name(PlzaAction::TALK_THROUGH_MAP_SCREEN), no_args([](PlzaBackend& backend, RunState&){ return talk_through_map_screen(backend); })},
        {action_name(PlzaAction::TALK_TO_POKEDEX_SCREEN), no_args(pokedex_step)},
        {action_name(PlzaAction::CLIMB_LADDER), no_args([](PlzaBackend& backend, RunState&){ return climb_ladder(backend); })},
        {action_name(PlzaAction::CHOOSE_0), no_args([](PlzaBackend& backend, RunState&){ return choose_row(backend, 0); })},
        {action_name(PlzaAction::CHOOSE_1), no_args([](PlzaBackend& backend, RunState&){ return choose_row(backend, 1); })},
        {action_name(PlzaAction::SCENE), no_args(scene_step)},
        {action_name(PlzaAction::MASH), no_args(mash_scene_step)},
        {action_name(PlzaAction::MASH_TO_MOVE_HINTS), move_hints_action()},
        {action_name(PlzaAction::TEXT_TO_CHOICE), text_to_choice_action()},
        {action_name(PlzaAction::STORY_BATTLE_START), no_args([&hooks](PlzaBackend& backend, RunState& state){
            return story_battle_start_step(hooks, backend, state);
        })},
        {action_name(PlzaAction::STORY_BATTLE), no_args([&hooks](PlzaBackend& backend, RunState&){ return battle_step(hooks, backend); })},
        {action_name(PlzaAction::SAVE_FROM_MENU), no_args([&hooks](PlzaBackend& backend, RunState&){ return menu_save_step(hooks, backend); })},
        {action_name(PlzaAction::SET_TEXT_SPEED_FAST), no_args([](PlzaBackend& backend, RunState&){ return set_text_speed_fast(backend); })},
    };
    for (auto& [name, parser] : table){
        actions.emplace(name, std::move(parser));
    }
}

//  The actions that run only on a console, bound to `hooks.console`.
void add_console_actions(StoryActions& actions, PlzaConsoleHooks& hooks){
    std::pair<std::string_view, StoryActionParser> table[] = {
        {action_name(PlzaAction::RUN_TO_GATE), console_action(hooks, action_name(PlzaAction::RUN_TO_GATE), /*timeout=*/8s)},
        {action_name(PlzaAction::AIMED_RUN_TO_GATE), console_action(hooks, action_name(PlzaAction::AIMED_RUN_TO_GATE), /*timeout=*/4s)},
        {action_name(PlzaAction::NEW_GAME_INTRO), console_action(hooks, action_name(PlzaAction::NEW_GAME_INTRO))},
        {action_name(PlzaAction::CONFIRM_NEW_GAME), console_action(hooks, action_name(PlzaAction::CONFIRM_NEW_GAME))},
        {action_name(PlzaAction::CATCH_TUTORIAL), console_action(hooks, action_name(PlzaAction::CATCH_TUTORIAL))},
        {action_name(PlzaAction::BATTLE_THEN_CATCH), console_action(hooks, action_name(PlzaAction::BATTLE_THEN_CATCH))},
        {action_name(PlzaAction::CATCH_IN_ZONE), console_action(hooks, action_name(PlzaAction::CATCH_IN_ZONE))},
        {action_name(PlzaAction::WALK_BACK_TO_ZONE_ROAD), console_action(hooks, action_name(PlzaAction::WALK_BACK_TO_ZONE_ROAD))},
        {action_name(PlzaAction::CHECK_OUT_OF_ZONE_1), console_action(hooks, action_name(PlzaAction::CHECK_OUT_OF_ZONE_1))},
        {action_name(PlzaAction::WALK_TO_UNREAD_MARKER), console_action(hooks, action_name(PlzaAction::WALK_TO_UNREAD_MARKER))},
        {action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD), console_action(hooks, action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD))},
    };
    for (auto& [name, parser] : table){
        actions.emplace(name, std::move(parser));
    }
}

} // namespace


StoryActions plza_story_actions(PlzaConsoleHooks& hooks){
    StoryActions actions;
    add_backend_actions(actions, hooks);
    add_console_actions(actions, hooks);
    return actions;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

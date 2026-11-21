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
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenMatch.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenWait.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStorySceneMachine.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryWalks.h"
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

//  A walk "end" that holds the last leg until a scene takes control.
constexpr std::string_view WALK_UNTIL_SCENE = "scene";

//  The other walk "end"s: held until this event.
constexpr std::pair<std::string_view, ScreenEvent> WALK_ENDS[] = {
    {"fade_white", ScreenEvent::FADE_WHITE},
    {"fade_black", ScreenEvent::FADE_BLACK},
    {"dialog", ScreenEvent::DIALOG_BOX},
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


using PlatformStep = std::optional<StoryFailure> (*)(PlzaBackend&, Milliseconds);

Pokemon::StoryActionParser platform_action(PlatformStep step, Milliseconds fallback){
    return [step, fallback](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"timeout_ms"});
        const Milliseconds timeout = ms_or(args, "timeout_ms", fallback);
        return [step, timeout](PlzaBackend& backend, RunState&){
            return step(backend, timeout);
        };
    };
}


//  A walk step's args.
struct WalkArgs{
    StoryMove move = {};
    //  None: the legs alone. Set: held until this event, or a scene for none.
    std::optional<std::optional<ScreenEvent>> end = std::nullopt;
    Milliseconds end_after = 0ms;
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
    reject_unknown_keys(args, {"legs", "end", "held", "wait_ms", "end_after_ms"});
    WalkArgs walk;
    walk.move.legs = read_legs(args.get_array_throw("legs"));
    if (flag(args, "held")){
        walk.move.style = StoryWalkStyle::HELD;
    }
    walk.move.wait_first = ms_or(args, "wait_ms", 0ms);
    if (const JsonValue* end = args.get_value("end")){
        const std::string& word = end->to_string_throw();
        const std::optional<ScreenEvent> event = walk_end_event(word);
        if (word != WALK_UNTIL_SCENE && !event){
            throw ParseException("\"end\" must be \"fade_white\", \"fade_black\", \"dialog\", or \"scene\"");
        }
        walk.end = event;
    }
    if (const std::optional<Milliseconds> end_after = optional_ms(args, "end_after_ms")){
        if (!walk.end){
            throw ParseException("\"end_after_ms\" needs an \"end\"");
        }
        walk.end_after = *end_after;
    }
    return walk;
}

//  Starts only with free control.
std::optional<StoryFailure> walk_step(const WalkArgs& walk, PlzaBackend& backend, std::string_view name){
    if (walk.move.wait_first > 0ms){
        backend.wait(walk.move.wait_first);
    }
    if (std::optional<StoryFailure> busy = check_can_walk(backend, name)){
        return busy;
    }
    if (walk.end){
        return walk_until(backend, name, walk.move, {.end = *walk.end, .end_after = walk.end_after});
    }
    run_story_move(backend, walk.move);
    finish_walk(backend, name);
    return std::nullopt;
}


constexpr Milliseconds SCENE_TIMEOUT = 360s;

Pokemon::StoryActionParser scene_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, {"end_on_black_fade"});
        const SceneMachineConfig config{
            .end_on_black_fade = flag(args, "end_on_black_fade"),
        };
        return [config](PlzaBackend& backend, RunState&){
            return run_scene_machine(backend, "scene", config, SCENE_TIMEOUT);
        };
    };
}


constexpr Milliseconds PLUS_SKIP_POLL = 250ms;
//  Console B, 2026-11-19: Plus 3.0-3.4 s after the opening fade ended
//  skipped 01.01's ad; the skip's black fade showed 2.2-2.8 s after the press started.
constexpr Milliseconds PLUS_SKIP_AFTER_FADE = 2500ms;
constexpr Milliseconds PLUS_SKIP_FADE_WITHIN = 5000ms;
constexpr Milliseconds PLUS_SKIP_FADE_END_WAIT = 15s;

const Pokemon::ScreenMatch NOT_BLACK_FADE{
    .no_events = {ScreenEvent::FADE_BLACK},
};
const Pokemon::ScreenMatch BLACK_FADE{
    .all_events = {ScreenEvent::FADE_BLACK},
};

//  From a cutscene's opening black fade: once it ends, Plus, then the
//  skip's black fade. Fails as "fade_not_ended" or "cutscene_not_skipped".
std::optional<StoryFailure> plus_skip_step(PlzaBackend& backend){
    const Pokemon::WaitOutcome fade_end = Pokemon::wait_for(
        backend,
        NOT_BLACK_FADE,
        Pokemon::WaitRule{.poll = PLUS_SKIP_POLL, .timeout = PLUS_SKIP_FADE_END_WAIT}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&fade_end)){
        return *failure;
    }
    if (!std::holds_alternative<Pokemon::WaitMatched>(fade_end)){
        return StoryFailure{.code = "fade_not_ended", .detail = "plus_skip: the black fade did not end within 15 s"};
    }
    backend.wait(PLUS_SKIP_AFTER_FADE);
    backend.press(BUTTON_PLUS, CUTSCENE_SKIP_HOLD, /*release=*/0ms);
    const Pokemon::WaitOutcome skipped = Pokemon::wait_for(
        backend,
        BLACK_FADE,
        Pokemon::WaitRule{.poll = PLUS_SKIP_POLL, .timeout = PLUS_SKIP_FADE_WITHIN}
    );
    if (const auto* failure = std::get_if<StoryFailure>(&skipped)){
        return *failure;
    }
    if (const auto* matched = std::get_if<Pokemon::WaitMatched>(&skipped)){
        backend.log(std::format("[PLZA] plus_skip: a black fade {} ms after the hold.", matched->elapsed.count()));
        return std::nullopt;
    }
    const std::string detail = std::format("plus_skip: no black fade within {} ms after the hold.", PLUS_SKIP_FADE_WITHIN.count());
    backend.log(std::format("[PLZA] {}", detail));
    return StoryFailure{.code = "cutscene_not_skipped", .detail = detail};
}

StoryActionParser walk_action(){
    return [](const JsonObject& args) -> StoryStepRun{
        return [walk = read_walk_args(args)](PlzaBackend& backend, RunState&) -> std::optional<StoryFailure>{
            return walk_step(walk, backend, "walk");
        };
    };
}

} // namespace


StoryActions plza_story_actions(){
    StoryActions actions;
    std::pair<std::string_view, StoryActionParser> table[] = {
        {action_name(PlzaAction::WAIT_FOR_PLATFORM), platform_action(wait_for_platform, /*fallback=*/180s)},
        {action_name(PlzaAction::WALK), walk_action()},
        {action_name(PlzaAction::SCENE), scene_action()},
        {action_name(PlzaAction::PLUS_SKIP), no_args([](PlzaBackend& backend, RunState&){ return plus_skip_step(backend); })},
        {action_name(PlzaAction::SET_TEXT_SPEED_FAST), no_args([](PlzaBackend& backend, RunState&){ return set_text_speed_fast(backend); })},
    };
    for (auto& [name, parser] : table){
        actions.emplace(name, std::move(parser));
    }
    return actions;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

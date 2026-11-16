/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Outcome;
using Pokemon::StoryActionArgs;
using Pokemon::StoryActionCall;
using Pokemon::StoryActionSpec;
using Pokemon::StoryArgSpec;
using Pokemon::StoryArgType;
using Pokemon::StoryStepKind;
using Pokemon::ZonePose;
using Handler = Pokemon::StoryActionHandler<ZonePose>;


namespace{


StoryActionSpec game_spec(std::string name, std::vector<StoryArgSpec> args = {}){
    return StoryActionSpec{.kind = StoryStepKind::GAME, .name = std::move(name), .args = std::move(args)};
}

StoryArgSpec optional_ms(std::string key, int64_t fallback, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::INTEGER, .required = false, .fallback = fallback, .why = std::move(why)};
}

//  An integer that has no value when left out.
StoryArgSpec optional_int(std::string key, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::INTEGER, .required = false, .fallback = std::nullopt, .why = std::move(why)};
}

std::vector<StoryActionSpec> make_specs(){
    StoryActionSpec confirm = game_spec("confirm_new_game");
    confirm.console_only = true;
    return {
        std::move(confirm),
        game_spec("wait_for_platform", {
            optional_ms("timeout_ms", 180000, "no platform this long fails the step"),
        }),
        game_spec("opening_to_platform", {
            optional_ms("timeout_ms", 240000, "no autosave and platform this long fails the step"),
        }),
        game_spec("walk", {
            StoryArgSpec{
                .key = "legs", .type = StoryArgType::TIMED_LEGS, .required = true, .fallback = std::nullopt,
                .why = "the legs: [turn in degrees, clockwise (negative: left), walk ms] each",
            },
            StoryArgSpec{
                .key = "end", .type = StoryArgType::WORD, .required = false, .fallback = std::nullopt,
                .why = "\"fade_white\", \"fade_black\" or \"dialog\": the last leg is held until it shows, its ms the bound",
            },
        }),
        game_spec("mash_to_move_hints", {
            optional_int("end_after_black_fades", "end 5 s after this many black fades, or on the controls hint; the banner does not end it"),
        }),
    };
}


//  The game onto its latest save by reset_game_from_home(). Fails as
//  "launch_failed".
Outcome reload_latest_save(StoryEnv& env){
    go_home(env.env.console, env.context);
    if (!reset_game_from_home(env.env, env.env.console, env.context, /*backup_save=*/false)){
        return Pokemon::outcome_failed("launch_failed", "the game did not reach the overworld after the reload");
    }
    return Pokemon::outcome_ok();
}


//  "confirm_new_game" on the console `env`; none: fails as console_only.
//  An OperationFailedException becomes a failed Outcome, so the run can
//  report it.
Handler confirm_handler(StoryEnv* env){
    if (env == nullptr){
        return [](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
            return Pokemon::outcome_failed(std::string(Pokemon::CONSOLE_ONLY), "confirm_new_game runs only on a console");
        };
    }
    return [env](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
        try{
            confirm_new_game_choices(env->env.console, env->context);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] confirm_new_game failed: {}", e.message()));
            return Pokemon::outcome_failed("confirm_new_game_failed", e.message());
        }
        return Pokemon::outcome_ok();
    };
}


//  What a platform wait needs before the platform counts.
enum class PlatformWait{
    PLATFORM_ONLY,      //  "wait_for_platform": a start check
    AFTER_AUTOSAVE,     //  "opening_to_platform": the autosave badge first
};

//  Looks every PLATFORM_POLL: the autosave badge shows for 1-3 s,
//  so at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

//  One platform wait's progress over its looks.
struct PlatformProgress{
    bool autosave_seen = false;
    size_t streak = 0;
    //  The text with the advance arrow was on the last look.
    bool text_waiting = false;
};

//  Takes `look` into `progress`; logs the autosave when it shows. True once
//  the platform has held PLATFORM_LOOKS looks.
bool platform_look(Pokemon::StoryBackend<ZonePose>& backend, const ScreenObservation& look, PlatformWait wait, PlatformProgress& progress){
    if (look.readings.contains(AUTOSAVE_READING)){
        if (!progress.autosave_seen){
            backend.log("[PLZA] platform wait: the autosave badge is on screen");
        }
        progress.autosave_seen = true;
        progress.streak = 0;
        return false;
    }
    const bool counts = wait == PlatformWait::PLATFORM_ONLY || progress.autosave_seen;
    progress.streak = counts && look.readings.contains(PLATFORM_READING) ? progress.streak + 1 : 0;
    return progress.streak >= PLATFORM_LOOKS;
}

//  A text box with the advance arrow: the opening's last line ("Your train
//  has arrived in Lumiose City!") waits for A.
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

//  The station platform after the opening cutscene: PLATFORM_READING on
//  PLATFORM_LOOKS looks in a row; AFTER_AUTOSAVE counts them only once the
//  autosave badge has shown and gone. No input through the cutscene (A does
//  not advance it), but one A for each text box with the advance arrow, when
//  it shows (not again while the same one is up). Fails as
//  "platform_not_seen" after `timeout`.
Outcome wait_for_platform(Pokemon::StoryBackend<ZonePose>& backend, Milliseconds timeout, PlatformWait wait){
    //  The looks bound a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / PLATFORM_POLL) + 1;
    std::optional<WallClock> start;
    PlatformProgress progress;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<ZonePose> look = backend.observe();
        start = start.value_or(look.when);
        if (platform_look(backend, look, wait, progress)){
            backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", looks + 1));
            return Pokemon::outcome_ok();
        }
        if (look.when - *start >= timeout){
            break;
        }
        const bool text_now = text_waiting_for_a(look);
        if (text_now && !progress.text_waiting){
            backend.log("[PLZA] platform wait: text waiting for A; A");
            backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/400ms);
        }
        progress.text_waiting = text_now;
        backend.wait(PLATFORM_POLL);
    }
    return Pokemon::outcome_failed("platform_not_seen", std::format(
        "No platform on {} looks in a row within {} ms (autosave {}).",
        PLATFORM_LOOKS, timeout.count(), progress.autosave_seen ? "seen" : "not seen"
    ));
}

//  The handler of "wait_for_platform" or "opening_to_platform".
Handler platform_handler(PlatformWait wait){
    return [wait](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
        return wait_for_platform(backend, Milliseconds(Pokemon::story_arg<int64_t>(call.args, "timeout_ms")), wait);
    };
}


//  The walk of `args`: the legs of its arg "legs", each [turn in degrees
//  clockwise (negative: left), walk ms]. The args must have passed the
//  walk's spec.
StoryMove story_walk_move(const StoryActionArgs& args){
    StoryMove move;
    for (const Pokemon::StoryTimedLeg& row : Pokemon::story_arg<std::vector<Pokemon::StoryTimedLeg>>(args, "legs")){
        move.legs.push_back({.turn = (double)row.turn, .walk = row.walk});
    }
    return move;
}

//  "walk": its legs, once the screen shows free control; held until the
//  event of its "end" arg, if it has one, else finished with one look
//  (finish_walk()).
Outcome walk_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
    const std::string_view name = call.step.name;
    const Outcome free = check_can_walk(backend, name);
    if (!free.succeeded()){
        return free;
    }
    const StoryMove move = story_walk_move(call.args);
    const auto end = call.args.find("end");
    if (end != call.args.end()){
        const std::optional<ScreenEvent> event = walk_end_event(std::get<std::string>(end->second));
        if (event){
            return walk_until(backend, name, move, *event);
        }
    }
    const Outcome walked = run_story_move(backend, move);
    if (walked.succeeded()){
        finish_walk(backend, name);
    }
    return walked;
}


//  "mash_to_move_hints": A through the scenes (a choice takes its top
//  row, the advance chevron its text) until the player has control: the
//  controls hint held a second, or a main mission's banner on two looks
//  (the step returns once it has gone, 10 s at most). With
//  "end_after_black_fades", that many black fades lifted and settled end
//  it, 5 s after the last one (or the controls hint, if it comes first),
//  instead of the banner: 01.01's arrival goes ad, loading, a fade from
//  black, the rival's talk, a second black fade, Pancham, the "MAIN
//  MISSION 01" banner, a third black fade, then control (console B,
//  2026-11-15, English and Italian; the banner detector missed the banner
//  on the Italian game). The next step's walk waits for control (check_can_walk()).
//  01.01's arrival takes ~240 s on the console, hence
//  360 s. A text-free cutscene (01.01's city flyover, ~20 s) is not
//  control: a minute with nothing busy fails the step. After 5 s with
//  nothing busy and no sign of control, A every 2 s, for text the detectors
//  miss (01.01's ad).
Outcome move_hints_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
    SceneMachineConfig config;
    config.quiet = 60s;
    config.blind_press_after = 5s;
    const auto fades = call.args.find("end_after_black_fades");
    if (fades != call.args.end()){
        config.end_after_black_fades = (size_t)std::get<int64_t>(fades->second);
    }
    return run_scene_machine(backend, "mash_to_move_hints", config, 360s);
}


//  The handler of each of plza_story_specs(), by name.
Handler plza_handler(StoryEnv* env, const StoryActionSpec& spec){
    if (spec.name == "confirm_new_game"){
        return confirm_handler(env);
    }
    if (spec.name == "wait_for_platform"){
        return platform_handler(PlatformWait::PLATFORM_ONLY);
    }
    if (spec.name == "opening_to_platform"){
        return platform_handler(PlatformWait::AFTER_AUTOSAVE);
    }
    if (spec.name == "mash_to_move_hints"){
        return move_hints_step;
    }
    return walk_step;
}

//  PLZA's actions over `env` (none: no console), with no save or reload.
Pokemon::StoryGame<ZonePose> plza_game_actions(StoryEnv* env){
    Pokemon::StoryGame<ZonePose> game;
    game.id = "plza";
    for (StoryActionSpec spec : make_specs()){
        Handler handler = plza_handler(env, spec);
        game.actions.push_back({.spec = std::move(spec), .handler = std::move(handler)});
    }
    return game;
}

const Pokemon::StoryActionRegistry<ZonePose>& spec_registry(){
    static const Pokemon::StoryGame<ZonePose> game = plza_game_actions(nullptr);
    static const Pokemon::StoryActionRegistry<ZonePose> registry(game);
    return registry;
}


} // namespace


const std::vector<StoryActionSpec>& plza_story_specs(){
    return spec_registry().specs();
}

std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint){
    std::vector<std::string> errors = spec_registry().lint(checkpoint);
    if (!errors.empty()){
        return errors;
    }
    for (const Pokemon::StoryStep& step : checkpoint.steps){
        const auto end = step.args.find("end");
        if (step.action != "walk" || end == step.args.end()){
            continue;
        }
        const std::string* word = std::get_if<std::string>(&end->second);
        if (word == nullptr || !walk_end_event(*word)){
            errors.push_back(std::format("{} {}: \"end\" must be \"fade_white\", \"fade_black\" or \"dialog\"", checkpoint.id, step.name));
        }
    }
    return errors;
}

std::optional<ScreenEvent> walk_end_event(std::string_view end){
    if (end == "fade_white"){
        return ScreenEvent::FADE_WHITE;
    }
    if (end == "fade_black"){
        return ScreenEvent::FADE_BLACK;
    }
    if (end == "dialog"){
        return ScreenEvent::DIALOG_BOX;
    }
    return std::nullopt;
}

Pokemon::StoryGame<ZonePose> make_plza_story_game(
    StoryEnv& env, std::set<std::string, std::less<>> autosave_rows
){
    Pokemon::StoryGame<ZonePose> game = plza_game_actions(&env);
    game.save = [autosave_rows = std::move(autosave_rows)](Pokemon::StoryBackend<ZonePose>& backend, const Pokemon::StoryCheckpoint& checkpoint){
        if (autosave_rows.contains(checkpoint.id)){
            backend.log(std::format("[Save] {}: the game autosaved at its end", checkpoint.id));
            return Pokemon::outcome_ok();
        }
        return Pokemon::outcome_failed("no_autosave", "the row does not end on an autosave a run log has shown");
    };
    game.reload = [&env](Pokemon::StoryBackend<ZonePose>&){
        return reload_latest_save(env);
    };
    return game;
}

Pokemon::StoryGame<ZonePose> make_plza_story_game_without_console(){
    return plza_game_actions(nullptr);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

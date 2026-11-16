/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <cstdlib>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryFiles.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_MenuNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryCatches.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLines.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMarkerWalk.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryScenes.h"
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

//  A flag that is off when left out.
StoryArgSpec flag(std::string key, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::BOOLEAN, .required = false, .fallback = false, .why = std::move(why)};
}

//  An integer that has no value when left out.
StoryArgSpec optional_int(std::string key, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::INTEGER, .required = false, .fallback = std::nullopt, .why = std::move(why)};
}

//  `spec`, run only on a console.
StoryActionSpec console_only(StoryActionSpec spec){
    spec.console_only = true;
    return spec;
}

//  The GAME actions that run only on a console.
std::vector<StoryActionSpec> console_specs(){
    return {
        console_only(game_spec("confirm_new_game")),
        console_only(game_spec("run_to_gate", {
            optional_ms("timeout_ms", 8000, "running this long with no A prompt fails the step"),
        })),
        console_only(game_spec("catch_tutorial")),
        console_only(game_spec("battle_then_catch")),
        console_only(game_spec("walk_to_marker")),
        console_only(game_spec("claim_mable_research_reward")),
    };
}

std::vector<StoryActionSpec> make_specs(){
    std::vector<StoryActionSpec> specs{
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
            flag("held", "hold the stick at each leg's angle (L only before a later turning leg), not tap-and-forward"),
            optional_int("wait_ms", "wait this long before looking for free control: NPCs moving off"),
        }),
        game_spec("walk_to_prompt", {
            optional_ms("timeout_ms", 10000, "walking this long with no A prompt fails the step"),
        }),
        game_spec("mash_to_move_hints", {
            optional_ms("min_length_ms", 0, "the scene's rough length: no quiet failure before it"),
            optional_ms("quiet_ms", 60000, "nothing busy and no control this long fails the step"),
            flag("press_a_first", "press A first, at the prompt that starts the scene (a door's \"A Enter\")"),
            flag("quiet_is_control", "the quiet time ends the step with control, where no party bar or prompt shows (Hotel Z)"),
            optional_int("end_after_black_fades", "end 5 s after this many black fades, or on the controls hint; the banner does not end it"),
        }),
        game_spec("text_to_choice", {
            flag("advance_with_a", "advance the text with A, for a talk that B cancels (Hotel Z's elevator)"),
        }),
        game_spec("choose_0"),
        game_spec("choose_1"),
        StoryActionSpec{.kind = StoryStepKind::TALK, .name = "talk", .args = {}},
        StoryActionSpec{.kind = StoryStepKind::MASH, .name = "a", .args = {}},
        game_spec("story_battle_start"),
        game_spec("story_battle"),
        game_spec("talk_through_map_screen"),
        game_spec("climb_ladder"),
        game_spec("talk_to_pokedex_screen"),
    };
    for (StoryActionSpec& spec : console_specs()){
        specs.push_back(std::move(spec));
    }
    return specs;
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


//  "run_to_gate" on the console `env`; none: fails as console_only. Runs
//  straight on with upstream's run_towards_gate_with_A_button() until the
//  gate's A prompt; once more after a day/night change, which stops the run.
//  Fails as "gate_not_reached" when the run ends with no prompt.
Handler gate_run_handler(StoryEnv* env){
    if (env == nullptr){
        return [](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
            return Pokemon::outcome_failed(std::string(Pokemon::CONSOLE_ONLY), "run_to_gate runs only on a console");
        };
    }
    return [env](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
        const Milliseconds timeout(Pokemon::story_arg<int64_t>(call.args, "timeout_ms"));
        //  0: the A prompt, 1: a day/night change, else no prompt.
        int result = 1;
        try{
            for (size_t runs = 0; runs < 2 && result == 1; runs++){
                result = run_towards_gate_with_A_button(
                    env->env.console, env->context, /*run_direction_x=*/0, /*run_direction_y=*/1, timeout
                );
            }
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] run_to_gate failed: {}", e.message()));
            return Pokemon::outcome_failed("gate_not_reached", e.message());
        }
        if (result != 0){
            return Pokemon::outcome_failed("gate_not_reached", std::format("no gate A prompt in {} ms of running", timeout.count()));
        }
        return Pokemon::outcome_ok();
    };
}


//  03.11's reward: the main menu (overworld_to_main_menu()), Mable's
//  Research three rows down, A. The page opens on Rewards under her lines on
//  the level 2 reward, TM004 Rock Smash (console B, 2026-11-10): A through
//  them, then B out of the menu to her talk. No line at all: the page did
//  not open on her reward (03.10 r6, console B 2026-11-14: not in her
//  office). Fails as "no_reward_lines", "reward_lines_endless" or
//  "menu_not_closed".
Outcome claim_research_reward(StoryEnv& env, PlzaBackend& backend){
    overworld_to_main_menu(env.env.console, env.context);
    for (int i = 0; i < 3; i++){
        backend.press(BUTTON_DOWN, /*hold=*/160ms, /*release=*/400ms);
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/2840ms);
    const LinesPassed passed = press_a_through_lines(backend, /*quiet=*/3s, /*limit=*/60s);
    if (passed.timed_out){
        return Pokemon::outcome_failed("reward_lines_endless", "Mable's lines did not end within 60 s");
    }
    if (!passed.text){
        return Pokemon::outcome_failed("no_reward_lines", "no dialog after opening Mable's Research: no reward was claimed");
    }
    if (!press_b_out_of_menu(backend, /*presses=*/60)){
        return Pokemon::outcome_failed("menu_not_closed", "no party bar or talk after 60 B presses out of the menu");
    }
    return Pokemon::outcome_ok();
}

//  A console action's body.
using ConsoleStep = Outcome (*)(StoryEnv& env, PlzaBackend& backend);

//  The console actions run by a ConsoleStep, by name: console_only.
const std::map<std::string_view, ConsoleStep>& console_steps(){
    static const std::map<std::string_view, ConsoleStep> steps{
        {"catch_tutorial", [](StoryEnv& env, PlzaBackend& backend){
            return catch_tutorial(env, backend, /*max_throws=*/6);
        }},
        {"battle_then_catch", [](StoryEnv& env, PlzaBackend& backend){
            return battle_then_catch(env, backend, /*max_rounds=*/6);
        }},
        {"walk_to_marker", walk_to_marker},
        {"claim_mable_research_reward", claim_research_reward},
    };
    return steps;
}

//  `step` on the console `env`; none: fails as console_only. An
//  OperationFailedException becomes a failed Outcome, so the run can report
//  it.
Handler console_handler(StoryEnv* env, std::string_view name, ConsoleStep step){
    if (env == nullptr){
        return [name = std::string(name)](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
            return Pokemon::outcome_failed(std::string(Pokemon::CONSOLE_ONLY), std::format("{} runs only on a console", name));
        };
    }
    return [env, name = std::string(name), step](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
        try{
            return step(*env, backend);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
            return Pokemon::outcome_failed("operation_failed", e.message());
        }
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
//  clockwise (negative: left), walk ms], held with "held", after the wait
//  "wait_ms". The args must have passed the walk's spec.
StoryMove story_walk_move(const StoryActionArgs& args){
    StoryMove move;
    const auto held = args.find("held");
    if (held != args.end() && std::get<bool>(held->second)){
        move.style = StoryWalkStyle::HELD;
    }
    const auto wait = args.find("wait_ms");
    if (wait != args.end()){
        move.wait_first = Milliseconds(std::get<int64_t>(wait->second));
    }
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
    const StoryMove move = story_walk_move(call.args);
    if (move.wait_first > Milliseconds(0)){
        backend.wait(move.wait_first);
    }
    const Outcome free = check_can_walk(backend, name);
    if (!free.succeeded()){
        return free;
    }
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
//  "min_length_ms" holds the quiet failure off for a long still scene;
//  "quiet_ms" is the quiet time, which with "quiet_is_control" ends the
//  step with control (Hotel Z shows no party bar). "press_a_first" presses
//  A at the prompt the scene starts from (press_through_prompt()), which
//  would otherwise read as control at once.
Outcome move_hints_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
    if (Pokemon::story_arg<bool>(call.args, "press_a_first")){
        press_through_prompt(backend);
    }
    SceneMachineConfig config;
    config.quiet = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "quiet_ms"));
    config.quiet_is_control = Pokemon::story_arg<bool>(call.args, "quiet_is_control");
    config.min_length = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "min_length_ms"));
    config.blind_press_after = 5s;
    const auto fades = call.args.find("end_after_black_fades");
    if (fades != call.args.end()){
        config.end_after_black_fades = (size_t)std::get<int64_t>(fades->second);
    }
    return run_scene_machine(backend, "mash_to_move_hints", config, 360s);
}

//  "text_to_choice": B through the scene's text until its choice box
//  shows, which the next step answers (B: an A as the text ends could land
//  on the choice). 01.02's Pancham scene takes ~60 s, hence 120 s.
//  "advance_with_a" is for a talk that B closes: Hotel Z's elevator before
//  "Which floor?" (02.05, console B 2026-11-10).
Outcome text_to_choice_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
    SceneMachineConfig config;
    config.stop_at_choice = true;
    config.text_button = Pokemon::story_arg<bool>(call.args, "advance_with_a") ? BUTTON_A : BUTTON_B;
    return run_scene_machine(backend, "text_to_choice", config, 120s);
}


//  A step's scene ("scene" "a"): A through its text until the party bar
//  holds a second, 120 s at most. Fails as "scene_never_started" when
//  neither text, a choice, a battle nor control shows within 45 s: the
//  player is not at the scene's trigger.
Outcome scene_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    SceneMachineConfig config;
    config.start_window = 45s;
    return run_scene_machine(backend, "scene", config, 120s);
}

//  "walk_to_prompt": straight on until an A prompt shows (walk_to_prompt()).
Outcome walk_to_prompt_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
    return walk_to_prompt(backend, Milliseconds(Pokemon::story_arg<int64_t>(call.args, "timeout_ms")));
}


//  "story_battle_start": A through the talk before a story battle (a
//  choice takes its top row) and, every 2 s where no text shows, through
//  the battle tutorial's cards and the VS splash, until the battle HUD
//  shows, 90 s at most.
Outcome battle_start_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    SceneMachineConfig config;
    config.stop_at_battle = true;
    config.blind_press_after = Milliseconds(0);
    return run_scene_machine(backend, "story_battle_start", config, 90s);
}

//  "story_battle": the battle on screen, by the backend's battle code.
Outcome battle_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    return backend.battle();
}


//  "talk_through_map_screen": talk_through_map_screen().
Outcome map_talk_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    return talk_through_map_screen(backend);
}

//  "climb_ladder": climb_ladder().
Outcome ladder_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    climb_ladder(backend);
    return Pokemon::outcome_ok();
}

//  "talk_to_pokedex_screen": the nurse's talk and the Pokedex app (03.04)
//  until the party bar holds a second, 90 s at most: B at its text and,
//  every 2 s while neither text nor control shows, at the Pokedex screens,
//  which only B closes (A plays the entry's motions).
Outcome pokedex_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    SceneMachineConfig config;
    config.text_button = BUTTON_B;
    config.blind_press_after = Milliseconds(0);
    config.blind_button = BUTTON_B;
    return run_scene_machine(backend, "talk_to_pokedex_screen", config, 90s);
}


//  After A at what the player faces, how long "talk" waits for its text
//  box, and how often it looks.
constexpr Milliseconds TALK_WAIT(5000);
constexpr Milliseconds TALK_POLL(250);

//  "talk": A at what the player faces, then a text box within TALK_WAIT;
//  fails as "talk_no_text" without one.
Outcome talk_step(Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    for (Milliseconds waited(0); ; waited += TALK_POLL){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(ScreenEvent::DIALOG_BOX)){
            return Pokemon::outcome_ok();
        }
        if (waited >= TALK_WAIT){
            break;
        }
        backend.wait(TALK_POLL);
    }
    return Pokemon::outcome_failed("talk_no_text", std::format("no text box within {} ms of A", TALK_WAIT.count()));
}


//  How long "choose_N" waits for the choice box.
constexpr Milliseconds CHOICE_WAIT(3000);
constexpr Milliseconds CHOICE_POLL(250);

//  The highlighted row of the choice box on `look`, and how many rows it
//  has; none without a choice box or rows that read.
std::optional<std::pair<int, int>> choice_rows(const ScreenObservation& look){
    const auto selected = look.readings.find(CHOICE_SELECTED_READING);
    const auto rows = look.readings.find(CHOICE_ROWS_READING);
    if (!look.events.has(ScreenEvent::CHOICE_BOX) || selected == look.readings.end() || rows == look.readings.end()){
        return std::nullopt;
    }
    return std::make_pair(selected->second, rows->second);
}

//  "choose_<index>": with a choice box on screen (within CHOICE_WAIT),
//  moves the highlight from where it reads to row `index` (0 the top) with
//  the D-pad, reads it there, then A. Fails as "no_choice" when no choice
//  box with rows that read shows, "no_such_choice" when it has no row
//  `index`, and "choice_not_reached" when the highlight is elsewhere after
//  the presses.
Outcome choose_step(Pokemon::StoryBackend<ZonePose>& backend, int index){
    std::optional<std::pair<int, int>> rows;
    for (Milliseconds waited(0); ; waited += CHOICE_POLL){
        backend.scope().throw_if_cancelled();
        rows = choice_rows(backend.observe());
        if (rows || waited >= CHOICE_WAIT){
            break;
        }
        backend.wait(CHOICE_POLL);
    }
    if (!rows){
        return Pokemon::outcome_failed("no_choice", std::format("no choice box with rows that read in {} ms", CHOICE_WAIT.count()));
    }
    const auto [selected, count] = *rows;
    if (index >= count){
        return Pokemon::outcome_failed("no_such_choice", std::format("row {} of a choice of {} rows", index, count));
    }
    backend.log(std::format("[PLZA] choose_{}: row {} of {} highlighted", index, selected, count));
    const int moves = index - selected;
    for (int moved = 0; moved < std::abs(moves); moved++){
        backend.press(moves > 0 ? BUTTON_DOWN : BUTTON_UP, /*hold=*/160ms, /*release=*/240ms);
    }
    if (moves != 0){
        const std::optional<std::pair<int, int>> now = choice_rows(backend.observe());
        if (!now || now->first != index){
            return Pokemon::outcome_failed("choice_not_reached", std::format(
                "the highlight is on row {}, not {}, after {} presses from row {}",
                now ? std::to_string(now->first) : "none", index, std::abs(moves), selected
            ));
        }
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/240ms);
    return Pokemon::outcome_ok();
}

Handler choose_handler(int index){
    return [index](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
        return choose_step(backend, index);
    };
}


//  A handler that needs nothing but its step.
using StepFunction = Outcome (*)(Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&);

//  The actions run by a StepFunction, by name.
const std::map<std::string_view, StepFunction>& step_functions(){
    static const std::map<std::string_view, StepFunction> functions{
        {"mash_to_move_hints", move_hints_step},
        {"text_to_choice", text_to_choice_step},
        {"talk", talk_step},
        {"a", scene_step},
        {"walk_to_prompt", walk_to_prompt_step},
        {"story_battle_start", battle_start_step},
        {"story_battle", battle_step},
        {"talk_through_map_screen", map_talk_step},
        {"climb_ladder", ladder_step},
        {"talk_to_pokedex_screen", pokedex_step},
        {"walk", walk_step},
    };
    return functions;
}

//  The handler of each of plza_story_specs(), by name.
Handler plza_handler(StoryEnv* env, const StoryActionSpec& spec){
    if (spec.name == "confirm_new_game"){
        return confirm_handler(env);
    }
    if (spec.name == "run_to_gate"){
        return gate_run_handler(env);
    }
    const auto console_step = console_steps().find(spec.name);
    if (console_step != console_steps().end()){
        return console_handler(env, spec.name, console_step->second);
    }
    if (spec.name == "wait_for_platform"){
        return platform_handler(PlatformWait::PLATFORM_ONLY);
    }
    if (spec.name == "opening_to_platform"){
        return platform_handler(PlatformWait::AFTER_AUTOSAVE);
    }
    if (spec.name == "choose_0"){
        return choose_handler(0);
    }
    if (spec.name == "choose_1"){
        return choose_handler(1);
    }
    return step_functions().at(spec.name);
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

namespace{

//  The game's own autosave at the end of the rows `rows` marks "autosave";
//  the others are not saved, an "unverified" one for that reason.
Pokemon::StoryGame<ZonePose> with_autosaves(Pokemon::StoryGame<ZonePose> game, const Pokemon::StoryRows& rows){
    game.save = [rows](Pokemon::StoryBackend<ZonePose>& backend, const Pokemon::StoryCheckpoint& checkpoint){
        if (rows.autosaves.contains(checkpoint.id)){
            backend.log(std::format("[Save] {}: the game autosaved at its end", checkpoint.id));
            return Pokemon::outcome_ok();
        }
        if (rows.unverified_autosaves.contains(checkpoint.id)){
            return Pokemon::outcome_not_saved("save unverified");
        }
        return Pokemon::outcome_not_saved("no autosave at its end");
    };
    return game;
}

//  Whether `row` starts on the station platform: its first step checks it
//  (01.01).
bool starts_on_platform(const Pokemon::StoryCheckpoint& row){
    return !row.steps.empty() && row.steps.front().action == "wait_for_platform";
}

//  Looks after a load, PLATFORM_POLL apart, to tell 01.01's start from a
//  later one: right after a load the platform reads on the first two.
constexpr size_t START_LOOKS = 4;

void add_segment(const Pokemon::SegmentReport& part, Pokemon::SegmentReport& total){
    total.checkpoints_run += part.checkpoints_run;
    total.reloads += part.reloads;
    total.runs.insert(total.runs.end(), part.runs.begin(), part.runs.end());
    total.failed_checkpoint = part.failed_checkpoint;
    total.failed_event = part.failed_event;
    total.failure = part.failure;
}

} // namespace

Pokemon::StoryGame<ZonePose> make_plza_story_game(StoryEnv& env, const Pokemon::StoryRows& rows){
    Pokemon::StoryGame<ZonePose> game = with_autosaves(plza_game_actions(&env), rows);
    game.reload = [&env](Pokemon::StoryBackend<ZonePose>&){
        return reload_latest_save(env);
    };
    return game;
}

Pokemon::StoryGame<ZonePose> make_plza_story_game_without_console(const Pokemon::StoryRows& rows){
    return with_autosaves(plza_game_actions(nullptr), rows);
}

Outcome load_checkpoint_start(const Pokemon::StoryGame<ZonePose>& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row){
    if (!game.reload){
        return Pokemon::outcome_failed("no_reload", "this game has no reload");
    }
    backend.log(std::format("[Save] {}: loading the latest save", row.id));
    const Outcome loaded = game.reload(backend);
    if (!loaded.succeeded()){
        return loaded;
    }
    if (starts_on_platform(row)){
        return Pokemon::outcome_ok();
    }
    size_t streak = 0;
    for (size_t looks = 0; looks < START_LOOKS; looks++){
        backend.scope().throw_if_cancelled();
        streak = backend.observe().readings.contains(PLATFORM_READING) ? streak + 1 : 0;
        if (streak >= PLATFORM_LOOKS){
            backend.log(std::format("[Checkpoint] {}: the save is on the station platform, not at its start", row.id));
            return Pokemon::outcome_failed("older_save", std::format(
                "the latest save is on the station platform (01.01's start), older than {}'s start", row.id
            ));
        }
        backend.wait(PLATFORM_POLL);
    }
    backend.log(std::format("[Checkpoint] {}: off the station platform, as at its start", row.id));
    return Pokemon::outcome_ok();
}

Pokemon::SegmentReport run_plza_segments(
    const Pokemon::StoryGame<ZonePose>& game, const Pokemon::StoryRows& story,
    std::span<const Pokemon::StoryCheckpoint> rows, size_t attempts_per_checkpoint,
    Pokemon::StoryEngine<ZonePose>& engine, PlzaBackend& backend, Pokemon::RunState& state
){
    Pokemon::SegmentReport total;
    const std::vector<std::vector<Pokemon::StoryCheckpoint>> segments = Pokemon::story_run_segments(rows, story);
    for (size_t index = 0; index < segments.size(); index++){
        const std::vector<Pokemon::StoryCheckpoint>& segment = segments[index];
        if (index > 0){
            const Pokemon::StoryCheckpoint& row = segment.front();
            backend.log(std::format("[Event] {}/0 reload_when_chained: start", row.id));
            const Outcome loaded = load_checkpoint_start(game, backend, row);
            if (!loaded.succeeded()){
                backend.log(std::format("[Event] {}/0 reload_when_chained: failed: {}", row.id, to_string(loaded)));
                total.failed_checkpoint = row.id;
                total.failed_event = "reload_when_chained";
                total.failure = loaded;
                return total;
            }
            backend.log(std::format("[Event] {}/0 reload_when_chained: done", row.id));
            state = Pokemon::RunState{};
        }
        const Pokemon::SegmentReport part = engine.run_segment("PLZA", segment, attempts_per_checkpoint, state);
        add_segment(part, total);
        if (!part.completed){
            return total;
        }
    }
    total.completed = true;
    return total;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

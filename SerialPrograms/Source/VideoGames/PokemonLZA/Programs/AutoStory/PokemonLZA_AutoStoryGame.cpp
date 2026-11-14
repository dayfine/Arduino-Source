/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActions.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryDialogs.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryScenes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryStepValidator.h"
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


const std::map<std::string, std::string, std::less<>>& story_actions_not_implemented(){
    static const std::map<std::string, std::string, std::less<>> todos{
    };
    return todos;
}


namespace{

using TimedLegs = std::vector<Pokemon::StoryTimedLeg>;

//  What a console action does with the run's console and the step's args.
using ConsoleBody = std::function<void(StoryEnv& env, const StoryActionArgs& args)>;

//  A GAME action that only presses through the game's own screens: one
//  call on the console, with no args.
using ConsoleStep = void (*)(StoryEnv& env);

//  The GAME actions that are one ConsoleStep each, by name: console_only.
const std::map<std::string, ConsoleStep, std::less<>>& console_game_actions(){
    static const std::map<std::string, ConsoleStep, std::less<>> actions{
        {"launch_game_from_home", [](StoryEnv& env){ launch_game_from_home(env.env.console, env.context); }},
        {"new_game_language", [](StoryEnv& env){ new_game_language(env.env.console, env.context, env.language); }},
        {"pick_look", [](StoryEnv& env){ pick_player_look(env.env.console, env.context, env.new_game.look); }},
        {"type_player_name", [](StoryEnv& env){
            type_player_name(env.env.console, env.context, env.new_game.name, env.new_game.name_first_letter_auto_caps);
        }},
        {"confirm_new_game", [](StoryEnv& env){ confirm_new_game_choices(env.env.console, env.context); }},
        {"choose_0", [](StoryEnv& env){ choose_dialog_option(env.env.console, env.context, 0); }},
        {"choose_1", [](StoryEnv& env){ choose_dialog_option(env.env.console, env.context, 1); }},
        {"story_battle_start", [](StoryEnv& env){
            mash_scene(env.env.console, env.context, SceneMashGoal::BATTLE, BUTTON_A, 90s);
        }},
        {"story_battle", [](StoryEnv& env){
            fight_battle_in_progress(env.env, env.env.console, env.context, env.battle_ai, 120s);
        }},
    };
    return actions;
}

//  The built-in action of `kind` named `name`: a replacement keeps its args.
StoryActionSpec builtin_spec(StoryStepKind kind, std::string name){
    const StoryActionSpec* spec = Pokemon::find_story_action(Pokemon::builtin_story_actions(), kind, name);
    if (spec != nullptr){
        return *spec;
    }
    return StoryActionSpec{.kind = kind, .name = std::move(name), .args = {}};
}

//  `spec`, run only on a console.
StoryActionSpec console_only(StoryActionSpec spec){
    spec.console_only = true;
    return spec;
}

StoryActionSpec game_spec(std::string name, std::vector<StoryArgSpec> args = {}){
    return StoryActionSpec{.kind = StoryStepKind::GAME, .name = std::move(name), .args = std::move(args)};
}

StoryArgSpec optional_ms(std::string key, int64_t fallback, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::INTEGER, .required = false, .fallback = fallback, .why = std::move(why)};
}

StoryArgSpec legs_arg(std::string key, bool required, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::TIMED_LEGS, .required = required, .fallback = std::nullopt, .why = std::move(why)};
}

//  "walk": its legs, the fade it ends on, the heading it faces first.
StoryActionSpec walk_spec(){
    return game_spec("walk", {
        legs_arg("legs", true, "the legs: [turn in degrees, clockwise (negative: left), walk ms] each"),
        StoryArgSpec{
            .key = "end", .type = StoryArgType::WORD, .required = false, .fallback = std::nullopt,
            .why = "\"fade_white\" or \"fade_black\": the last leg is held until it shows, its ms the bound",
        },
        StoryArgSpec{
            .key = "heading", .type = StoryArgType::INTEGER, .required = false, .fallback = std::nullopt,
            .why = "the compass heading to face first, clockwise from north",
        },
        StoryArgSpec{
            .key = "held", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false,
            .why = "hold the stick at each leg's angle (L only before a later turning leg), not tap-and-forward",
        },
        StoryArgSpec{
            .key = "wait_ms", .type = StoryArgType::INTEGER, .required = false, .fallback = std::nullopt,
            .why = "wait this long before looking for free control: NPCs moving off, a ride's fade starting",
        },
    });
}

//  "approach_starter": the walk to each starter, by the starter's key.
StoryActionSpec approach_starter_spec(){
    std::vector<StoryArgSpec> args;
    for (StarterChoice starter : STARTER_CHOICES){
        args.push_back(legs_arg(std::string(starter_key(starter)), false, "the walk to this starter, as a walk's legs"));
    }
    return game_spec("approach_starter", std::move(args));
}



//  Builds the handlers. The console actions hold the console; without one
//  (a test, a simulation) their specs are console_only and the engine
//  refuses them before their handler.
class PlzaHandlers{
public:
    PlzaHandlers(StoryEnv* env, StarterChoice starter)
        : m_env(env)
        , m_starter(starter)
    {
        if (m_env != nullptr){
            m_heading = heading_reader(*m_env);
        }
    }

    //  `body` on the console. A console step that fails becomes a failed
    //  Outcome, so the run can reload; an action that is not implemented
    //  ends the run.
    Handler console(std::string name, ConsoleBody body) const{
        if (m_env == nullptr){
            return [name = std::move(name)](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
                return Pokemon::outcome_failed(std::string(Pokemon::CONSOLE_ONLY), std::format("{} runs only on a console", name));
            };
        }
        StoryEnv& env = *m_env;
        return [&env, name = std::move(name), body = std::move(body)](
            Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&
        ){
            try{
                body(env, call.args);
            }catch (StoryActionNotImplemented&){
                throw;
            }catch (OperationFailedException& e){
                backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
                return Pokemon::outcome_failed("operation_failed", e.message());
            }
            return Pokemon::outcome_ok();
        };
    }

    //  "walk": its args' legs, walked once the screen shows free control,
    //  until its end event if it has one.
    Handler walk() const{
        return [heading = m_heading](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
            return walk_legs(backend, call, story_walk_move(call.args, "legs"), heading);
        };
    }

    //  "approach_starter": the walk to the run's starter, from the arg of
    //  its name. A starter with no walk is not implemented.
    Handler approach_starter() const{
        return [heading = m_heading, starter = m_starter](
            Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState& state
        ){
            const std::string_view key = starter_key(starter);
            if (!call.args.contains(key)){
                return not_implemented("approach_starter", std::format(
                    "the walk to {}: only Chikorita's was recorded (console B, 2026-11-08).", key
                ))(backend, call, state);
            }
            return walk_legs(backend, call, story_walk_move(call.args, key), heading);
        };
    }

    static Handler not_implemented(std::string name, std::string todo){
        return [name = std::move(name), todo = std::move(todo)](
            Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&
        ) -> Outcome{
            const std::string message = std::format("PLZA AutoStory: {} is not implemented yet. TODO: {}", name, todo);
            backend.log(message);
            throw StoryActionNotImplemented(message);
        };
    }

    //  "mash_to_move_hints": A through the scenes (a choice takes its top
    //  row, the advance chevron its text) until the player has control: the
    //  controls hint or the party bar held a second, or a main mission's
    //  banner on two looks (01.01's arrival ends on "MAIN MISSION 01"; the
    //  step returns once it has gone, 10 s at most). 01.01's arrival takes
    //  ~240 s on the console, hence 360 s. A text-free cutscene
    //  (01.01's city flyover, ~20 s) is not control: nothing busy for the
    //  quiet time fails the step. After 5 s with nothing busy and no sign
    //  of control, A every 2 s, for text the detectors miss (01.01's ad).
    //  A banner that stays past the 10 s gets A up to 3 times (01.03's
    //  "MAIN MISSION 02"), so the step ends with control. "press_a_first"
    //  presses A once at a prompt (02.03's Hotel Z door, "A Enter"), which
    //  would otherwise end the step at once as control. "quiet_is_control"
    //  ends it on the quiet time instead of failing, inside Hotel Z.
    static Handler move_hints(){
        return [](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
            if (Pokemon::story_arg<bool>(call.args, "press_a_first")){
                backend.press(BUTTON_A, 160ms, 400ms);
            }
            SceneMachineConfig config;
            config.quiet = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "quiet_ms"));
            config.min_length = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "min_length_ms"));
            config.quiet_is_control = Pokemon::story_arg<bool>(call.args, "quiet_is_control");
            config.blind_press_after = 5s;
            return run_scene_machine(backend, "mash_to_move_hints", config, 360s);
        };
    }

    //  B advances text by default. "advance_with_a" is for a talk that B
    //  cancels: Hotel Z's elevator closes on B before "Which floor?"
    //  (02.04, console B 2026-11-10).
    static Handler text_to_choice(){
        return [](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
            SceneMachineConfig config;
            config.goal = SceneMashGoal::CHOICE;
            config.text_button = Pokemon::story_arg<bool>(call.args, "advance_with_a") ? BUTTON_A : BUTTON_B;
            return run_scene_machine(backend, "text_to_choice", config, 60s);
        };
    }

private:
    //  `move`, once the screen shows free control; held until the fade of
    //  `call`'s "end" arg, if it names one. Each leg is a breakpoint.
    static Outcome walk_legs(
        Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, const StoryMove& move,
        const PlzaReader& heading
    ){
        const std::string_view name = call.step.name;
        const StoryActionArgs& args = call.args;
        if (move.wait_first > Milliseconds(0)){
            backend.wait(move.wait_first);
        }
        const Outcome free = check_can_walk(backend, name);
        if (!free.succeeded()){
            return free;
        }
        const auto end = args.find("end");
        std::optional<PokemonAutomation::ScreenEvent> fade;
        if (end != args.end()){
            fade = walk_end_event(std::get<std::string>(end->second));
        }
        return fade ? walk_until(backend, name, move, heading, *fade, &call.pause) : run_story_move(backend, move, heading, &call.pause);
    }

    StoryEnv* m_env;
    StarterChoice m_starter;
    //  The compass for the walks; none without a console, where a look
    //  carries its own heading.
    PlzaReader m_heading;
};

//  What a platform wait needs before the platform counts.
enum class PlatformWait{
    PLATFORM_ONLY,      //  "wait_for_platform": a start check
    AFTER_AUTOSAVE,     //  "opening_to_platform": the autosave badge first
};

//  Looks every PLATFORM_POLL: the autosave badge shows for 1-3 s (console B,
//  2026-11-10), so at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

//  One platform wait's progress over its looks.
struct PlatformProgress{
    bool autosave_seen = false;
    bool skip_prompt = false;
    size_t streak = 0;
    //  The text with the advance arrow was on the last look.
    bool text_waiting = false;
};

//  Takes `look` into `progress`; logs the skip prompt and the autosave
//  when they show. True once the platform has held PLATFORM_LOOKS looks.
bool platform_look(Pokemon::StoryBackend<ZonePose>& backend, const ScreenObservation& look, PlatformWait wait, PlatformProgress& progress){
    const bool skip_now = look.readings.contains(SKIP_PROMPT_READING);
    if (skip_now && !progress.skip_prompt){
        backend.log("[PLZA] platform wait: the skip prompt is on screen");
    }
    progress.skip_prompt = skip_now;
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
//  has arrived in Lumiose City!") waits for A (console B, 2026-11-13).
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(PokemonAutomation::ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

//  The station platform after the opening cutscene: PLATFORM_READING on
//  PLATFORM_LOOKS looks in a row; AFTER_AUTOSAVE counts them only once the
//  autosave badge has shown and gone. No input through the cutscene (A does
//  not advance it), but one A for each text box with the advance arrow, when
//  it shows (not again while the same one is up). Fails after `timeout`.
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
            backend.press(BUTTON_A, 160ms, 400ms);
        }
        progress.text_waiting = text_now;
        backend.wait(PLATFORM_POLL);
    }
    return Pokemon::outcome_failed("platform_not_seen", std::format(
        "No platform on {} looks in a row within {} ms (autosave {}).",
        PLATFORM_LOOKS, timeout.count(), progress.autosave_seen ? "seen" : "not seen"
    ));
}

//  One slice of walk_to_prompt()'s walk between looks.
constexpr Milliseconds PROMPT_WALK_SLICE(250);

//  "walk_to_prompt": straight on until an A prompt shows (INTERACT_PROMPT:
//  "A Leave" at the Battle Zone's gate), a slice at a time, nothing if it
//  shows already. Fails after `timeout` of walking without one.
Outcome walk_to_prompt(Pokemon::StoryBackend<ZonePose>& backend, Milliseconds timeout){
    Milliseconds walked(0);
    while (true){
        backend.scope().throw_if_cancelled();
        if (backend.observe().events.has(PokemonAutomation::ScreenEvent::INTERACT_PROMPT)){
            backend.log(std::format("[PLZA] walk_to_prompt: the A prompt shows after {} ms of walking", walked.count()));
            return Pokemon::outcome_ok();
        }
        if (walked >= timeout){
            return Pokemon::outcome_failed("no_prompt", std::format("walk_to_prompt: no A prompt in {} ms of walking", timeout.count()));
        }
        backend.hold(Pokemon::Direction::NORTH, PROMPT_WALK_SLICE, Pokemon::StoryPace::WALK);
        walked += PROMPT_WALK_SLICE;
    }
}

//  The handler of "wait_for_platform" or "opening_to_platform".
Handler platform_handler(PlatformWait wait){
    return [wait](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
        return wait_for_platform(backend, Milliseconds(Pokemon::story_arg<int64_t>(call.args, "timeout_ms")), wait);
    };
}

//  The GAME actions that are not ConsoleSteps, by name: each one's handler
//  from the run's handlers.
using HandlerOf = Handler (*)(const PlzaHandlers& handlers);
const std::map<std::string, HandlerOf, std::less<>>& story_game_actions(){
    static const std::map<std::string, HandlerOf, std::less<>> actions{
        {"walk", [](const PlzaHandlers& handlers){ return handlers.walk(); }},
        {"approach_starter", [](const PlzaHandlers& handlers){ return handlers.approach_starter(); }},
        {"mash_to_move_hints", [](const PlzaHandlers&){ return PlzaHandlers::move_hints(); }},
        {"text_to_choice", [](const PlzaHandlers&){ return PlzaHandlers::text_to_choice(); }},
        {"wait_for_platform", [](const PlzaHandlers&){ return platform_handler(PlatformWait::PLATFORM_ONLY); }},
        {"opening_to_platform", [](const PlzaHandlers&){ return platform_handler(PlatformWait::AFTER_AUTOSAVE); }},
        {"walk_to_prompt", [](const PlzaHandlers&){
            return Handler([](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
                return walk_to_prompt(backend, Milliseconds(Pokemon::story_arg<int64_t>(call.args, "timeout_ms")));
            });
        }},
    };
    return actions;
}

//  The handler of the GAME action `name`: a todo, a ConsoleStep or one of
//  story_game_actions().
Handler game_handler(const PlzaHandlers& handlers, const std::string& name){
    const auto todo = story_actions_not_implemented().find(name);
    if (todo != story_actions_not_implemented().end()){
        return PlzaHandlers::not_implemented(name, todo->second);
    }
    const auto step = console_game_actions().find(name);
    if (step != console_game_actions().end()){
        return handlers.console(name, [step = step->second](StoryEnv& env, const StoryActionArgs&){
            step(env);
        });
    }
    return story_game_actions().at(name)(handlers);
}

//  Where a fly to `args`' "place" leaves the player: in that place.
Pokemon::Pose flown_to(const StoryActionArgs& args){
    Pokemon::ZoneDeclared place;
    place.place = Pokemon::story_arg<std::string>(args, "place");
    return Pokemon::pose_of(Pokemon::DeclaredPose(std::move(place)));
}

//  The handler of each of plza_story_action_specs(), by kind and name.
Handler plza_handler(const PlzaHandlers& handlers, const StoryActionSpec& spec){
    const std::string& name = spec.name;
    switch (spec.kind){
    case StoryStepKind::ROUTE:{
        Handler fly = handlers.console(name, [](StoryEnv& env, const StoryActionArgs& args){
            const std::string& place = Pokemon::story_arg<std::string>(args, "place");
            fly_to(env.env.console, env.context, env.language, get_location_item_from_slug(place).location);
        });
        //  Flown: the player is at the target.
        return [fly = std::move(fly)](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState& state){
            Outcome outcome = fly(backend, call, state);
            if (outcome.succeeded()){
                outcome.pose = flown_to(call.args);
            }
            return outcome;
        };
    }
    case StoryStepKind::TALK:
        return handlers.console(name, [](StoryEnv& env, const StoryActionArgs&){
            talk_to_facing(env.env.console, env.context);
        });
    case StoryStepKind::ANSWER:
        return handlers.console(name, [](StoryEnv& env, const StoryActionArgs& args){
            choose_dialog_option(env.env.console, env.context, Pokemon::story_arg<bool>(args, "yes") ? 0 : 1);
        });
    case StoryStepKind::MASH:{
        const Button button = name == "a" ? BUTTON_A : BUTTON_B;
        return handlers.console(name, [button](StoryEnv& env, const StoryActionArgs&){
            mash_story_scene(env.env.console, env.context, button);
        });
    }
    default:
        return game_handler(handlers, name);
    }
}

const Pokemon::StoryActionRegistry<ZonePose>& spec_registry(){
    static const Pokemon::StoryGame<ZonePose> game = []{
        Pokemon::StoryGame<ZonePose> specs_only;
        specs_only.id = "plza";
        for (StoryActionSpec& spec : plza_story_action_specs()){
            specs_only.actions.push_back({.spec = std::move(spec), .handler = {}});
        }
        return specs_only;
    }();
    static const Pokemon::StoryActionRegistry<ZonePose> registry(game);
    return registry;
}

//  PLZA's actions over `env` (none: no console), with no save or reload.
Pokemon::StoryGame<ZonePose> plza_game_actions(StoryEnv* env, StarterChoice starter){
    const PlzaHandlers handlers(env, starter);
    Pokemon::StoryGame<ZonePose> game;
    game.id = "plza";
    for (StoryActionSpec& spec : plza_story_action_specs()){
        Handler handler = plza_handler(handlers, spec);
        game.actions.push_back({.spec = std::move(spec), .handler = std::move(handler)});
    }
    return game;
}

} // namespace


std::vector<StoryActionSpec> plza_story_action_specs(){
    std::vector<StoryActionSpec> specs{
        console_only(StoryActionSpec{.kind = StoryStepKind::ROUTE, .name = "fly", .args = {
            StoryArgSpec{
                .key = "place", .type = StoryArgType::WORD, .required = true,
                .fallback = std::nullopt, .why = "the fast travel place, by slug",
            },
        }}),
        console_only(builtin_spec(StoryStepKind::TALK, "talk")),
        console_only(builtin_spec(StoryStepKind::ANSWER, "answer")),
        console_only(builtin_spec(StoryStepKind::MASH, "mash")),
        console_only(StoryActionSpec{.kind = StoryStepKind::MASH, .name = "a", .args = {}}),
        game_spec("mash_to_move_hints", {
            optional_ms("min_length_ms", 0, "the scene's rough length: no quiet failure before it"),
            optional_ms("quiet_ms", 60000, "nothing busy and no control this long fails the step"),
            StoryArgSpec{
                .key = "press_a_first", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false,
                .why = "press A once first, at the prompt that starts the scene (a door's \"A Enter\")",
            },
            StoryArgSpec{
                .key = "quiet_is_control", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false,
                .why = "the quiet time ends the step with control, where no party bar or prompt shows (Hotel Z)",
            },
        }),
        game_spec("text_to_choice", {
            StoryArgSpec{
                .key = "advance_with_a", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false,
                .why = "advance the text with A, for a talk that B cancels",
            },
        }),
        game_spec("wait_for_platform", {
            optional_ms("timeout_ms", 180000, "no platform this long fails the step"),
        }),
        game_spec("walk_to_prompt", {
            optional_ms("timeout_ms", 10000, "walking this long with no A prompt fails the step"),
        }),
        game_spec("opening_to_platform", {
            optional_ms("timeout_ms", 240000, "no autosave and platform this long fails the step"),
        }),
    };
    for (const auto& [name, step] : console_game_actions()){
        specs.push_back(console_only(game_spec(name)));
    }
    specs.push_back(walk_spec());
    specs.push_back(approach_starter_spec());
    for (const auto& [name, todo] : story_actions_not_implemented()){
        specs.push_back(game_spec(name));
    }
    return specs;
}

const std::vector<StoryActionSpec>& plza_story_specs(){
    return spec_registry().specs();
}

std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint){
    std::vector<std::string> errors = spec_registry().lint(checkpoint);
    if (!errors.empty()){
        return errors;
    }
    for (const Pokemon::StoryStep& step : checkpoint.steps){
        for (std::string& error : plza_step_arg_errors(step)){
            errors.push_back(std::format("{}: {}", checkpoint.id, error));
        }
    }
    return errors;
}

StoryMove story_walk_move(const StoryActionArgs& args, std::string_view legs){
    StoryMove move;
    const auto heading = args.find("heading");
    if (heading != args.end()){
        move.heading = (double)std::get<int64_t>(heading->second);
    }
    const auto held = args.find("held");
    move.held = held != args.end() && std::get<bool>(held->second);
    const auto wait = args.find("wait_ms");
    if (wait != args.end()){
        move.wait_first = Milliseconds(std::get<int64_t>(wait->second));
    }
    for (const Pokemon::StoryTimedLeg& row : Pokemon::story_arg<TimedLegs>(args, legs)){
        move.legs.push_back({.turn = (double)row.turn, .walk = Milliseconds(row.ms)});
    }
    return move;
}

Pokemon::StoryGame<ZonePose> make_plza_story_game(
    StoryEnv& env, StarterChoice starter, std::set<std::string, std::less<>> autosave_rows
){
    Pokemon::StoryGame<ZonePose> game = plza_game_actions(&env, starter);
    //  Slow, and it throws without the minimap: never after a checkpoint.
    game.readers.push_back({
        .name = std::string(Pokemon::STORY_HEADING), .supplies = {std::string(Pokemon::STORY_HEADING)},
        .use = Pokemon::StoryReaderUse::ON_REQUEST, .console_only = true, .read = heading_reader(env),
    });
    game.save = [&env, autosave_rows = std::move(autosave_rows)](Pokemon::StoryBackend<ZonePose>& backend, const Pokemon::StoryCheckpoint& checkpoint){
        if (autosave_rows.contains(checkpoint.id)){
            backend.log(std::format("[Save] {}: the game autosaved at its end", checkpoint.id));
            return Pokemon::outcome_ok();
        }
        try{
            save_story_game(env.env.console, env.context);
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("save_failed", e.message());
        }
        return Pokemon::outcome_ok();
    };
    game.reload = [&env](Pokemon::StoryBackend<ZonePose>&){
        go_home(env.env.console, env.context);
        if (!reset_game_from_home(env.env, env.env.console, env.context)){
            return Pokemon::outcome_failed("reload_failed", "Failed to reset the game from HOME.");
        }
        return Pokemon::outcome_ok();
    };
    return game;
}

Pokemon::StoryGame<ZonePose> make_plza_story_game(StarterChoice starter){
    return plza_game_actions(nullptr, starter);
}


namespace{

//  Looks a reload at a boundary takes, PLATFORM_POLL apart, to tell 01.01's
//  start from a later one: right after a load the platform reads on the
//  first two (console B, 2026-11-10).
constexpr size_t BOUNDARY_LOOKS = 4;

//  The reload before `row`, which a run reached from the row before it: the
//  game's reload, then BOUNDARY_LOOKS looks, which fail if the station
//  platform holds PLATFORM_LOOKS of them in a row. A save on the platform is
//  01.01's start: the reload did not bring back the autosave `row` starts
//  from, which a reload_when_chained row comes after, but an older one.
Outcome reload_at_boundary(const Pokemon::StoryGame<ZonePose>& game, Pokemon::StoryBackend<ZonePose>& backend, std::string_view row){
    if (!game.reload){
        return Pokemon::outcome_failed("no_reload", "this game has no reload");
    }
    const Outcome reloaded = game.reload(backend);
    if (!reloaded.succeeded()){
        return reloaded;
    }
    size_t streak = 0;
    for (size_t looks = 0; looks < BOUNDARY_LOOKS; looks++){
        backend.scope().throw_if_cancelled();
        streak = backend.observe().readings.contains(PLATFORM_READING) ? streak + 1 : 0;
        if (streak >= PLATFORM_LOOKS){
            return Pokemon::outcome_failed("older_save", std::format(
                "the reload did not land at {}'s start: the station platform shows, so it loaded an older save", row
            ));
        }
        backend.wait(PLATFORM_POLL);
    }
    return Pokemon::outcome_ok();
}

//  Whether a run reloads the save before `row` when it reaches it from the
//  row before.
bool reloads_before(const StoryData& story, StoryRunPolicy::BoundaryReload boundary_reload, std::string_view row){
    return boundary_reload == StoryRunPolicy::BoundaryReload::AT_PLAN_BOUNDARIES && story.reload_when_chained.contains(row);
}

void add_segment(const Pokemon::SegmentReport& part, Pokemon::SegmentReport& total){
    total.checkpoints_run += part.checkpoints_run;
    total.reloads += part.reloads;
    total.runs.insert(total.runs.end(), part.runs.begin(), part.runs.end());
    total.failed_checkpoint = part.failed_checkpoint;
    total.failed_event = part.failed_event;
    total.failure = part.failure;
}

} // namespace

Pokemon::SegmentReport run_plza_segment(
    const Pokemon::StoryGame<ZonePose>& game, const StoryData& story,
    std::span<const Pokemon::StoryCheckpoint> rows, const StoryRunPolicy& policy,
    Pokemon::StoryEngine<ZonePose>& engine, Pokemon::StoryBackend<ZonePose>& backend, Pokemon::RunState& state
){
    Pokemon::SegmentReport total;
    size_t begin = 0;
    while (begin < rows.size()){
        size_t end = begin + 1;
        while (end < rows.size() && !reloads_before(story, policy.boundary_reload, rows[end].id)){
            end++;
        }
        if (begin > 0){
            const std::string& id = rows[begin].id;
            backend.log(std::format("[Event] {}/0 reload_at_boundary: start", id));
            const Outcome reloaded = reload_at_boundary(game, backend, id);
            if (!reloaded.succeeded()){
                backend.log(std::format("[Event] {}/0 reload_at_boundary: failed: {}", id, to_string(reloaded)));
                total.failed_checkpoint = id;
                total.failed_event = "reload_at_boundary";
                total.failure = reloaded;
                return total;
            }
            backend.log(std::format("[Event] {}/0 reload_at_boundary: done", id));
            state = Pokemon::RunState{};
        }
        const Pokemon::SegmentReport part = engine.run_segment("plza", rows.subspan(begin, end - begin), policy.attempts_per_checkpoint, state);
        add_segment(part, total);
        if (!part.completed){
            return total;
        }
        begin = end;
    }
    total.completed = true;
    return total;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

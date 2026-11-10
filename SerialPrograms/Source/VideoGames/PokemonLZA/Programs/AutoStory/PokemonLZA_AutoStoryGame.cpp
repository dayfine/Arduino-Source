/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryDialogs.h"
#include "PokemonLZA_AutoStoryGuardedWalks.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryScenes.h"
#include "PokemonLZA_AutoStoryTable.h"
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
        {"catch_tutorial", "a throw: ZL+ZR held 1500 ms, then release (500 ms only aims); retry while the catch HUD comes back (it broke out) until \"You caught\"; turn to find the target; balls as a budget."},
        {"battle_then_catch", "fight at once (the target attacks first) until the \"catch window\" line, A twice (a throw is ignored while a dialog shows), then the 1500 ms ZL+ZR throw."},
        {"register_five_species", "find a wild Pokemon of a new species, approach, ZL lock and throw, read caught / broke out, count from the quest panel to 5/5; then back to the lab gate (route not recorded)."},
        {"mash_to_instruction", "a mash that stops at an instruction line (\"Press X to open the main menu...\"): A once to close it, then X; X is ignored while the line shows."},
        {"claim_mable_research_reward", "the main menu: DOWN x3 to \"Mable's Research\", A, the Rewards page (TM004 Rock Smash)."},
    };
    return todos;
}


namespace{

//  The GAME actions that are not moves or todos and run on any backend,
//  with no args.
const std::vector<std::string>& backend_game_actions(){
    static const std::vector<std::string> actions{
        "approach_starter", "text_to_choice",
    };
    return actions;
}

//  The same that work through the game's own screens: console_only.
const std::vector<std::string>& console_game_actions(){
    static const std::vector<std::string> actions{
        "story_battle",
        "new_game_language", "pick_look", "type_player_name",
        "story_battle_start",
        "talk_through_map_screen", "talk_to_pokedex_screen", "climb_ladder",
        "choose_0", "choose_1",
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

//  What a console action does with the run's console and the step's args.
using ConsoleBody = std::function<void(StoryEnv& env, const StoryActionArgs& args)>;


//  Builds the handlers. The console actions hold the console; without one
//  (a test, a simulation) their specs are console_only and the engine
//  refuses them before their handler.
class PlzaActions{
public:
    PlzaActions(StoryEnv* env, StarterChoice starter)
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

    //  The move `name`, walked once the screen shows free control.
    Handler walk(std::string name, StoryMove move) const{
        return [name = std::move(name), move = std::move(move), heading = m_heading](
            Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&
        ){
            const Outcome free = check_can_walk(backend, name);
            if (!free.succeeded()){
                return free;
            }
            const auto end = story_move_ends().find(name);
            const std::optional<PokemonAutomation::ScreenEvent> fade =
                end == story_move_ends().end() ? std::nullopt : engine_fade_event(end->second);
            return fade ? walk_until(backend, name, move, heading, *fade) : run_story_move(backend, move, heading);
        };
    }

    Handler approach_starter() const{
        const auto move = starter_moves().find(m_starter);
        if (move == starter_moves().end()){
            return not_implemented("approach_starter", "the walk to this starter: only Chikorita's was recorded (console B, 2026-11-08).");
        }
        return walk("approach_starter", move->second);
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
    static Handler move_hints(){
        return [](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
            SceneMachineConfig config;
            config.quiet = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "quiet_ms"));
            config.min_length = Milliseconds(Pokemon::story_arg<int64_t>(call.args, "min_length_ms"));
            config.blind_press_after = 5s;
            return run_scene_machine(backend, "mash_to_move_hints", config, 360s);
        };
    }

    static Handler text_to_choice(){
        return [](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
            SceneMachineConfig config;
            config.goal = SceneMashGoal::CHOICE;
            config.text_button = BUTTON_B;
            return run_scene_machine(backend, "text_to_choice", config, 60s);
        };
    }

private:
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

//  The station platform after the opening cutscene, with no input (A does
//  not advance the cutscene): PLATFORM_READING on PLATFORM_LOOKS looks in a
//  row; AFTER_AUTOSAVE counts them only once the autosave badge has shown
//  and gone. Fails after `timeout`.
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

//  mash_scene() to `goal`, pressing `button` through the text.
ConsoleBody mash(SceneMashGoal goal, Button button, Milliseconds timeout){
    return [=](StoryEnv& env, const StoryActionArgs&){
        mash_scene(env.env.console, env.context, goal, button, timeout);
    };
}

//  The handler of the GAME action `name`.
Handler game_handler(const PlzaActions& make, const std::string& name){
    const auto todo = story_actions_not_implemented().find(name);
    if (todo != story_actions_not_implemented().end()){
        return PlzaActions::not_implemented(name, todo->second);
    }
    const auto move = story_moves().find(name);
    if (move != story_moves().end()){
        return make.walk(name, move->second);
    }
    if (name == "choose_0" || name == "choose_1"){
        const size_t row = name == "choose_0" ? 0 : 1;
        return make.console(name, [row](StoryEnv& env, const StoryActionArgs&){
            choose_dialog_option(env.env.console, env.context, row);
        });
    }
    if (name == "approach_starter"){
        return make.approach_starter();
    }
    if (name == "mash_to_move_hints"){
        return PlzaActions::move_hints();
    }
    if (name == "text_to_choice"){
        return PlzaActions::text_to_choice();
    }
    if (name == "story_battle_start"){
        return make.console(name, mash(SceneMashGoal::BATTLE, BUTTON_A, 90s));
    }
    if (name == "talk_through_map_screen" || name == "talk_to_pokedex_screen"){
        //  B through the talk closes the map (A would place a pin) and the
        //  Pokedex entry (A only plays its motions).
        return make.console(name, mash(SceneMashGoal::OVERWORLD, BUTTON_B, 90s));
    }
    if (name == "story_battle"){
        return make.console(name, [](StoryEnv& env, const StoryActionArgs&){
            fight_battle_in_progress(env.env, env.env.console, env.context, env.battle_ai, 120s);
        });
    }
    return make.console(name, [name](StoryEnv& env, const StoryActionArgs&){
        if (name == "new_game_language"){
            new_game_language(env.env.console, env.context);
        }else if (name == "pick_look"){
            pick_first_look(env.env.console, env.context);
        }else if (name == "type_player_name"){
            type_player_name(env.env.console, env.context, env.player_name);
        }else{
            climb_ladder(env.env.console, env.context);
        }
    });
}

//  The handler of each of plza_story_action_specs(), by kind and name.
Handler plza_handler(const PlzaActions& make, const StoryActionSpec& spec){
    const std::string& name = spec.name;
    switch (spec.kind){
    case StoryStepKind::ROUTE:{
        Handler fly = make.console(name, [](StoryEnv& env, const StoryActionArgs& args){
            const Pokemon::GridPose& target = Pokemon::story_arg<Pokemon::GridPose>(args, "target");
            fly_to(env.env.console, env.context, env.language, get_location_item_from_slug(target.map).location);
        });
        //  Flown: the player is at the target.
        return [fly = std::move(fly)](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState& state){
            Outcome outcome = fly(backend, call, state);
            if (outcome.succeeded()){
                outcome.pose = Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(call.args, "target"));
            }
            return outcome;
        };
    }
    case StoryStepKind::TALK:
        return make.console(name, [](StoryEnv& env, const StoryActionArgs&){
            talk_to_facing(env.env.console, env.context);
        });
    case StoryStepKind::ANSWER:
        return make.console(name, [](StoryEnv& env, const StoryActionArgs& args){
            choose_dialog_option(env.env.console, env.context, Pokemon::story_arg<bool>(args, "yes") ? 0 : 1);
        });
    case StoryStepKind::MASH:
        //  120 s: 02.01's last scene holds a ~25 s black screen.
        return make.console(name, mash(SceneMashGoal::OVERWORLD, name == "a" ? BUTTON_A : BUTTON_B, 120s));
    case StoryStepKind::HEAL:
        return make.console(name, [](StoryEnv& env, const StoryActionArgs&){
            heal_at_pokemon_center(env.env.console, env.context);
        });
    case StoryStepKind::SHOP:
        return make.console(name, [](StoryEnv& env, const StoryActionArgs& args){
            std::vector<std::string> items;
            for (const Pokemon::StoryItem& item : Pokemon::story_arg<std::vector<Pokemon::StoryItem>>(args, "items")){
                items.push_back(item.item);
            }
            buy_items(env.env.console, env.context, items);
        });
    default:
        if (name == "wait_for_platform" || name == "opening_to_platform"){
            return platform_handler(name == "wait_for_platform" ? PlatformWait::PLATFORM_ONLY : PlatformWait::AFTER_AUTOSAVE);
        }
        return game_handler(make, name);
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
    const PlzaActions make(env, starter);
    Pokemon::StoryGame<ZonePose> game;
    game.id = "plza";
    for (StoryActionSpec& spec : plza_story_action_specs()){
        Handler handler = plza_handler(make, spec);
        game.actions.push_back({.spec = std::move(spec), .handler = std::move(handler)});
    }
    return game;
}

} // namespace


std::vector<StoryActionSpec> plza_story_action_specs(){
    std::vector<StoryActionSpec> specs{
        console_only(StoryActionSpec{.kind = StoryStepKind::ROUTE, .name = "fly", .args = {
            StoryArgSpec{
                .key = "target", .type = StoryArgType::GRID_POSE, .required = true,
                .fallback = std::nullopt, .why = "the fast travel place, by slug, as the map",
            },
        }}),
        console_only(builtin_spec(StoryStepKind::TALK, "talk")),
        console_only(builtin_spec(StoryStepKind::ANSWER, "answer")),
        console_only(builtin_spec(StoryStepKind::MASH, "mash")),
        console_only(StoryActionSpec{.kind = StoryStepKind::MASH, .name = "a", .args = {}}),
        console_only(StoryActionSpec{.kind = StoryStepKind::HEAL, .name = "heal", .args = {}}),
        console_only(builtin_spec(StoryStepKind::SHOP, "shop")),
        game_spec("mash_to_move_hints", {
            optional_ms("min_length_ms", 0, "the scene's rough length: no quiet failure before it"),
            optional_ms("quiet_ms", 60000, "nothing busy and no control this long fails the step"),
        }),
        game_spec("wait_for_platform", {
            optional_ms("timeout_ms", 180000, "no platform this long fails the step"),
        }),
        game_spec("opening_to_platform", {
            optional_ms("timeout_ms", 240000, "no autosave and platform this long fails the step"),
        }),
    };
    for (const std::string& name : backend_game_actions()){
        specs.push_back(game_spec(name));
    }
    for (const std::string& name : console_game_actions()){
        specs.push_back(console_only(game_spec(name)));
    }
    for (const auto& [name, move] : story_moves()){
        specs.push_back(game_spec(name));
    }
    for (const auto& [name, todo] : story_actions_not_implemented()){
        specs.push_back(game_spec(name));
    }
    return specs;
}

const std::vector<StoryActionSpec>& plza_story_specs(){
    return spec_registry().specs();
}

std::vector<std::string> plza_story_lint(const Pokemon::StoryCheckpoint& checkpoint){
    return spec_registry().lint(checkpoint);
}

Pokemon::StoryGame<ZonePose> make_plza_story_game(StoryEnv& env, StarterChoice starter){
    Pokemon::StoryGame<ZonePose> game = plza_game_actions(&env, starter);
    //  Slow, and it throws without the minimap: never after a checkpoint.
    game.readers.push_back({
        .name = std::string(Pokemon::STORY_HEADING), .supplies = {std::string(Pokemon::STORY_HEADING)},
        .use = Pokemon::StoryReaderUse::ON_REQUEST, .console_only = true, .read = heading_reader(env),
    });
    game.save = [&env](Pokemon::StoryBackend<ZonePose>& backend, const Pokemon::StoryCheckpoint& checkpoint){
        if (story_autosave_rows().contains(checkpoint.id)){
            backend.log(std::format("[Save] {}: the game autosaved at its end; the menu cannot save yet", checkpoint.id));
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

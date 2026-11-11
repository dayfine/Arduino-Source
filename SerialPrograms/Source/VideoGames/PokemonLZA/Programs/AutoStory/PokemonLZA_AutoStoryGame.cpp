/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <set>
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

using IntegerRows = std::vector<std::vector<int64_t>>;

//  approach_starter's arg for each starter: the walk to it.
std::string_view starter_key(StarterChoice starter){
    switch (starter){
    case StarterChoice::CHIKORITA:
        return "chikorita";
    case StarterChoice::TEPIG:
        return "tepig";
    case StarterChoice::TOTODILE:
        return "totodile";
    }
    return "";
}

constexpr StarterChoice STARTERS[] = {StarterChoice::CHIKORITA, StarterChoice::TEPIG, StarterChoice::TOTODILE};

//  A walk's "end" arg as the event it ends on: a fade.
std::optional<Pokemon::ScreenEvent> walk_end_event(std::string_view end){
    if (end == "fade_white"){
        return Pokemon::ScreenEvent::FADE_WHITE;
    }
    if (end == "fade_black"){
        return Pokemon::ScreenEvent::FADE_BLACK;
    }
    return std::nullopt;
}

//  What is wrong with the legs `rows` of step `step`: each a [turn, ms]
//  pair turning at most half a turn either way and walking no negative
//  time, and at least one that walks.
void add_legs_errors(std::string_view step, std::string_view key, const IntegerRows& rows, std::vector<std::string>& errors){
    if (rows.empty()){
        errors.push_back(std::format("{}: {} has no legs", step, key));
        return;
    }
    bool goes = false;
    for (size_t i = 0; i < rows.size(); i++){
        const std::vector<int64_t>& row = rows[i];
        if (row.size() != 2){
            errors.push_back(std::format("{}: {} leg {} needs 2 numbers [turn, ms], has {}", step, key, i, row.size()));
            continue;
        }
        if (row[0] < -180 || row[0] > 180){
            errors.push_back(std::format("{}: {} leg {} turns {} deg, expected -180 to 180", step, key, i, row[0]));
        }
        if (row[1] < 0){
            errors.push_back(std::format("{}: {} leg {} walks {} ms, expected 0 or more", step, key, i, row[1]));
        }
        goes = goes || row[1] > 0;
    }
    if (!goes){
        errors.push_back(std::format("{}: {} walks nowhere", step, key));
    }
}

//  What is wrong with an "approach_starter" step's walks: it needs one.
std::vector<std::string> starter_walk_errors(const Pokemon::StoryStep& step){
    std::vector<std::string> errors;
    bool any = false;
    for (StarterChoice starter : STARTERS){
        const auto legs = step.args.find(starter_key(starter));
        if (legs != step.args.end()){
            any = true;
            add_legs_errors(step.name, legs->first, std::get<IntegerRows>(legs->second), errors);
        }
    }
    if (!any){
        errors.push_back(std::format("{}: no walk to any starter", step.name));
    }
    return errors;
}

//  What is wrong with the args of a "walk" or "approach_starter" step;
//  nothing for any other step.
std::vector<std::string> walk_errors(const Pokemon::StoryStep& step){
    std::vector<std::string> errors;
    if (step.kind != StoryStepKind::GAME){
        return errors;
    }
    if (step.action == "approach_starter"){
        return starter_walk_errors(step);
    }
    if (step.action != "walk"){
        return errors;
    }
    const auto legs = step.args.find("legs");
    if (legs == step.args.end()){
        return errors;
    }
    const IntegerRows& rows = std::get<IntegerRows>(legs->second);
    add_legs_errors(step.name, "legs", rows, errors);
    const auto end = step.args.find("end");
    if (end != step.args.end()){
        const std::string& name = std::get<std::string>(end->second);
        if (!walk_end_event(name)){
            errors.push_back(std::format("{}: end {} is not \"fade_white\" or \"fade_black\"", step.name, name));
        }
        if (rows.empty() || rows.back().size() != 2 || rows.back()[1] <= 0){
            errors.push_back(std::format("{}: the last leg has no walk to hold until the end", step.name));
        }
        const auto held = step.args.find("held");
        if (held != step.args.end() && std::get<bool>(held->second)){
            errors.push_back(std::format("{}: a held walk cannot end on a fade yet", step.name));
        }
    }
    const auto wait = step.args.find("wait_ms");
    if (wait != step.args.end() && std::get<int64_t>(wait->second) < 0){
        errors.push_back(std::format("{}: wait_ms {} is negative", step.name, std::get<int64_t>(wait->second)));
    }
    const auto heading = step.args.find("heading");
    if (heading != step.args.end()){
        const int64_t degrees = std::get<int64_t>(heading->second);
        if (degrees < 0 || degrees >= 360){
            errors.push_back(std::format("{}: heading {} deg, expected 0 to 359", step.name, degrees));
        }
    }
    return errors;
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

StoryArgSpec legs_arg(std::string key, bool required, std::string why){
    return StoryArgSpec{.key = std::move(key), .type = StoryArgType::INTEGER_ROWS, .required = required, .fallback = std::nullopt, .why = std::move(why)};
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
    for (StarterChoice starter : STARTERS){
        args.push_back(legs_arg(std::string(starter_key(starter)), false, "the walk to this starter, as a walk's legs"));
    }
    return game_spec("approach_starter", std::move(args));
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

    //  "walk": its args' legs, walked once the screen shows free control,
    //  until its end event if it has one.
    Handler walk() const{
        return [heading = m_heading](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
            return walk_legs(backend, call.step.name, story_walk_move(call.args, "legs"), call.args, heading);
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
            return walk_legs(backend, call.step.name, story_walk_move(call.args, key), call.args, heading);
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
    //  `args`' "end", if it names one.
    static Outcome walk_legs(
        Pokemon::StoryBackend<ZonePose>& backend, std::string_view name, const StoryMove& move,
        const StoryActionArgs& args, const PlzaReader& heading
    ){
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
            const std::optional<Pokemon::ScreenEvent> event = walk_end_event(std::get<std::string>(end->second));
            fade = event ? engine_fade_event(*event) : std::nullopt;
        }
        return fade ? walk_until(backend, name, move, heading, *fade) : run_story_move(backend, move, heading);
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
    if (name == "walk"){
        return make.walk();
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
        //  120 s: a scene can hold a ~25 s black screen (02.06's first evening).
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
        game_spec("opening_to_platform", {
            optional_ms("timeout_ms", 240000, "no autosave and platform this long fails the step"),
        }),
    };
    for (const std::string& name : console_game_actions()){
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
        for (std::string& error : walk_errors(step)){
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
    for (const std::vector<int64_t>& row : Pokemon::story_arg<IntegerRows>(args, legs)){
        if (row.size() != 2){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("A walk leg needs 2 numbers, has {}.", row.size()));
        }
        move.legs.push_back({.turn = (double)row[0], .walk = Milliseconds(row[1])});
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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

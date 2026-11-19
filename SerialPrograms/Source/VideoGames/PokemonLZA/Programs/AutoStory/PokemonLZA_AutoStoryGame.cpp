/*  PokemonLZA AutoStory - Story game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMenuSave.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryScenes.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryWalkScreenChecks.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStoryGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::RunState;
using Pokemon::StoryActions;
using Pokemon::StoryFailure;
using Pokemon::StoryStepRun;
using Pokemon::reject_unknown_keys;


namespace{

//  A console action run with no console: every attempt would fail the same way.
constexpr std::string_view CONSOLE_ONLY = "console_only";

//  A walk "end" that holds the last leg until a scene takes control.
constexpr std::string_view WALK_UNTIL_SCENE = "scene";


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


enum class PlatformWait{
    PLATFORM_ONLY,      //  "wait_for_platform": a start check
    AFTER_AUTOSAVE,     //  "opening_to_platform": the autosave badge first
};

//  The autosave badge shows for 1-3 s: at least 2 looks a second.
constexpr Milliseconds PLATFORM_POLL(300);
constexpr size_t PLATFORM_LOOKS = 2;

struct PlatformProgress{
    bool autosave_seen = false;
    size_t streak = 0;
    //  On the last look.
    bool text_waiting = false;
};

//  True once the platform has held PLATFORM_LOOKS looks.
bool platform_look(PlzaBackend& backend, const ScreenObservation& look, PlatformWait wait, PlatformProgress& progress){
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

//  The opening's last line ("Your train has arrived in Lumiose City!") waits for A.
bool text_waiting_for_a(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) && look.readings.contains(ADVANCE_ARROW_READING);
}

//  No input through the cutscene (A does not advance it), except one A per
//  text box with the advance arrow. Fails as "platform_not_seen".
std::optional<StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout, PlatformWait wait){
    //  The looks bound a script whose time stands still.
    const size_t max_looks = (size_t)(timeout / PLATFORM_POLL) + 1;
    std::optional<WallClock> start;
    PlatformProgress progress;
    for (size_t looks = 0; looks < max_looks; looks++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        start = start.value_or(look.when);
        if (platform_look(backend, look, wait, progress)){
            backend.log(std::format("[PLZA] platform wait: on the platform after {} looks", looks + 1));
            return std::nullopt;
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
    return StoryFailure{.code = "platform_not_seen", .detail = std::format(
        "No platform on {} looks in a row within {} ms (autosave {}).",
        PLATFORM_LOOKS, timeout.count(), progress.autosave_seen ? "seen" : "not seen"
    )};
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

//  Starts only with free control. "heading" needs the arrow, else CONSOLE_ONLY.
std::optional<StoryFailure> walk_step(const PlzaStoryTools& tools, const WalkArgs& walk, PlzaBackend& backend, std::string_view name){
    if (walk.move.wait_first > 0ms){
        backend.wait(walk.move.wait_first);
    }
    if (std::optional<StoryFailure> busy = check_can_walk(backend, name)){
        return busy;
    }
    if (walk.heading){
        if (!tools.arrow){
            return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = std::format("{}: its heading needs the minimap arrow", name), .repeats = true};
        }
        if (std::optional<StoryFailure> faced = face_heading(backend, *tools.arrow, *walk.heading)){
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
            "scene_start_ms", "gap_press_ms", "advance_with_a",
        });
        SceneMachineConfig config;
        config.quiet = ms_or(args, "quiet_ms", 60000ms);
        config.quiet_is_control = flag(args, "quiet_is_control");
        config.min_length = ms_or(args, "min_length_ms", 0ms);
        if (flag(args, "advance_with_a")){
            config.text_button = BUTTON_A;
        }
        if (const std::optional<Milliseconds> gap = optional_ms(args, "gap_press_ms")){
            config.gap_press = *gap;
        }
        if (const std::optional<Milliseconds> start = optional_ms(args, "scene_start_ms")){
            config.start_window = *start;
        }
        if (const JsonValue* fades = args.get_value("end_after_black_fades")){
            config.end_after_black_fades = (size_t)fades->to_integer_throw();
        }
        return [press_a_first = flag(args, "press_a_first"), config](PlzaBackend& backend, RunState&){
            if (press_a_first){
                press_through_prompt(backend);
            }
            return run_scene_machine(backend, "mash_to_move_hints", config, 360s);
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
        return [config](PlzaBackend& backend, RunState&){
            return run_scene_machine(backend, "text_to_choice", config, 120s);
        };
    };
}


//  B through the text (B cannot talk again once it ends). Fails as
//  "scene_never_started" when the HUD shows 45 s with nothing else: the
//  player is not at the scene's trigger.
std::optional<StoryFailure> scene_step(PlzaBackend& backend, RunState&){
    SceneMachineConfig config;
    config.start_window = 45s;
    return run_scene_machine(backend, "scene", config, 120s);
}

//  B on every other look closes the menus a talk opens (Mable's mission and
//  reward screens), which mash_to_move_hints sits on.
std::optional<StoryFailure> mash_scene_step(PlzaBackend& backend, RunState&){
    SceneMachineConfig config;
    config.blind_press_after = 0ms;
    config.blind_press_every = 0ms;
    config.blind_button = BUTTON_B;
    return run_scene_machine(backend, "scene", config, 120s);
}


//  As long as the start's lock-on holds ZL.
constexpr Milliseconds BATTLE_LOCK_ON_HOLD = 2000ms;

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
            backend.wait(250ms);
            waited += 250ms;
            continue;
        }
        if ((*lock_on)(BATTLE_LOCK_ON_HOLD)){
            backend.log("[PLZA] story_battle: no battle HUD; ZL held: the move symbols showed.");
            return true;
        }
        waited += BATTLE_LOCK_ON_HOLD;
    }
}

//  "battle_never_started" after 10 s, not 2 min of looks on the overworld.
std::optional<StoryFailure> battle_step(const PlzaStoryTools& tools, PlzaBackend& backend){
    if (!battle_on_within(backend, tools.lock_on, 10s)){
        return StoryFailure{
            .code = "battle_never_started",
            .detail = tools.lock_on ? "no battle HUD and no move symbols under ZL within 10 s" : "no battle HUD within 10 s",
        };
    }
    if (!tools.battle){
        return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = "no battle code off a console", .repeats = true};
    }
    return tools.battle(backend);
}


//  Blind B: no detector reads the Pokedex screens yet, and only B closes
//  them (A plays the entry's motions).
std::optional<StoryFailure> pokedex_step(PlzaBackend& backend, RunState&){
    SceneMachineConfig config;
    config.text_button = BUTTON_B;
    config.blind_press_after = 0ms;
    config.blind_button = BUTTON_B;
    return run_scene_machine(backend, "talk_to_pokedex_screen", config, 90s);
}


//  A console action: the console's code when `tools` has it, else
//  CONSOLE_ONLY. `timeout`: the step's "timeout_ms" fallback; none: no args.
Pokemon::StoryActionParser console_action(const PlzaStoryTools& tools, std::string name, std::optional<Milliseconds> timeout){
    return [&tools, name = std::move(name), timeout](const JsonObject& args) -> StoryStepRun{
        reject_unknown_keys(args, timeout ? std::initializer_list<std::string_view>{"timeout_ms"} : std::initializer_list<std::string_view>{});
        const Milliseconds limit = ms_or(args, "timeout_ms", timeout.value_or(0ms));
        return [&tools, name, limit](PlzaBackend& backend, RunState&) -> std::optional<StoryFailure>{
            const auto action = tools.console.find(name);
            if (action == tools.console.end()){
                return StoryFailure{.code = std::string(CONSOLE_ONLY), .detail = name + " runs only on a console", .repeats = true};
            }
            return action->second(backend, limit);
        };
    };
}

//  The actions that run only on a console, bound to `tools.console`.
void add_console_actions(StoryActions& actions, const PlzaStoryTools& tools){
    actions.emplace("run_to_gate", console_action(tools, "run_to_gate", 8000ms));
    actions.emplace("aimed_run_to_gate", console_action(tools, "aimed_run_to_gate", 4000ms));
    for (std::string_view name : {
        "new_game_intro", "confirm_new_game", "catch_tutorial", "battle_then_catch", "catch_in_zone",
        "walk_back_to_zone_road", "check_out_of_zone_1", "walk_to_unread_marker", "claim_mable_research_reward",
    }){
        actions.emplace(std::string(name), console_action(tools, std::string(name), std::nullopt));
    }
}

//  The actions that play a scene from a talk or prompt.
void add_scene_actions(StoryActions& actions, const PlzaStoryTools& tools){
    actions.emplace("mash", no_args(mash_scene_step));
    actions.emplace("a", no_args(scene_step));
    actions.emplace("story_battle_start", no_args([&tools](PlzaBackend& backend, RunState&){
        SceneMachineConfig config;
        config.stop_at_battle = true;
        config.lock_on = tools.lock_on;
        return run_scene_machine(backend, "story_battle_start", config, 90s);
    }));
    actions.emplace("story_battle", no_args([&tools](PlzaBackend& backend, RunState&){
        return battle_step(tools, backend);
    }));
}

} // namespace


StoryActions plza_story_actions(const PlzaStoryTools& tools){
    StoryActions actions{
        {"wait_for_platform", platform_action(PlatformWait::PLATFORM_ONLY, 180000ms)},
        {"opening_to_platform", platform_action(PlatformWait::AFTER_AUTOSAVE, 240000ms)},
        {"walk", [&tools](const JsonObject& args) -> StoryStepRun{
            return [&tools, walk = read_walk_args(args)](PlzaBackend& backend, RunState&){
                if (!walk.turn_to_starter){
                    return walk_step(tools, walk, backend, "walk");
                }
                WalkArgs aimed = walk;
                aimed.move.legs.front().turn = tools.starter_turn;
                return walk_step(tools, aimed, backend, "walk");
            };
        }},
        {"mash_to_move_hints", move_hints_action()},
        {"text_to_choice", text_to_choice_action()},
        {"choose_0", no_args([](PlzaBackend& backend, RunState&){
            return choose_row(backend, 0);
        })},
        {"choose_1", no_args([](PlzaBackend& backend, RunState&){
            return choose_row(backend, 1);
        })},
        {"talk", no_args([](PlzaBackend& backend, RunState&){
            return talk_at_prompt(backend);
        })},
        {"talk_through_map_screen", no_args([](PlzaBackend& backend, RunState&){
            return talk_through_map_screen(backend);
        })},
        {"climb_ladder", no_args([](PlzaBackend& backend, RunState&) -> std::optional<StoryFailure>{
            climb_ladder(backend);
            return std::nullopt;
        })},
        {"talk_to_pokedex_screen", no_args(pokedex_step)},
        {std::string(MENU_SAVE_ACTION), no_args([&tools](PlzaBackend& backend, RunState&){
            std::optional<StoryFailure> failure = save_from_menu(backend);
            if (!failure && tools.sightings != nullptr){
                tools.sightings->saved(backend);
            }
            return failure;
        })},
    };
    add_scene_actions(actions, tools);
    add_console_actions(actions, tools);
    return actions;
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
    if (end == "prompt"){
        return ScreenEvent::INTERACT_PROMPT;
    }
    return std::nullopt;
}

namespace{

//  01.01's first step waits for the platform; a new game has no save to check.
bool first_step_checks_start(const Pokemon::StoryCheckpoint& row){
    return story_row_first_action(row) == "wait_for_platform" || story_row_starts_without_save(row);
}

//  Right after a load the platform reads on the first two looks.
constexpr size_t START_LOOKS = 4;

enum class StartCheck{
    PASSED,     //  its first step checks it
    FAILED,     //  the station platform holds: 01.01's start, not this row's
    UNCHECKED,  //  off the platform; nothing checks the row's own start yet
};

//  Only the station platform is read so far.
StartCheck check_start(PlzaBackend& backend, const Pokemon::StoryCheckpoint& row){
    if (first_step_checks_start(row)){
        return StartCheck::PASSED;
    }
    size_t streak = 0;
    for (size_t looks = 0; looks < START_LOOKS; looks++){
        backend.scope().throw_if_cancelled();
        streak = backend.observe().readings.contains(PLATFORM_READING) ? streak + 1 : 0;
        if (streak >= PLATFORM_LOOKS){
            return StartCheck::FAILED;
        }
        backend.wait(PLATFORM_POLL);
    }
    return StartCheck::UNCHECKED;
}

} // namespace


Pokemon::StoryGame plza_story_game(const StoryRows& rows, AutosaveSightings& sightings){
    return Pokemon::StoryGame{.id = "plza", .save = autosave_policy(rows, sightings)};
}

std::optional<Pokemon::StoryFailure> load_checkpoint_start(Pokemon::StoryGame& game, PlzaBackend& backend, const Pokemon::StoryCheckpoint& row){
    if (!game.reload){
        return Pokemon::StoryFailure{.code = "no_reload", .detail = "this game has no reload"};
    }
    backend.log(std::format("[Save] {}: loading the latest save", row.id));
    if (!game.reload(backend, row)){
        return Pokemon::StoryFailure{.code = "reload_failed", .detail = "the reload is at no checkpoint's start"};
    }
    switch (check_start(backend, row)){
    case StartCheck::PASSED:
        return std::nullopt;
    case StartCheck::FAILED:
        backend.log(std::format("[Checkpoint] {}: the save is on the station platform, not at its start", row.id));
        return Pokemon::StoryFailure{.code = "older_save", .detail = std::format(
            "the latest save is on the station platform (01.01's start), older than {}'s start", row.id
        )};
    case StartCheck::UNCHECKED:
        break;
    }
    backend.log(std::format("[Checkpoint] {}: off the station platform; START UNCHECKED: nothing reads this row's own start", row.id));
    return std::nullopt;
}

namespace{

enum class Chain{
    ON,         //  from the live screen
    RELOAD,     //  from `row`'s start save
};

std::variant<Chain, Pokemon::StoryFailure> check_live_start(
    PlzaBackend& backend, const SegmentRunOptions& options, const Pokemon::StoryCheckpoint& row, bool seen
){
    const CheckpointStart* start = find_start(options.starts, row.id);
    if (start == nullptr || !options.read_start){
        backend.log(std::format(
            "[Checkpoint] {}: START UNCHECKED: {}; on from the live screen (autosave before it {})",
            row.id, start == nullptr ? "its plan names no start signature" : "nothing reads the screen", seen ? "seen" : "not seen"
        ));
        return Chain::ON;
    }
    const StartSignature read = options.read_start(start->signature);
    const std::string screen = std::format("the screen reads ({}), its start is ({})", to_string(read), to_string(start->signature));
    if (start_confirmed(start->signature, read)){
        backend.log(std::format("[Checkpoint] {}: START CHECKED: {}; on from the live screen", row.id, screen));
        return Chain::ON;
    }
    backend.log(std::format("[Checkpoint] {}: START CHECK FAILED: {}; nothing run from there", row.id, screen));
    return Pokemon::StoryFailure{.code = "chain_off_start", .detail = std::format("the run is not at {}'s start: {}", row.id, screen)};
}

//  `seen`: whether the row before's autosave showed. Fails when only a
//  reload would do and there is no save to load.
std::variant<Chain, Pokemon::StoryFailure> decide_chain(
    PlzaBackend& backend, const SegmentRunOptions& options, const Pokemon::StoryCheckpoint& row, bool seen
){
    const auto no_save = [&](std::string_view why){
        backend.log(std::format("[Checkpoint] {}: no save at its start: {}", row.id, why));
        return Pokemon::StoryFailure{.code = "no_start_save", .detail = std::format(
            "no save at {}'s start: {}; a reload would rewind to an older save", row.id, why
        )};
    };
    switch (check_start(backend, row)){
    case StartCheck::PASSED:
        backend.log(std::format("[Checkpoint] {}: start check passed; on from the live screen", row.id));
        return Chain::ON;
    case StartCheck::UNCHECKED:
        return check_live_start(backend, options, row, seen);
    case StartCheck::FAILED:
        break;
    }
    if (!seen){
        return no_save("the start check failed and the autosave before it was not seen");
    }
    backend.log(std::format("[Checkpoint] {}: start check failed; reloads its start save", row.id));
    return Chain::RELOAD;
}

std::optional<Pokemon::StoryFailure> reload_into(
    Pokemon::StoryGame& game, PlzaBackend& backend, const SegmentRunOptions& options,
    const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& row, const AutosaveSightings& sightings
){
    backend.log(std::format(
        "[Checkpoint] {}: reloads its start save when chained, whether or not {}'s autosave showed at its end ({})",
        row.id, previous.id, sightings.last_end_seen() ? "seen" : "not seen"
    ));
    const std::optional<Pokemon::StoryFailure> control = check_can_walk(backend, std::format("the reload into {}", row.id));
    if (control){
        backend.log(std::format("[Checkpoint] {}: control never came back after {}: no reload", row.id, previous.id));
        return control;
    }
    backend.log(std::format("[Checkpoint] {}: control is back after {}'s last scene; reloading the latest save", row.id, previous.id));
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(game, backend, row);
    if (loaded){
        return loaded;
    }
    const CheckpointStart* start = find_start(options.starts, row.id);
    if (start == nullptr || !options.read_start){
        const std::string why = start == nullptr ? "its plan names no start signature" : "nothing reads the screen";
        backend.log(std::format("[Checkpoint] {}: where the reload landed is not checked ({}); nothing pressed after the reload", row.id, why));
        return Pokemon::StoryFailure{.code = "reload_unchecked", .detail = std::format(
            "{}: where the reload landed is not checked: {}; nothing pressed after the reload", row.id, why
        )};
    }
    const StartSignature read = options.read_start(start->signature);
    if (!run_start(std::span<const Pokemon::StoryCheckpoint>(&row, 1), std::span<const CheckpointStart>(start, 1), /*asked=*/0, read)){
        const std::string where = std::format(
            "the screen reads ({}), the start is ({}); nothing pressed after the reload", to_string(read), to_string(start->signature)
        );
        backend.log(std::format("[Checkpoint] {}: the reload landed somewhere other than its start: {}", row.id, where));
        return Pokemon::StoryFailure{.code = "reload_off_start", .detail = std::format(
            "the reload into {} landed somewhere other than its start: {}", row.id, where
        )};
    }
    backend.log(std::format("[Checkpoint] {}: the reload landed at its start ({})", row.id, to_string(read)));
    return std::nullopt;
}

//  A reload starts a fresh `state`.
std::optional<Pokemon::StoryFailure> chain_into(
    Pokemon::StoryGame& game, PlzaBackend& backend, const SegmentRunOptions& options,
    const Pokemon::StoryCheckpoint& previous, const Pokemon::StoryCheckpoint& row,
    const AutosaveSightings& sightings, Pokemon::RunState& state
){
    const bool reload = options.reload_when_chained.contains(row.id);
    const std::variant<Chain, Pokemon::StoryFailure> chain = reload
        ? std::variant<Chain, Pokemon::StoryFailure>(Chain::RELOAD)
        : decide_chain(backend, options, row, sightings.last_end_seen());
    if (const Pokemon::StoryFailure* failure = std::get_if<Pokemon::StoryFailure>(&chain)){
        return *failure;
    }
    if (std::get<Chain>(chain) == Chain::ON){
        return std::nullopt;
    }
    const std::optional<Pokemon::StoryFailure> loaded = reload
        ? reload_into(game, backend, options, previous, row, sightings)
        : load_checkpoint_start(game, backend, row);
    if (!loaded){
        state = Pokemon::RunState{};
    }
    return loaded;
}

} // namespace

std::optional<Pokemon::CheckpointFailure> run_plza_segments(
    Pokemon::StoryGame& game, std::span<Pokemon::StoryCheckpoint> rows, const SegmentRunOptions& options,
    PlzaBackend& backend, Pokemon::RunState& state, AutosaveSightings& sightings
){
    for (size_t index = 0; index < rows.size(); index++){
        Pokemon::StoryCheckpoint& row = rows[index];
        const bool start_saved = index == 0 || options.reload_when_chained.contains(row.id) || sightings.last_end_seen();
        if (index > 0){
            if (std::optional<Pokemon::StoryFailure> chained = chain_into(game, backend, options, rows[index - 1], row, sightings, state)){
                backend.log(std::format("[Event] {}/0 chain: failed: {}", row.id, *chained));
                return Pokemon::CheckpointFailure{.checkpoint_id = row.id, .failure = std::move(*chained)};
            }
        }
        if (options.at_start){
            options.at_start(row);
        }
        if (!start_saved && options.attempts_per_checkpoint > 1){
            backend.log(std::format("[Checkpoint] {}: no autosave seen at its start, so no reload if it fails", row.id));
        }
        sightings.restart();
        Pokemon::StoryGame retry{
            .id = game.id,
            .save = [&game](PlzaBackend& on, const Pokemon::StoryCheckpoint& passed) -> std::optional<Pokemon::StoryFailure>{
                return game.save ? game.save(on, passed) : std::nullopt;
            },
            .attempts_per_checkpoint = start_saved ? options.attempts_per_checkpoint : 1,
        };
        if (game.reload){
            retry.reload = reloading_only_to_start(game, sightings);
        }
        if (std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, rows.subspan(index, 1), retry, state)){
            log_moved_save(backend, sightings, row.id);
            return failure;
        }
    }
    return std::nullopt;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

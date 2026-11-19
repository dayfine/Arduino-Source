/*  PokemonLZA AutoStory - Story game on the console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Settings.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_MenuNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryCatches.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLaunch.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLines.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMapLegs.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMapWalk.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMarkerWalk.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGame.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryConsoleGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Outcome;
using Pokemon::StoryActionCall;
using Pokemon::ZonePose;
using Handler = Pokemon::StoryActionHandler<ZonePose>;


namespace{

//  After a reload reaches the overworld, before the first look: the wait
//  reset_game_from_home() takes by default.
constexpr Milliseconds RELOAD_SETTLE = 1000ms;

//  The game onto its latest save: PLZA launched by launch_plza_from_home()
//  (its own tile, whichever game ran), then reset_game_from_home()'s title
//  and load. Fails as "launch_failed".
Outcome reload_latest_save(StoryEnv& env){
    ConsoleHandle& console = env.env.console;
    launch_plza_from_home(console, env.context);
    const bool loaded =
        openedgame_to_gamemenu(console, env.context, GameSettings::instance().START_GAME_WAIT) &&
        gamemenu_to_ingame(console, env.context, GameSettings::instance().ENTER_GAME_WAIT0);
    if (!loaded){
        return Pokemon::outcome_failed("launch_failed", "the game did not reach the overworld after the reload");
    }
    env.context.wait_for(RELOAD_SETTLE);
    return Pokemon::outcome_ok();
}


//  `message`, ending with `hint` (none: as it is).
std::string with_hint(std::string message, std::string_view hint){
    if (hint.empty() || message.ends_with(hint)){
        return message;
    }
    return std::format("{} {}", message, hint);
}

//  The new game step `name` on the console `env`, running `step`. An
//  OperationFailedException becomes a failed Outcome, so the run can report
//  it; its message and a UserSetupError's end with `hint`.
Handler new_game_handler(StoryEnv& env, std::string name, std::string_view hint, std::function<void(StoryEnv&)> step){
    return [&env, name, hint, step](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
        try{
            step(env);
        }catch (OperationFailedException& e){
            const std::string message = with_hint(e.message(), hint);
            backend.log(std::format("[PLZA] {} failed: {}", name, message));
            return Pokemon::outcome_failed(name + "_failed", message);
        }catch (UserSetupError& e){
            throw UserSetupError(env.env.console, with_hint(e.message(), hint));
        }
        return Pokemon::outcome_ok();
    };
}


//  "run_to_gate"'s run: straight on with upstream's
//  run_towards_gate_with_A_button() until the gate's A prompt; once more
//  after a day/night change, which stops the run. Fails as
//  "gate_not_reached" when the run ends with no prompt.
Outcome run_to_gate(StoryEnv& env, Milliseconds timeout){
    //  0: the A prompt, 1: a day/night change, else no prompt.
    int result = 1;
    for (size_t runs = 0; runs < 2 && result == 1; runs++){
        result = run_towards_gate_with_A_button(
            env.env.console, env.context, /*run_direction_x=*/0, /*run_direction_y=*/1, timeout
        );
    }
    if (result != 0){
        return Pokemon::outcome_failed("gate_not_reached", std::format("no gate A prompt in {} ms of running", timeout.count()));
    }
    return Pokemon::outcome_ok();
}

//  A run to a wild zone's gate on the console: no A prompt within
//  `timeout` fails it.
using GateStep = Outcome (*)(StoryEnv& env, PlzaBackend& backend, Milliseconds timeout);

//  "aimed_run_to_gate": up to 3 runs (run_to_gate()), each after map legs
//  along the far gate's line (ZONE1_GATE_LINE, ZONE1_GATE_APPROACH) and
//  facing the bearing from the pin there to the gate. A run with no A
//  prompt (a pillar) walks the legs again: a bearing from next to the gate
//  is noise (47 degrees from (359, 255), out of the zone).
Outcome aimed_gate_run(StoryEnv& env, PlzaBackend& backend, Milliseconds timeout){
    const MapWaypoint approach[] = {ZONE1_GATE_LINE, ZONE1_GATE_APPROACH};
    Outcome ran = Pokemon::outcome_ok();
    for (int runs = 0; runs < 3; runs++){
        if (runs > 0){
            backend.log(std::format("[PLZA] aimed_run_to_gate: no prompt on run {}, back along the gate's line", runs));
        }
        const Outcome walked = walk_map_legs(env, backend, approach);
        if (!walked.succeeded()){
            return walked;
        }
        const std::optional<MapPoint> pin = read_pin_on_map(env, backend);
        if (!pin){
            return Pokemon::outcome_failed("map_pin_not_read", "no map pin before the run to the far gate");
        }
        const Outcome faced = face_heading(backend, minimap_arrow(env), bearing(*pin, ZONE1_FAR_GATE.pin));
        if (!faced.succeeded()){
            return faced;
        }
        ran = run_to_gate(env, timeout);
        if (ran.succeeded()){
            return ran;
        }
    }
    return ran;
}

//  The handler of the gate run `name` on the console `env`, its arg
//  "timeout_ms" the run's timeout. An OperationFailedException fails as
//  "gate_not_reached".
Handler gate_handler(StoryEnv& env, std::string name, GateStep step){
    return [&env, name, step](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState&){
        const Milliseconds timeout(Pokemon::story_arg<int64_t>(call.args, "timeout_ms"));
        try{
            return step(env, backend, timeout);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
            return Pokemon::outcome_failed("gate_not_reached", e.message());
        }
    };
}


//  03.11's reward: the main menu (overworld_to_main_menu()), Mable's
//  Research three rows down, A. The page opens on Rewards under her lines on
//  the level 2 reward, TM004 Rock Smash (console B, 2026-11-10): A through
//  them, then B out of the menu to her talk. No line at all: not in her
//  office. Fails as "no_reward_lines", "reward_lines_endless" or
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
        {"catch_in_zone", [](StoryEnv& env, PlzaBackend& backend){
            //  5 registered species, 3 after the tutorial: 2 new; 12 throws.
            return catch_in_zone(env, backend, ZoneCatchGoal{.new_species = 2, .party_species = 5, .max_throws = 12});
        }},
        {"walk_back_to_zone_road", walk_back_to_zone_road},
        {"check_out_of_zone_1", [](StoryEnv& env, PlzaBackend& backend){
            return check_out_of_zone_1(backend, [&env, &backend]{
                return read_pin_on_map(env, backend);
            });
        }},
        {"walk_to_unread_marker", walk_to_unread_marker},
        {"claim_mable_research_reward", claim_research_reward},
    };
    return steps;
}

//  `step` on the console `env`. An OperationFailedException becomes a
//  failed Outcome, so the run can report it.
Handler console_handler(StoryEnv& env, std::string_view name, ConsoleStep step){
    return [&env, name = std::string(name), step](Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall&, Pokemon::RunState&){
        try{
            return step(env, backend);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
            return Pokemon::outcome_failed("operation_failed", e.message());
        }
    };
}


//  The console_only actions of plza_story_specs() on the console `env`.
PlzaConsoleActions console_actions(StoryEnv& env){
    PlzaConsoleActions actions;
    actions.emplace("new_game_intro", new_game_handler(env, "new_game_intro", NEW_GAME_BY_HAND_HINT, [](StoryEnv& story){
        play_new_game_intro(story.env.console, story.context, story.new_game);
    }));
    actions.emplace("confirm_new_game", new_game_handler(env, "confirm_new_game", /*hint=*/"", [](StoryEnv& story){
        confirm_new_game_choices(story.env.console, story.context, story.new_game);
    }));
    actions.emplace("run_to_gate", gate_handler(env, "run_to_gate", [](StoryEnv& story, PlzaBackend&, Milliseconds timeout){
        return run_to_gate(story, timeout);
    }));
    actions.emplace("aimed_run_to_gate", gate_handler(env, "aimed_run_to_gate", aimed_gate_run));
    for (const auto& [name, step] : console_steps()){
        actions.emplace(std::string(name), console_handler(env, name, step));
    }
    return actions;
}

} // namespace


Pokemon::StoryGame<ZonePose> make_plza_story_game(
    StoryEnv& env, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
){
    Pokemon::StoryGame<ZonePose> game = plza_story_game(console_actions(env), rows, minimap_arrow(env), move_symbols_lock_on(env), sightings);
    game.reload = [&env, &sightings](Pokemon::StoryBackend<ZonePose>&){
        sightings.restart();
        return reload_latest_save(env);
    };
    return game;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

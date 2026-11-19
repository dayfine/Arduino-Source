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
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryBattles.h"
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


namespace{

//  The wait reset_game_from_home() takes by default.
constexpr Milliseconds RELOAD_SETTLE = 1000ms;

//  Launches PLZA's own tile, whichever game ran. Fails as "launch_failed".
std::optional<Pokemon::StoryFailure> reload_latest_save(StoryEnv& env){
    ConsoleHandle& console = env.env.console;
    launch_plza_from_home(console, env.context);
    const bool loaded =
        openedgame_to_gamemenu(console, env.context, GameSettings::instance().START_GAME_WAIT) &&
        gamemenu_to_ingame(console, env.context, GameSettings::instance().ENTER_GAME_WAIT0);
    if (!loaded){
        return Pokemon::StoryFailure{.code = "launch_failed", .detail = "the game did not reach the overworld after the reload"};
    }
    env.context.wait_for(RELOAD_SETTLE);
    return std::nullopt;
}


std::string with_hint(std::string message, std::string_view hint){
    if (hint.empty() || message.ends_with(hint)){
        return message;
    }
    return std::format("{} {}", message, hint);
}

//  An OperationFailedException becomes a failed std::optional<Pokemon::StoryFailure>; its message and a
//  UserSetupError's end with `hint`.
PlzaConsoleAction new_game_handler(StoryEnv& env, std::string name, std::string_view hint, std::function<void(StoryEnv&)> step){
    return [&env, name, hint, step](PlzaBackend& backend, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
        try{
            step(env);
        }catch (OperationFailedException& e){
            const std::string message = with_hint(e.message(), hint);
            backend.log(std::format("[PLZA] {} failed: {}", name, message));
            return Pokemon::StoryFailure{.code = name + "_failed", .detail = message};
        }catch (UserSetupError& e){
            throw UserSetupError(env.env.console, with_hint(e.message(), hint));
        }
        return std::nullopt;
    };
}


//  Runs once more after a day/night change, which stops the run.
std::optional<Pokemon::StoryFailure> run_to_gate(StoryEnv& env, Milliseconds timeout){
    //  0: the A prompt, 1: a day/night change, else no prompt.
    int result = 1;
    for (size_t runs = 0; runs < 2 && result == 1; runs++){
        result = run_towards_gate_with_A_button(
            env.env.console, env.context, /*run_direction_x=*/0, /*run_direction_y=*/1, timeout
        );
    }
    if (result != 0){
        return Pokemon::StoryFailure{.code = "gate_not_reached", .detail = std::format("no gate A prompt in {} ms of running", timeout.count())};
    }
    return std::nullopt;
}

using GateStep = std::optional<Pokemon::StoryFailure> (*)(StoryEnv& env, PlzaBackend& backend, Milliseconds timeout);

//  A run with no A prompt (a pillar) walks the legs again: a bearing from
//  next to the gate is noise.
std::optional<Pokemon::StoryFailure> aimed_gate_run(StoryEnv& env, PlzaBackend& backend, Milliseconds timeout){
    const MapWaypoint approach[] = {ZONE1_GATE_LINE, ZONE1_GATE_APPROACH};
    std::optional<Pokemon::StoryFailure> ran = std::nullopt;
    for (int runs = 0; runs < 3; runs++){
        if (runs > 0){
            backend.log(std::format("[PLZA] aimed_run_to_gate: no prompt on run {}, back along the gate's line", runs));
        }
        const std::optional<Pokemon::StoryFailure> walked = walk_map_legs(env, backend, approach);
        if (walked){
            return walked;
        }
        const std::optional<MapPoint> pin = read_pin_on_map(env, backend);
        if (!pin){
            return Pokemon::StoryFailure{.code = "map_pin_not_read", .detail = "no map pin before the run to the far gate"};
        }
        const std::optional<Pokemon::StoryFailure> faced = face_heading(backend, minimap_arrow(env), bearing(*pin, ZONE1_FAR_GATE.pin));
        if (faced){
            return faced;
        }
        ran = run_to_gate(env, timeout);
        if (!ran){
            return ran;
        }
    }
    return ran;
}

PlzaConsoleAction gate_handler(StoryEnv& env, std::string name, GateStep step){
    return [&env, name, step](PlzaBackend& backend, Milliseconds timeout) -> std::optional<Pokemon::StoryFailure>{
        try{
            return step(env, backend, timeout);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
            return Pokemon::StoryFailure{.code = "gate_not_reached", .detail = e.message()};
        }
    };
}


//  Mable's Research is three rows down the main menu; it opens on Rewards
//  under her lines. No line at all: not in her office.
std::optional<Pokemon::StoryFailure> claim_research_reward(StoryEnv& env, PlzaBackend& backend){
    overworld_to_main_menu(env.env.console, env.context);
    for (int i = 0; i < 3; i++){
        backend.press(BUTTON_DOWN, /*hold=*/160ms, /*release=*/400ms);
    }
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/2840ms);
    const LinesPassed passed = press_a_through_lines(backend, /*quiet=*/3s, /*limit=*/60s);
    if (passed.timed_out){
        return Pokemon::StoryFailure{.code = "reward_lines_endless", .detail = "Mable's lines did not end within 60 s"};
    }
    if (!passed.text){
        return Pokemon::StoryFailure{.code = "no_reward_lines", .detail = "no dialog after opening Mable's Research: no reward was claimed"};
    }
    if (!press_b_out_of_menu(backend, /*presses=*/60)){
        return Pokemon::StoryFailure{.code = "menu_not_closed", .detail = "no party bar or talk after 60 B presses out of the menu"};
    }
    return std::nullopt;
}

using ConsoleStep = std::optional<Pokemon::StoryFailure> (*)(StoryEnv& env, PlzaBackend& backend);

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

PlzaConsoleAction console_handler(StoryEnv& env, std::string_view name, ConsoleStep step){
    return [&env, name = std::string(name), step](PlzaBackend& backend, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
        try{
            return step(env, backend);
        }catch (OperationFailedException& e){
            backend.log(std::format("[PLZA] {} failed: {}", name, e.message()));
            return Pokemon::StoryFailure{.code = "operation_failed", .detail = e.message()};
        }
    };
}


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


PlzaStoryTools plza_console_tools(StoryEnv& env, AutosaveSightings& sightings){
    return PlzaStoryTools{
        .console = console_actions(env),
        .arrow = minimap_arrow(env),
        .lock_on = move_symbols_lock_on(env),
        .battle = [&env](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{
            try{
                fight_story_battle(env.env, env.env.console, env.context, env.battle_ai, /*timeout=*/120s);
            }catch (OperationFailedException& e){
                return Pokemon::StoryFailure{.code = "battle_failed", .detail = e.message()};
            }
            return std::nullopt;
        },
        .sightings = &sightings,
    };
}

Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const StoryRows& rows, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(rows, sightings);
    game.reload = [&env, &sightings](PlzaBackend& backend, const Pokemon::StoryCheckpoint& failed) -> std::optional<std::string>{
        sightings.restart();
        if (const std::optional<Pokemon::StoryFailure> failure = reload_latest_save(env)){
            backend.log(std::format("[Save] {}: the reload failed: {}", failed.id, *failure));
            return std::nullopt;
        }
        return failed.id;
    };
    return game;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

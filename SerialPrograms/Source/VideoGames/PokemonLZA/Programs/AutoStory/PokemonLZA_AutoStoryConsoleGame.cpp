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
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLaunch.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryNewGame.h"
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


//  The console_only actions of plza_story_specs() on the console `env`.
PlzaConsoleActions console_actions(StoryEnv& env){
    PlzaConsoleActions actions;
    actions.emplace("new_game_intro", new_game_handler(env, "new_game_intro", NEW_GAME_BY_HAND_HINT, [](StoryEnv& story){
        play_new_game_intro(story.env.console, story.context, story.new_game);
    }));
    actions.emplace("confirm_new_game", new_game_handler(env, "confirm_new_game", /*hint=*/"", [](StoryEnv& story){
        confirm_new_game_choices(story.env.console, story.context, story.new_game);
    }));
    return actions;
}

} // namespace


Pokemon::StoryGame<ZonePose> make_plza_story_game(
    StoryEnv& env, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
){
    Pokemon::StoryGame<ZonePose> game = plza_story_game(console_actions(env), rows, minimap_arrow(env), sightings);
    game.reload = [&env, &sightings](Pokemon::StoryBackend<ZonePose>&){
        sightings.restart();
        return reload_latest_save(env);
    };
    return game;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

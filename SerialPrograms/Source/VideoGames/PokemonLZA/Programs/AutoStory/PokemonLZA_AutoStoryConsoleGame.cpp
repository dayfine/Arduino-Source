/*  PokemonLZA AutoStory - Story game on the console
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Settings.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryBattles.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryConsoleActions.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryConsoleGame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The wait reset_game_from_home() takes by default.
constexpr Milliseconds RELOAD_SETTLE = 1s;

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

} // namespace


PlzaConsoleHooks plza_console_hooks(StoryEnv& env, AutosaveSightings& sightings, std::optional<StoryMove> starter_move){
    return PlzaConsoleHooks{
        .console = plza_console_actions(env),
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
        .starter_move = std::move(starter_move),
    };
}

Pokemon::StoryGame make_plza_story_game(StoryEnv& env, const MarkedCheckpoints& marked, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(marked, sightings);
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
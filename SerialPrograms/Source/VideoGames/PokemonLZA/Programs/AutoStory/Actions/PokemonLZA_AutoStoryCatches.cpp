/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_AlertEyeDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BallThrow.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryCatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The throw of plza/autostory's catches, which passed 03.07 on console B
//  (2026-11-13): the banner showed 6.9 s after the throw (03.08).
const BallThrowOptions STORY_THROW{
    .lock_on = Milliseconds(500),
    .aim = Milliseconds(1500),
    .after_throw = Milliseconds(8000),
};

//  The lines after a throw end once none has shown for LINES_QUIET.
constexpr Milliseconds LINES_QUIET(3000);
constexpr Milliseconds LINES_LIMIT(30000);

//  Whether a wild battle is on: its Run prompt, or the red alert bar with
//  the eye at the top while a wild Pokemon attacks the player before the
//  partner is out (03.08, console B 2026-11-14).
bool wild_battle_on(StoryEnv& env){
    env.context.wait_for_all_requests();
    const VideoSnapshot screen = env.env.console.video().snapshot();
    if (!screen){
        return false;
    }
    RunFromBattleDetector run_prompt(COLOR_RED, nullptr);
    AlertEyeDetector alert(COLOR_RED, nullptr);
    return run_prompt.detect(screen) || alert.detect(screen);
}

//  Attacks until 3 land (12 tries at most). Two lock-ons in a row with no
//  move symbols press D-pad up (twice at most): the partner may not be
//  out. Returns the attacks that landed.
int weaken_target(StoryEnv& env, PlzaBackend& backend, TrainerBattleState& battle_state){
    int landed = 0;
    int misses = 0;
    int sends = 0;
    for (int tries = 0; tries < 12 && landed < 3; tries++){
        if (battle_state.attempt_one_attack(env.env, env.env.console, env.context)){
            landed++;
            misses = 0;
        }else if (++misses == 2 && sends < 2){
            backend.log("[PLZA] weaken_target: no lock-on twice: D-pad up to send the partner out");
            backend.press(BUTTON_UP, 100ms, 2000ms);
            misses = 0;
            sends++;
        }
    }
    return landed;
}

} // namespace


Pokemon::Outcome catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws){
    TrainerBattleState battle_state(env.battle_ai);
    for (int throws = 1; throws <= max_throws; throws++){
        throw_ball(env.context, STORY_THROW);
        if (press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT).card){
            backend.log(std::format("[PLZA] catch_tutorial: caught on throw {}", throws));
            return Pokemon::outcome_ok();
        }
        if (throws < max_throws && wild_battle_on(env)){
            const int landed = weaken_target(env, backend, battle_state);
            backend.log(std::format("[PLZA] catch_tutorial: throw {} broke out into a battle: {} attacks landed", throws, landed));
            press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        }
    }
    return Pokemon::outcome_failed("no_catch", std::format("no tutorial card after {} throws", max_throws));
}

Pokemon::Outcome battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds){
    backend.press(BUTTON_A, 160ms, 840ms);
    press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    backend.press(BUTTON_UP, 100ms, 2000ms);
    TrainerBattleState battle_state(env.battle_ai);
    for (int round = 0; round < max_rounds; round++){
        const int landed = weaken_target(env, backend, battle_state);
        if (landed == 0 && round > 0){
            backend.log(std::format("[PLZA] battle_then_catch: nothing to lock on in round {}: caught", round + 1));
            return Pokemon::outcome_ok();
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        throw_ball(env.context, STORY_THROW);
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        backend.log(std::format("[PLZA] battle_then_catch: round {}: {} attacks landed, then a throw", round + 1, landed));
    }
    return Pokemon::outcome_failed("still_a_target", std::format("a target still locks on after {} rounds", max_rounds));
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

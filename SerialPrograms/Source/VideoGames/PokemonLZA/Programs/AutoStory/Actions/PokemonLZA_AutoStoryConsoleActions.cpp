/*  PokemonLZA AutoStory - Console actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_MenuNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryActionNames.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryConfirmChoices.h"
#include "PokemonLZA_AutoStoryHeading.h"
#include "PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryMapWalk.h"
#include "PokemonLZA_AutoStoryMarkerWalk.h"
#include "PokemonLZA_AutoStoryNewGame.h"
#include "PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryConsoleActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The pin between the gate's pillars, where "A Leave" shows; 1 north of
//  the (361, 253) read, as runs with no prompt ended at y 255 on the left
//  pillar.
constexpr MapWaypoint ZONE1_FAR_GATE{.name = "the far gate", .pin = {.x = 361, .y = 252}, .provisional = false};
//  ~15 pixels east of ZONE1_GATE_APPROACH, so the leg from it walks the
//  gate's own heading; a leg in from the north and a 90 degree turn there
//  ended on the left pillar.
constexpr MapWaypoint ZONE1_GATE_LINE{.name = "the far gate's line", .pin = {.x = 391, .y = 252}, .provisional = true};
//  ~15 pixels before the gate, an exact start (no stop area): runs from the
//  60 spot's loose ends hit a pillar.
constexpr MapWaypoint ZONE1_GATE_APPROACH{.name = "the far gate's approach", .pin = {.x = 376, .y = 252}, .provisional = true};


std::string with_hint(std::string message, std::string_view hint){
    if (hint.empty() || message.ends_with(hint)){
        return message;
    }
    return std::format("{} {}", message, hint);
}

//  An OperationFailedException becomes a failed std::optional<Pokemon::StoryFailure>; its message and a
//  UserSetupError's end with `hint`.
PlzaConsoleAction new_game_handler(StoryEnv& env, std::string_view name, std::string_view hint, void (*step)(StoryEnv&)){
    return [&env, name = std::string(name), hint, step](PlzaBackend& backend, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
        try{
            step(env);
        }catch (OperationFailedException& e){
            const std::string message = with_hint(e.message(), hint);
            backend.log(std::format("[PLZA] {} failed: {}", name, message));
            return Pokemon::StoryFailure{.code = std::format("{}_failed", name), .detail = message};
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

PlzaConsoleAction gate_handler(StoryEnv& env, std::string_view name, GateStep step){
    return [&env, name = std::string(name), step](PlzaBackend& backend, Milliseconds timeout) -> std::optional<Pokemon::StoryFailure>{
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
        {action_name(PlzaAction::CATCH_TUTORIAL), [](StoryEnv& env, PlzaBackend& backend){
            return catch_tutorial(env, backend, /*max_throws=*/6);
        }},
        {action_name(PlzaAction::BATTLE_THEN_CATCH), [](StoryEnv& env, PlzaBackend& backend){
            return battle_then_catch(env, backend, /*max_rounds=*/6);
        }},
        {action_name(PlzaAction::CATCH_IN_ZONE), [](StoryEnv& env, PlzaBackend& backend){
            //  5 registered species, 3 after the tutorial: 2 new; 12 throws.
            return catch_in_zone(env, backend, ZoneCatchGoal{.new_species = 2, .party_species = 5, .max_throws = 12});
        }},
        {action_name(PlzaAction::WALK_BACK_TO_ZONE_ROAD), walk_back_to_zone_road},
        {action_name(PlzaAction::CHECK_OUT_OF_ZONE_1), [](StoryEnv& env, PlzaBackend& backend){
            return check_out_of_zone_1(backend, [&env, &backend]{
                return read_pin_on_map(env, backend);
            });
        }},
        {action_name(PlzaAction::WALK_TO_UNREAD_MARKER), walk_to_unread_marker},
        {action_name(PlzaAction::CLAIM_MABLE_RESEARCH_REWARD), claim_research_reward},
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

} // namespace


PlzaConsoleActions plza_console_actions(StoryEnv& env){
    PlzaConsoleActions actions;
    actions.emplace(action_name(PlzaAction::NEW_GAME_INTRO), new_game_handler(env, action_name(PlzaAction::NEW_GAME_INTRO), NEW_GAME_BY_HAND_HINT, [](StoryEnv& story){
        play_new_game_intro(story.env.console, story.context, story.new_game);
    }));
    actions.emplace(action_name(PlzaAction::CONFIRM_NEW_GAME), new_game_handler(env, action_name(PlzaAction::CONFIRM_NEW_GAME), /*hint=*/"", [](StoryEnv& story){
        confirm_new_game_choices(story.env.console, story.context, story.new_game);
    }));
    actions.emplace(action_name(PlzaAction::RUN_TO_GATE), gate_handler(env, action_name(PlzaAction::RUN_TO_GATE), [](StoryEnv& story, PlzaBackend&, Milliseconds timeout){
        return run_to_gate(story, timeout);
    }));
    actions.emplace(action_name(PlzaAction::AIMED_RUN_TO_GATE), gate_handler(env, action_name(PlzaAction::AIMED_RUN_TO_GATE), aimed_gate_run));
    for (const auto& [name, step] : console_steps()){
        actions.emplace(name, console_handler(env, name, step));
    }
    return actions;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

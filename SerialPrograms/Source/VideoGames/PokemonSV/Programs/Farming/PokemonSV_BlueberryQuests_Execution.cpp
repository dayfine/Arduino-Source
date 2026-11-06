/*  Blueberry Quest Execution
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "PokemonSV_BBQLogger.h"
#include "PokemonSV_BBQSnapshot.h"
#include "PokemonSV_BlueberryCatchPhoto.h"
#include "PokemonSV_BlueberryQuests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

// Returns true after executing one supported quest; false performs no action.
bool run_basic_quest(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS, BBQuests current_quest
){
    switch (current_quest){
    case BBQuests::make_tm:
        quest_make_tm(env.program_info(), console, context);
        return true;
    case BBQuests::travel_500:
        quest_travel_500(env.program_info(), console, context);
        return true;
    case BBQuests::tera_self_defeat:
        quest_tera_self_defeat(env.program_info(), console, context, BBQ_OPTIONS);
        return true;
    case BBQuests::sneak_up:
        quest_sneak_up(env.program_info(), console, context, BBQ_OPTIONS);
        return true;
    case BBQuests::wild_tera:
        quest_wild_tera(env.program_info(), console, context, BBQ_OPTIONS);
        return true;
    case BBQuests::wash_pokemon:
        quest_wash_pokemon(env.program_info(), console, context);
        return true;
    default:
        return false;
    }
}

// Returns true after executing one supported quest; false performs no action.
bool run_event_quest(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS, BBQuests current_quest
){
    switch (current_quest){
    case BBQuests::hatch_egg:
        quest_hatch_egg(env, console, context, BBQ_OPTIONS);
        return true;
    case BBQuests::bitter_sandwich:
    case BBQuests::salty_sandwich:
    case BBQuests::sour_sandwich:
    case BBQuests::spicy_sandwich:
    case BBQuests::sweet_sandwich:
    case BBQuests::sandwich_three:
        quest_sandwich(env, console, context, BBQ_OPTIONS, current_quest);
        return true;
    //case BBQuests::pickup_10:
    //    quest_pickup(env, env.program_info(), console, context, BBQ_OPTIONS);
    //    break;
    case BBQuests::tera_raid:
        quest_tera_raid(env, console, context, BBQ_OPTIONS);
        return true;
    case BBQuests::auto_10:
    case BBQuests::auto_30:
        quest_auto_battle(env, console, context, BBQ_OPTIONS, current_quest);
        return true;
    default:
        return false;
    }
}

// Returns true after executing one supported quest; false performs no action.
bool run_photo_quest(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS, BBQuests current_quest
){
    switch (current_quest){
    //All involve taking pictures
    case BBQuests::photo_fly:
    case BBQuests::photo_swim:
    case BBQuests::photo_canyon:
    case BBQuests::photo_coastal:
    case BBQuests::photo_polar:
    case BBQuests::photo_savanna:
    case BBQuests::photo_normal:
    case BBQuests::photo_fighting:
    case BBQuests::photo_flying:
    case BBQuests::photo_poison:
    case BBQuests::photo_ground:
    case BBQuests::photo_rock:
    case BBQuests::photo_bug:
    case BBQuests::photo_ghost:
    case BBQuests::photo_steel:
    case BBQuests::photo_fire:
    case BBQuests::photo_water:
    case BBQuests::photo_grass:
    case BBQuests::photo_electric:
    case BBQuests::photo_psychic:
    case BBQuests::photo_ice:
    case BBQuests::photo_dragon:
    case BBQuests::photo_dark:
    case BBQuests::photo_fairy:
        quest_photo(env.program_info(), console, context, BBQ_OPTIONS, current_quest);
        return true;
    default:
        return false;
    }
}

// Returns true after executing one supported quest; false performs no action.
bool run_catch_quest(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS, BBQuests current_quest
){
    switch (current_quest){
    //All involve catching a pokemon
    case BBQuests::catch_any:
    case BBQuests::catch_normal:
    case BBQuests::catch_fighting:
    case BBQuests::catch_flying:
    case BBQuests::catch_poison:
    case BBQuests::catch_ground:
    case BBQuests::catch_rock:
    case BBQuests::catch_bug:
    case BBQuests::catch_ghost:
    case BBQuests::catch_steel:
    case BBQuests::catch_fire:
    case BBQuests::catch_water:
    case BBQuests::catch_grass:
    case BBQuests::catch_electric:
    case BBQuests::catch_psychic:
    case BBQuests::catch_ice:
    case BBQuests::catch_dragon:
    case BBQuests::catch_dark:
    case BBQuests::catch_fairy:
        quest_catch(env.program_info(), console, context, BBQ_OPTIONS, current_quest);
        return true;
    default:
        return false;
    }
}

// Dispatch to the same quest routines as the original switch, or report an unsupported quest.
void execute_quest(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS, BBQuests current_quest
){
    if (run_basic_quest(env, console, context, BBQ_OPTIONS, current_quest)
        || run_event_quest(env, console, context, BBQ_OPTIONS, current_quest)
        || run_photo_quest(env, console, context, BBQ_OPTIONS, current_quest)
        || run_catch_quest(env, console, context, BBQ_OPTIONS, current_quest)
    ){
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT, "Unknown quest selection.", console
    );
}

} // namespace

// Execute/retry from the overworld, verify the board and preserve the caller's egg count.
bool process_and_do_quest(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS,
    BBQuests current_quest,
    uint8_t& eggs_hatched
){
    bool quest_completed = false;
    int quest_attempts = 0;

    while (!quest_completed){
        BBQAttemptLog attempt_log(
            console.logger(), /*quest=*/current_quest,
            /*attempt=*/quest_attempts + 1, /*limit=*/static_cast<unsigned>(BBQ_OPTIONS.NUM_RETRIES) + 1
        );
        execute_quest(env, console, context, BBQ_OPTIONS, current_quest);

        attempt_log.route_return();
        //Validate quest was completed by checking list
        std::vector<BBQuests> quest_list = read_quests(env.program_info(), console, context, BBQ_OPTIONS);
        if (std::find(quest_list.begin(), quest_list.end(), current_quest) != quest_list.end()){
            quest_attempts++;
        }else{
            if (current_quest == BBQuests::hatch_egg){
                eggs_hatched++;
                console.log("Eggs hatched: " + std::to_string(eggs_hatched));
            }

            quest_completed = true;
        }

        record_bbq_attempt_result(
            console, attempt_log, current_quest,
            quest_completed ? BBQAttemptOutcome::Completed : BBQAttemptOutcome::StillOnBoard
        );
        if (quest_attempts > BBQ_OPTIONS.NUM_RETRIES){
            BBQLogger(console.logger()).retry_exhausted(current_quest);
            break;
        }
    }
    return quest_completed;
}

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation

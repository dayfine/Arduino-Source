/*  PokemonLZA AutoStory Segment 01
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "PokemonLZA_AutoStory_Segment_01.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

std::string AutoStory_Segment_01::name() const{
    return "01: Get Your Travel Bag Back!";
}
std::string AutoStory_Segment_01::start_text() const{
    return "Arrived at Gare de Lumiose / Lumiose City entrance.";
}
std::string AutoStory_Segment_01::end_text() const{
    return "Recovered travel bag and received starter Pokémon.";
}

void AutoStory_Segment_01::run_segment(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    AutoStory_Checkpoint_01 cp1;
    cp1.run_checkpoint(env, context, options, stats);

    AutoStory_Checkpoint_02 cp2;
    cp2.run_checkpoint(env, context, options, stats);

    stats.m_segment++;
    env.update_stats();
}

std::string AutoStory_Checkpoint_01::name() const{
    return "01-01: Chase Pancham";
}
std::string AutoStory_Checkpoint_01::start_text() const{
    return "Standing at the plaza after Pancham steals travel bag.";
}
std::string AutoStory_Checkpoint_01::end_text() const{
    return "Cornered Pancham in alleyway.";
}

void AutoStory_Checkpoint_01::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_reattempt_loop(
        env, context,
        options.notif_status_update,
        options.notif_error_recoverable,
        stats,
        checkpoint_text(),
        [&](size_t /*attempt*/){
            env.console.log("Running towards Pancham...");
            pbf_move_left_joystick(context, {0, 128}, 3s, 0ms);
            clear_dialog(env.console, context, 20s);
        }
    );
}

std::string AutoStory_Checkpoint_02::name() const{
    return "01-02: Pick Starter & Retrieve Bag";
}
std::string AutoStory_Checkpoint_02::start_text() const{
    return "In front of Pancham choosing starter Pokémon.";
}
std::string AutoStory_Checkpoint_02::end_text() const{
    return "Retrieved travel bag, starter received.";
}

static void select_starter(
    ProControllerContext& context,
    StarterChoice choice
){
    if (choice == StarterChoice::CYNDAQUIL){
        pbf_press_dpad(context, DPAD_DOWN, 200ms, 200ms);
    }else if (choice == StarterChoice::TOTODILE){
        pbf_press_dpad(context, DPAD_DOWN, 200ms, 200ms);
        pbf_press_dpad(context, DPAD_DOWN, 200ms, 200ms);
    }
    pbf_press_button(context, BUTTON_A, 200ms, 500ms);
}

void AutoStory_Checkpoint_02::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_reattempt_loop(
        env, context,
        options.notif_status_update,
        options.notif_error_recoverable,
        stats,
        checkpoint_text(),
        [&](size_t /*attempt*/){
            env.console.log("Selecting starter Pokémon...");
            select_starter(context, options.starter_choice);
            clear_dialog(env.console, context, 30s);
            wait_until_overworld(env.console, context, 30s);
        }
    );
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BBQ Solo Farmer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonTools/StartupChecks/StartProgramChecks.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
//#include "VideoGames/PokemonSV/PokemonSV_Settings.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BBQPointsLedger.h"
#include "PokemonSV_BBQSoloFarmer.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;
 
BBQSoloFarmer_Descriptor::BBQSoloFarmer_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:BBQSoloFarmer",
        STRING_POKEMON + " SV", "BBQ Farmer",
        "Programs/PokemonSV/BBQSoloFarmer.html",
        "Farm Blueberry Quests in the Terarium for BP.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        {
            "PaddleOCRDetection",
        }
    )
{}
struct BBQSoloFarmer_Descriptor::Stats : public StatsTracker{
    Stats()
        : questsCompleted(m_stats["Quests Completed"])
        , saves(m_stats["Saves"])
        , errors(m_stats["Errors"])
        , bp_earned(m_stats["BP Earned"])
        , bp_spent(m_stats["BP Spent"])
    {
        m_display_order.emplace_back("Quests Completed");
        m_display_order.emplace_back("Saves");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
        m_display_order.emplace_back("BP Earned");
        m_display_order.emplace_back("BP Spent");
    }

    void update_BP(const BBQPointsLedger& ledger){
        bp_earned.store(ledger.earned, std::memory_order_relaxed);
        bp_spent.store(ledger.spent, std::memory_order_relaxed);
        bp_per_hour.store(ledger.net_per_hour(), std::memory_order_relaxed);
        has_bp_per_hour.store(true, std::memory_order_relaxed);
    }

    //  Net and BP/hour are derived, so they are shown but not saved. Past runs
    //  have no BP/hour since their time span isn't saved.
    virtual std::string to_str(PrintMode mode) const override{
        std::string str = StatsTracker::to_str(mode);
        if (mode == SAVE_TO_STATS_FILE){
            return str;
        }
        const int64_t net = (int64_t)bp_earned.load(std::memory_order_relaxed) - (int64_t)bp_spent.load(std::memory_order_relaxed);
        str += " - Net BP: " + tostr_u_commas(net);
        if (has_bp_per_hour.load(std::memory_order_relaxed)){
            str += " - BP/hour: " + tostr_u_commas(bp_per_hour.load(std::memory_order_relaxed));
        }
        return str;
    }

    std::atomic<uint64_t>& questsCompleted;
    std::atomic<uint64_t>& saves;
    std::atomic<uint64_t>& errors;
    std::atomic<uint64_t>& bp_earned;
    std::atomic<uint64_t>& bp_spent;
    std::atomic<int64_t> bp_per_hour{0};
    std::atomic<bool> has_bp_per_hour{false};
};
std::unique_ptr<StatsTracker> BBQSoloFarmer_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}
BBQSoloFarmer::BBQSoloFarmer()
    : GO_HOME_WHEN_DONE(true)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        & NOTIFICATION_PROGRAM_FINISH,
        & NOTIFICATION_ERROR_FATAL,
        })
{
    PA_ADD_OPTION(BBQ_OPTIONS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void BBQSoloFarmer::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    StartProgramChecks::check_performance_class_wired_or_wireless(context);
    assert_16_9_720p_min(env.logger(), env.console);

    BBQSoloFarmer_Descriptor::Stats& stats = env.current_stats<BBQSoloFarmer_Descriptor::Stats>();

    //Make sure console type is set
    if (env.console.state().console_type() == ConsoleType::Unknown){
        throw UserSetupError(env.console, "Console Type (Switch 1 or 2) must be specified.");
    }

    //  Connect the controller.
    require_player(env.console, context, BUTTON_LCLICK);
    
    //Fly to plaza
    open_map_from_overworld(env.program_info(), env.console, context);
    fly_to_overworld_from_map(env.program_info(), env.console, context);

    std::vector<BBQuests> quest_list; //all quests
    std::vector<BBQuests> quests_to_do; //do-able quests
    uint8_t eggs_hatched = 0; //Track eggs
    uint64_t num_completed_quests = 0;

    //BP is read from the quest panel screenshots the program already takes.
    BBQPointsLedger bp_ledger;
    int BP = -1;
    auto record_BP = [&](){
        bp_ledger.record(BP, current_time());
        env.log(
            "BP: " + std::to_string(BP) + " (start " + std::to_string(bp_ledger.starting_balance) +
            ", earned " + std::to_string(bp_ledger.earned) + ", spent " + std::to_string(bp_ledger.spent) + ")"
        );
        stats.update_BP(bp_ledger);
        env.update_stats();
    };

    //Test a specific quest
    /*
    BBQuests test_quest = BBQuests::catch_water;
    bool questTest = process_and_do_quest(env, env.console, context, BBQ_OPTIONS, test_quest, eggs_hatched);
    if (questTest){
        env.log("Finished quest.");
    }
    */

    while (num_completed_quests < BBQ_OPTIONS.NUM_QUESTS){
        if (BBQ_OPTIONS.OUT_OF_EGGS == BBQOption::OOEggs::Stop && eggs_hatched >= BBQ_OPTIONS.NUM_EGGS){
            env.log("Stop when out of eggs selected. Stopping program.");
            break;
        }

        //Get and reroll quests until we can at least one
        while (quests_to_do.size() < 1){
            quest_list = read_quests(env.program_info(), env.console, context, BBQ_OPTIONS, &BP);
            record_BP();
            quests_to_do = process_quest_list(env.program_info(), env.console, context, BBQ_OPTIONS, quest_list, eggs_hatched, &bp_ledger);

            //Clear out the regular quest list.
            quest_list.clear();
        }

        for (auto current_quest : quests_to_do){
            //Check if quest was already completed (ex. 500 meters completed while navigating to take a photo)
            quest_list = read_quests(env.program_info(), env.console, context, BBQ_OPTIONS, &BP);
            record_BP();
            if (std::find(quest_list.begin(), quest_list.end(), current_quest) != quest_list.end()){
                env.log("Current quest exists on list. Doing quest.");
                bool questSuccess = process_and_do_quest(env, env.console, context, BBQ_OPTIONS, current_quest, eggs_hatched);
                if (questSuccess){
                    env.log("Quest completed successfully.");
                    stats.questsCompleted++;
                    env.update_stats();
                    num_completed_quests++;
                }else{
                    env.log("Quest did not complete successfully.");
                }
            }else{
                //Note: This doesn't account for case such as "sneak up" being added and then completed alongside the next quest
                env.log("Current quest does not exist on list. Quest completed at some point.");
                stats.questsCompleted++;
                env.update_stats();
                num_completed_quests++;
            }
            quest_list.clear();
        }
        //Clear out the todo list
        quests_to_do.clear();

        //Fix the time to prevent running out of years
        pbf_wait(context, 2000ms);
        context.wait_for_all_requests();
        go_home(env.console, context);
        home_to_date_time(env.console, context, false);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
        resume_game_from_home(env.console, context);

        uint64_t temp_save_num_option = BBQ_OPTIONS.SAVE_NUM_QUESTS;
        if (temp_save_num_option != 0 && num_completed_quests % temp_save_num_option == 0){
            env.log("Saving and resetting.");
            save_game_from_overworld(env.program_info(), env.console, context);
            reset_game(env.program_info(), env.console, context);
            stats.saves++;
        }

        env.update_stats();
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE);
    }

    //No quest panel is read after the last batch, so read the balance once more.
    BP = read_BP(env.program_info(), env.console, context);
    record_BP();
    env.log(bp_ledger.reconciliation(), bp_ledger.reconciled() ? COLOR_BLACK : COLOR_RED);

    if (BBQ_OPTIONS.FIX_TIME_WHEN_DONE){
        go_home(env.console, context);
        home_to_date_time(env.console, context, false);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
        resume_game_from_home(env.console, context);

        open_map_from_overworld(env.program_info(), env.console, context);
        fly_to_overworld_from_map(env.program_info(), env.console, context);
    }

    GO_HOME_WHEN_DONE.run_end_of_program(context);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}
    
}
}
}

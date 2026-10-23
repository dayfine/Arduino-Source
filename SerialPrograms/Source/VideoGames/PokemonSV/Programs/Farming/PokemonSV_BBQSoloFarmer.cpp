/*  BBQ Solo Farmer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <set>
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
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
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
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PanelDeprecation::NOT_DEPRECATED,
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

namespace{

// Forget the failed quests that are gone from the panel, so a quest that
// comes back later gets its attempts again. The first 3 slots are always
// read, as UnableToDetect when OCR fails, but a 4th slot is only there when
// OCR reads it. So only a read of 4 known quests proves a quest is gone; a
// missed read must not give a failing quest new attempts.
void forget_quests_gone_from_panel(std::set<BBQuests>& failed_quests, const std::vector<BBQuests>& panel){
    if (panel.size() < 4 || std::find(panel.begin(), panel.end(), BBQuests::UnableToDetect) != panel.end()){
        return;
    }
    std::erase_if(failed_quests, [&](BBQuests quest){
        return std::find(panel.begin(), panel.end(), quest) == panel.end();
    });
}

// When nothing on the panel is doable, the farmer rerolls it and reads it
// again. If no blue quest can ever be doable, that loop never ends, so the
// settings are checked before starting.
// A quest is done if any exclusion row says run, matching process_quest_list().
bool exclusions_leave_a_blue_quest(const BBQOption& options){
    const std::vector<std::unique_ptr<BBQuestTableRow>> exclusions = options.QUEST_EXCLUSIONS.copy_snapshot();
    for (BBQuests quest : blue_quests){
        if (not_possible_quests.contains(quest)){
            continue;
        }
        bool in_table = false;
        bool run = false;
        for (const std::unique_ptr<BBQuestTableRow>& row : exclusions){
            if (row->quest == quest){
                in_table = true;
                run = run || row->action == BBQAction::run;
            }
        }
        if (!in_table || run){
            return true;
        }
    }
    return false;
}

// Valid settings can still produce a panel with nothing doable, which the
// next reroll fixes. If only 3 of the 31 blue quests are doable, a panel has
// nothing doable 73% of the time, and 30 such panels in a row happen with
// probability 0.008%. More than that means rerolling does not help: the game
// language is wrong so no quest is read, or there is no BP left to reroll.
const size_t MAX_READS_WITHOUT_DOABLE_QUEST = 30;
}

// Quests done at Central Plaza with no flight out: they start at its fly
// point and fly back to it, so the player is where the round actions and the
// next quest expect. Every other route flies to another plaza or area.
bool bbq_quest_is_no_trip(BBQuests quest){
    return quest == BBQuests::make_tm || quest == BBQuests::travel_500;
}

namespace{

// Whether a round that took the completed count from previous_completed
// to completed crossed a nonzero multiple of the save interval.
bool bbq_save_due(uint64_t previous_completed, uint64_t completed, uint64_t interval){
    return interval != 0 && completed > previous_completed
        && completed / interval > previous_completed / interval;
}

}

void run_bbq_rounds(Logger& logger, const BBQOption& options, BBQGameActions& game){
    if (!exclusions_leave_a_blue_quest(options)){
        throw UserSetupError(logger, "The quest exclusions table skips or rerolls every blue quest the program can do.");
    }

    std::vector<BBQuests> quest_list; //all quests
    std::vector<BBQuests> quests_to_do; //do-able quests
    uint8_t eggs_hatched = 0; //Track eggs
    uint64_t num_completed_quests = 0;

    // Quests that failed every attempt do_quest() made. A failed quest stays
    // on the panel, so without this it gets NUM_RETRIES more attempts every
    // round, forever, e.g. a sneak-up whose target does not spawn.
    std::set<BBQuests> failed_quests;
    std::optional<std::vector<BBQuests>> observed_panel;
    std::optional<std::vector<BBQuests>> dispatch_panel;
    auto read_panel = [&](){
        std::vector<BBQuests> panel = game.read_quests();
        forget_quests_gone_from_panel(failed_quests, panel);
        return panel;
    };

    while (num_completed_quests < options.NUM_QUESTS){
        if (options.OUT_OF_EGGS == BBQOption::OOEggs::Stop && eggs_hatched >= options.NUM_EGGS){
            logger.log("Stop when out of eggs selected. Stopping program.");
            break;
        }

        //Get and reroll quests until we can at least one
        size_t reads_without_doable_quest = 0;
        while (quests_to_do.size() < 1){
            if (reads_without_doable_quest++ == MAX_READS_WITHOUT_DOABLE_QUEST){
                throw UserSetupError(
                    logger,
                    "No doable quest after " + std::to_string(MAX_READS_WITHOUT_DOABLE_QUEST) +
                    " quest panel reads. Check the game language and that you have BP to reroll."
                );
            }
            if (observed_panel){
                logger.log("Reusing verified quest panel for scheduling.");
                quest_list = std::move(*observed_panel);
                observed_panel.reset();
                forget_quests_gone_from_panel(failed_quests, quest_list);
            }else{
                logger.log("Reading quest panel for scheduling.");
                quest_list = read_panel();
            }
            BBQuestBatch batch = game.process_quest_list(quest_list, eggs_hatched);
            quests_to_do = std::move(batch.to_do);
            observed_panel = std::move(batch.observed_panel);
            //Work is only returned when nothing changed in game, so the panel
            //it was chosen from is still current for the first dispatch.
            if (!quests_to_do.empty() && !observed_panel){
                dispatch_panel = quest_list;
            }

            // Rerolling costs BP and the failed quests may be doable another
            // time, so stop instead of spending BP to replace them.
            const bool found_doable = !quests_to_do.empty();
            std::erase_if(quests_to_do, [&](BBQuests quest){
                return failed_quests.contains(quest);
            });
            if (found_doable && quests_to_do.empty()){
                logger.log("Every doable quest on the panel failed all its attempts. Stopping program.", COLOR_RED);
                return;
            }

            //Clear out the regular quest list.
            quest_list.clear();
        }

        //Never carry an observation across quest actions or a save and reset.
        observed_panel.reset();
        const uint64_t completed_before_round = num_completed_quests;
        bool replan_after_reroll = false;
        std::optional<std::vector<BBQuests>> rerolled_panel;
        //Cheap, reliable quests first: a quest done early can also finish a later one (e.g.
        //travel-500 while flying to a photo target), and a failure costs less of the round.
        order_quests_by_cost(logger, quests_to_do);

        //The panel read that checked a completed no-trip quest. Nothing runs
        //between it and the next dispatch or plan, so a no-trip quest next or
        //the next plan starts from it instead of reading again.
        std::optional<std::vector<BBQuests>> no_trip_panel;
        for (auto current_quest : quests_to_do){
            //Check if quest was already completed (ex. 500 meters completed while navigating to take a photo)
            if (dispatch_panel){
                logger.log("Reusing verified quest panel for first dispatch.");
                quest_list = std::move(*dispatch_panel);
                dispatch_panel.reset();
            }else if (no_trip_panel && bbq_quest_is_no_trip(current_quest)){
                logger.log("Reusing quest panel read after a no-trip quest for dispatch.");
                quest_list = std::move(*no_trip_panel);
                no_trip_panel.reset();
                forget_quests_gone_from_panel(failed_quests, quest_list);
            }else{
                no_trip_panel.reset();
                logger.log("Reading quest panel before dispatch.");
                quest_list = read_panel();
            }
            if (std::find(quest_list.begin(), quest_list.end(), current_quest) != quest_list.end()){
                logger.log("Current quest exists on list. Doing quest.");
                std::vector<BBQuests> panel_after;
                bool questSuccess = game.do_quest(current_quest, eggs_hatched, panel_after);
                if (questSuccess){
                    logger.log("Quest completed successfully.");
                    game.quest_completed();
                    num_completed_quests++;
                    if (bbq_quest_is_no_trip(current_quest)){
                        no_trip_panel = std::move(panel_after);
                    }
                }else{
                    logger.log("Quest did not complete successfully.");
                    switch (options.OUT_OF_RETRIES){
                    case BBQOption::OutOfRetries::Skip:
                        failed_quests.insert(current_quest);
                        break;
                    case BBQOption::OutOfRetries::Reroll:
                        logger.log("Reroll selected. Rerolling the failed quest.");
                        rerolled_panel = game.reroll_quest(current_quest);
                        logger.log("Reroll finished. Discarding the remaining batch before scheduling.");
                        replan_after_reroll = true;
                        break;
                    case BBQOption::OutOfRetries::Stop:
                        logger.log("Stop selected. Stopping program.", COLOR_RED);
                        return;
                    }
                }
            }else{
                //Note: This doesn't account for case such as "sneak up" being added and then completed alongside the next quest
                logger.log("Current quest does not exist on list. Quest completed at some point.");
                game.quest_completed();
                num_completed_quests++;
            }
            quest_list.clear();
            if (replan_after_reroll){
                //A replacement belongs in the next plan, before more old queued
                //work. Finish normal round actions so save milestones are kept.
                break;
            }
        }
        //Clear out the todo list
        quests_to_do.clear();

        uint64_t temp_save_num_option = options.SAVE_NUM_QUESTS;
        // A round can cross a milestone without ending at an exact multiple.
        const bool save = bbq_save_due(completed_before_round, num_completed_quests, temp_save_num_option);

        //A no-trip quest ended the round at Central Plaza, and fixing the date
        //does not change the quests, so plan the next quests from its read
        //at once. Round actions still run before a save or the last stop.
        if (no_trip_panel && !save && num_completed_quests < options.NUM_QUESTS){
            logger.log("Chaining after a no-trip quest. Skipping round actions.");
            observed_panel = std::move(no_trip_panel);
            continue;
        }
        game.end_round(save);

        //The reroll was the last game action of the round. Fixing the date
        //does not change the quests, so its verified read can seed the next
        //plan. A save resets the game, so the panel is read again after one.
        if (rerolled_panel && !save){
            observed_panel = std::move(rerolled_panel);
        }
    }
}


namespace{

class BBQSoloFarmerGameActions : public BBQGameActions{
public:
    BBQSoloFarmerGameActions(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        BBQOption& options, BBQSoloFarmer_Descriptor::Stats& stats,
        EventNotificationOption& status_update
    )
        : m_env(env), m_context(context)
        , m_options(options), m_stats(stats)
        , m_status_update(status_update)
    {}

    virtual std::vector<BBQuests> read_quests() override{
        int BP = -1;
        std::vector<BBQuests> quests = PokemonSV::read_quests(m_env.program_info(), m_env.console, m_context, m_options, &BP);
        record_BP(BP);
        return quests;
    }
    virtual BBQuestBatch process_quest_list(
        const std::vector<BBQuests>& quests, uint8_t eggs_hatched
    ) override{
        //Reroll verification reads can seed the next plan instead of a
        //read_quests() call, so their BP is recorded too.
        return PokemonSV::process_quest_list(
            m_env.program_info(), m_env.console, m_context, m_options, quests, eggs_hatched,
            [this](int BP){ record_BP(BP); }
        );
    }
    virtual bool do_quest(BBQuests quest, uint8_t& eggs_hatched, std::vector<BBQuests>& panel_after) override{
        return process_and_do_quest(m_env, m_env.console, m_context, m_options, quest, eggs_hatched, &panel_after);
    }
    virtual std::optional<std::vector<BBQuests>> reroll_quest(BBQuests quest) override{
        int BP = -1;
        std::vector<BBQuests> quests = PokemonSV::read_quests(m_env.program_info(), m_env.console, m_context, m_options, &BP);
        record_BP(BP);
        auto iter = std::find(quests.begin(), quests.end(), quest);
        if (iter == quests.end()){
            m_env.log("Failed quest is no longer on the panel. Not rerolling.");
            return std::nullopt;
        }
        return reroll_quest_slot(
            m_env.program_info(), m_env.console, m_context, m_options, quests, iter - quests.begin(),
            [this](int BP){ record_BP(BP); }
        );
    }
    virtual void quest_completed() override{
        m_stats.questsCompleted++;
        m_env.update_stats();
    }
    virtual void end_round(bool save) override{
        //Fix the time to prevent running out of years
        pbf_wait(m_context, 2000ms);
        m_context.wait_for_all_requests();
        go_home(m_env.console, m_context);
        home_to_date_time(m_env.console, m_context, false);
        pbf_press_button(m_context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(m_context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(m_context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
        resume_game_from_home(m_env.console, m_context);

        if (save){
            m_env.log("Saving and resetting.");
            save_game_from_overworld(m_env.program_info(), m_env.console, m_context);
            reset_game(m_env.program_info(), m_env.console, m_context);
            m_stats.saves++;
        }

        m_env.update_stats();
        send_program_status_notification(m_env, m_status_update);
    }

    //BP is read from the quest panel screenshots the program already takes.
    void record_BP(int BP){
        m_bp_ledger.record(BP, current_time());
        m_env.log(
            "BP: " + std::to_string(BP) + " (start " + std::to_string(m_bp_ledger.starting_balance) +
            ", earned " + std::to_string(m_bp_ledger.earned) + ", spent " + std::to_string(m_bp_ledger.spent) + ")"
        );
        m_stats.update_BP(m_bp_ledger);
        m_env.update_stats();
    }

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    BBQOption& m_options;
    BBQSoloFarmer_Descriptor::Stats& m_stats;
    EventNotificationOption& m_status_update;
    BBQPointsLedger m_bp_ledger;
};

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
    
    //Fly to plaza. The program can start anywhere in the Terarium, so use the same
    //checked return as after each quest instead of flying to the map cursor.
    return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);

    //Test a specific quest
    /*
    uint8_t eggs_hatched = 0;
    BBQuests test_quest = BBQuests::catch_water;
    bool questTest = process_and_do_quest(env, env.console, context, BBQ_OPTIONS, test_quest, eggs_hatched);
    if (questTest){
        env.log("Finished quest.");
    }
    */

    BBQSoloFarmerGameActions game(env, context, BBQ_OPTIONS, stats, NOTIFICATION_STATUS_UPDATE);
    run_bbq_rounds(env.logger(), BBQ_OPTIONS, game);

    //No quest panel is read after the last batch, so read the balance once more.
    game.record_BP(read_BP(env.program_info(), env.console, context));

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

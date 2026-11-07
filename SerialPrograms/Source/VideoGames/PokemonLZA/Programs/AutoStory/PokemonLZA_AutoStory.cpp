/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/FatalProgramException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_AutoStory_Segment_01.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto-Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Automate the main story of Pokémon Legends: Z-A.",
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::make_unique<AutoStoryStats>();
}

std::vector<std::unique_ptr<AutoStory_Segment>> make_segment_list(){
    std::vector<std::unique_ptr<AutoStory_Segment>> list;
    list.emplace_back(std::make_unique<AutoStory_Segment_01>());
    return list;
}

const std::vector<std::unique_ptr<AutoStory_Segment>>& ALL_AUTO_STORY_SEGMENTS(){
    static std::vector<std::unique_ptr<AutoStory_Segment>> list = make_segment_list();
    return list;
}

StringSelectDatabase make_segments_database(){
    StringSelectDatabase db;
    int idx = 0;
    for (const auto& seg : ALL_AUTO_STORY_SEGMENTS()){
        db.add_entry(StringSelectEntry(std::to_string(idx), seg->name()));
        idx++;
    }
    return db;
}

const StringSelectDatabase& SEGMENTS_DATABASE(){
    static StringSelectDatabase db = make_segments_database();
    return db;
}

AutoStory::~AutoStory(){
    START_SEGMENT.remove_listener(*this);
    END_SEGMENT.remove_listener(*this);
}

void AutoStory::register_options(){
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(STARTER_CHOICE);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(START_SEGMENT);
    PA_ADD_OPTION(END_SEGMENT);
    PA_ADD_OPTION(START_DESCRIPTION);
    PA_ADD_OPTION(END_DESCRIPTION);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
    START_SEGMENT.add_listener(*this);
    END_SEGMENT.add_listener(*this);
}

AutoStory::AutoStory()
    : LANGUAGE("<b>Game Language:</b>", IV_READER().languages(), LockMode::LOCK_WHILE_RUNNING, true)
    , STARTER_CHOICE("<b>Starter Choice:</b>", {
        {StarterChoice::CHIKORITA, "chikorita", "Chikorita"},
        {StarterChoice::CYNDAQUIL, "cyndaquil", "Cyndaquil"},
        {StarterChoice::TOTODILE,  "totodile",  "Totodile"},
    }, LockMode::LOCK_WHILE_RUNNING, StarterChoice::CYNDAQUIL)
    , START_SEGMENT("<b>Start Segment:</b>", SEGMENTS_DATABASE(), LockMode::LOCK_WHILE_RUNNING, "0")
    , END_SEGMENT("<b>End Segment:</b>", SEGMENTS_DATABASE(), LockMode::UNLOCK_WHILE_RUNNING, std::to_string(ALL_AUTO_STORY_SEGMENTS().size() - 1))
    , START_DESCRIPTION("Start: " + ALL_AUTO_STORY_SEGMENTS()[0]->start_text())
    , END_DESCRIPTION("End: " + ALL_AUTO_STORY_SEGMENTS().back()->end_text())
    , GO_HOME_WHEN_DONE(true)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    register_options();
}

void AutoStory::on_config_value_changed(void* /*object*/){
    size_t start_idx = get_start_segment_index();
    size_t end_idx = get_end_segment_index();

    if (start_idx < ALL_AUTO_STORY_SEGMENTS().size()){
        START_DESCRIPTION.set_text("Start: " + ALL_AUTO_STORY_SEGMENTS()[start_idx]->start_text());
    }
    if (end_idx < ALL_AUTO_STORY_SEGMENTS().size()){
        END_DESCRIPTION.set_text("End: " + ALL_AUTO_STORY_SEGMENTS()[end_idx]->end_text());
    }
}

size_t AutoStory::get_start_segment_index(){
    return START_SEGMENT.index();
}

size_t AutoStory::get_end_segment_index(){
    return END_SEGMENT.index();
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AutoStoryStats& stats = env.current_stats<AutoStoryStats>();

    size_t start_idx = get_start_segment_index();
    size_t end_idx = get_end_segment_index();

    if (start_idx > end_idx){
        throw UserSetupError(env.logger(), "The start segment cannot be after the end segment.");
    }

    AutoStoryOptions options{
        LANGUAGE,
        STARTER_CHOICE,
        BATTLE_AI,
        NOTIFICATION_STATUS_UPDATE,
        NOTIFICATION_ERROR_RECOVERABLE,
    };

    require_player(env.console, context, BUTTON_L);

    for (size_t seg = start_idx; seg <= end_idx; seg++){
        env.console.log("Starting Segment: " + ALL_AUTO_STORY_SEGMENTS()[seg]->name(), COLOR_BLUE);
        ALL_AUTO_STORY_SEGMENTS()[seg]->run_segment(env, context, options, stats);
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

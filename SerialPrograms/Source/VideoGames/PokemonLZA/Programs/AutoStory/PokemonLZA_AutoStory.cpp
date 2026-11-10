/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryTable.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A. Prototype: Main Missions 01-03 as recorded on a console; plays from a new game to Wild Zone 1, the rest waits for code.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::make_unique<AutoStoryStats>();
}


namespace{

//  The start and end dropdowns' entries. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        std::vector<std::string> labels;
        for (const StoryCheckpoint& row : story_checkpoints()){
            labels.push_back(story_checkpoint_label(row));
        }
        return story_checkpoint_database(labels);
    }();
    return database;
}

} // namespace


AutoStory::AutoStory()
    : RUN(checkpoint_database(), Gen8ToGen9Languages_Database())
    , STARTER(
        "<b>Starter Pokemon:</b>",
        {
            {StarterChoice::CHIKORITA, "chikorita", "Chikorita"},
            {StarterChoice::TEPIG,     "tepig",     "Tepig"},
            {StarterChoice::TOTODILE,  "totodile",  "Totodile"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StarterChoice::CHIKORITA
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= rows.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    require_player(env.console, context, BUTTON_L);

    StoryEnv story{.env = env, .context = context, .language = RUN.language(), .battle_ai = BATTLE_AI};
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(story, STARTER);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;

    if (const std::optional<VerifyPolicy> verify = RUN.VERIFY.policy()){
        const VerifyReport report = engine.verify_checkpoint(rows[first], state, *verify);
        if (!report.verified){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format(
                    "AutoStory verify: {} {}/{} passed, {} needed{}", report.checkpoint, report.passes, report.runs,
                    verify->passes_needed, report.failure.succeeded() ? std::string() : ": " + to_string(report.failure)
                )
            );
        }
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::span<const StoryCheckpoint> checkpoints(rows.data() + first, last - first + 1);
    const SegmentReport report = engine.run_segment("plza", checkpoints, state, RUN.policy().attempts_per_checkpoint);
    record_story_counts(env.current_stats<AutoStoryStats>(), report.story_counts());
    env.update_stats();
    if (!report.completed){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure)
            )
        );
    }
    env.console.log("AutoStory: done through " + rows[last].id + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

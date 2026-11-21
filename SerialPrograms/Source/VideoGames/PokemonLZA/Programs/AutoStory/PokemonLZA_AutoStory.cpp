/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        std::format("{} LZA", STRING_POKEMON), "Auto Story",
        "Programs/PokemonLZA/AutoStory.html",
        "Play the main story of Pokemon Legends: Z-A through Main Mission 01 (checkpoint 01.01, from the station platform arrival). "
        "Load the latest save first unless that is turned off.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


namespace{

//  The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase entries;
        for (const StoryCheckpoint& checkpoint : builtin_story_data().checkpoints){
            entries.add_entry(StringSelectEntry(checkpoint.id, std::format("{} {}", checkpoint.id, checkpoint.title)));
        }
        return entries;
    }();
    return database;
}

void check_checkpoint_range(ConsoleHandle& console, size_t first, size_t last, size_t checkpoints){
    if (last >= checkpoints){
        throw UserSetupError(console, "The checkpoint selection is out of range: pick the start and end checkpoints again.");
    }
    if (first > last){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
}

constexpr std::string_view ATTEMPTS_PER_CHECKPOINT_LABEL =
    "<b>Attempts per Checkpoint:</b><br>A failed checkpoint that started from a save reloads it and tries again, up to this many times in all.";

constexpr std::string_view LOAD_SAVE_FIRST_LABEL =
    "Load the save first: loads the latest save before starting. "
    "Off: starts from wherever the game is, which must be the checkpoint's start.";

//  Captures the failure screenshot while the game is still on screen, then
//  immediately suspends to Switch HOME so PLZA cannot be attacked or overwrite
//  the save while stopped.
[[noreturn]] void stop_with_error(
    ConsoleHandle& console, ProControllerContext& context,
    ErrorReportMode mode, std::string message
){
    const VideoSnapshot frame = console.video().snapshot();
    go_home(console, context);
    OperationFailedExceptionWithScreenshot::fire(mode, std::move(message), console, frame.frame);
}

//  Starts from the latest save. Throws OperationFailedException if it does not load.
void load_first_save(ConsoleHandle& console, ProControllerContext& context, StoryGame& game, PlzaBackend& backend, const StoryCheckpoint& checkpoint){
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(game, backend, checkpoint);
    if (loaded){
        stop_with_error(
            console, context, ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", checkpoint.id, *loaded)
        );
    }
    console.log(std::format("AutoStory: loaded the latest save for {}.", checkpoint.id));
}

} // namespace


AutoStory::AutoStory()
    : RANGE("Checkpoints", checkpoint_database())
    , ATTEMPTS_PER_CHECKPOINT(std::string(ATTEMPTS_PER_CHECKPOINT_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/3, /*min_value=*/1, /*max_value=*/5)
    , LOAD_SAVE_FIRST(std::string(LOAD_SAVE_FIRST_LABEL), LockMode::LOCK_WHILE_RUNNING, /*default_value=*/true)
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RANGE);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(LOAD_SAVE_FIRST);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    StoryEnv story{.env = env, .context = context};
    AutosaveSightings sightings;
    StoryData story_data = load_plza_story();
    note_autosaves(story_data.checkpoints, sightings);
    std::vector<StoryCheckpoint>& checkpoints = story_data.checkpoints;
    const size_t first = RANGE.start_index();
    const size_t last = RANGE.end_index();
    check_checkpoint_range(env.console, first, last, checkpoints.size());
    require_player(env.console, context, BUTTON_L);

    PlzaConsoleBackend backend(story);
    StoryGame game = make_plza_story_game(story, story_data, sightings);
    RunState state;
    CheckpointRunOptions run_options{
        .attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT,
        .reload_when_chained = story_data.reload_when_chained,
        .starts = story_data.starts,
    };
    if (LOAD_SAVE_FIRST){
        load_first_save(env.console, context, game, backend, checkpoints[first]);
    }
    env.console.log(std::format("AutoStory: {} has no start signature: running it, no start check", checkpoints[first].id));
    backend.log(std::format("[Boundary] {} start: unchecked", checkpoints[first].id));

    const std::optional<CheckpointFailure> failure = run_plza_checkpoints(
        game, std::span<StoryCheckpoint>(checkpoints).subspan(first, last - first + 1), run_options, backend, state, sightings
    );
    if (failure){
        stop_with_error(
            env.console, context, ErrorReportMode::SEND_ERROR_REPORT,
            std::format("AutoStory stopped at {}: {}", failure->checkpoint_id, failure->failure)
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", checkpoints[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

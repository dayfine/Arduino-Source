/*  PokemonLZA AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryConfirmChoices.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: from a new game to the end of Main Mission 01, picking Chikorita. "
        "Start a new game and pick its language, look and name by hand: checkpoint 01.00 starts on the Confirm Choices screen that follows. "
        "It checks that screen, presses nothing anywhere else, and picks Ready!. Later checkpoints load the latest save first.",
        PanelQualityStanding::DEVELOPER,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


namespace{

//  The start and end dropdowns' entries: "01.00 New game from ...", by id.
//  The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = []{
        StringSelectDatabase entries;
        for (const StoryCheckpoint& row : builtin_story_data().checkpoints){
            entries.add_entry(StringSelectEntry(row.id, std::format("{} {}", row.id, row.title)));
        }
        return entries;
    }();
    return database;
}

//  Throws UserSetupError unless rows `first` to `last` are in a table of
//  `rows` rows, in order.
void check_checkpoint_range(ConsoleHandle& console, size_t first, size_t last, size_t rows){
    if (last >= rows){
        throw UserSetupError(console, "The checkpoint selection is out of range: pick the start and end checkpoints again.");
    }
    if (first > last){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
}

//  Connects the controller for a run from `first_row`. 01.00 starts on a
//  screen that proves the save is fresh, so it is checked before any press,
//  the connect press included.
void connect_controller(ConsoleHandle& console, ProControllerContext& context, const StoryCheckpoint& first_row){
    if (story_row_starts_without_save(first_row)){
        require_confirm_choices(console, context);
    }
    require_player(console, context, BUTTON_L);
}

} // namespace


AutoStory::AutoStory()
    : RANGE("<b>Checkpoints:</b>", checkpoint_database())
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per Checkpoint:</b><br>A failed checkpoint that started from a save reloads it and tries again, up to this many times in all.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, 5
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RANGE);
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const StoryData& story_data = builtin_story_data();
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RANGE.start_index();
    const size_t last = RANGE.end_index();
    check_checkpoint_range(env.console, first, last, rows.size());
    connect_controller(env.console, context, rows[first]);

    StoryEnv story{.env = env, .context = context, .battle_ai = BATTLE_AI};
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(story, story_data);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;

    //  A run that starts after the new game starts from the latest save
    //  (01.01: the platform autosave), not from wherever the game is, and
    //  stops if that save is older than its start.
    if (!story_row_starts_without_save(rows[first])){
        const Outcome loaded = load_checkpoint_start(game, backend, rows[first]);
        if (!loaded.succeeded()){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format("AutoStory: could not load the save for {}: {}", rows[first].id, to_string(loaded))
            );
        }
        env.console.log(std::format("AutoStory: loaded the latest save for {}.", rows[first].id));
    }

    const std::vector<StoryCheckpoint> checkpoints = story_run_rows(std::span<const StoryCheckpoint>(rows), first, last);
    const SegmentReport report = run_plza_segments(game, story_data, checkpoints, ATTEMPTS_PER_CHECKPOINT, engine, backend, state);
    if (!report.completed){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format(
                "AutoStory stopped at {} {}: {}", report.failed_checkpoint, report.failed_event, to_string(report.failure)
            )
        );
    }
    env.console.log(std::format("AutoStory: done through {}.", rows[last].id));
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

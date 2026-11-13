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
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_StoryLog.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Actions/PokemonLZA_AutoStoryIntro.h"
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
        "Play the main story of Pokemon Legends: Z-A. Prototype: plays a new game to the station platform, as recorded on a console. Checkpoint 01.00 (a new game) starts on the Switch HOME screen with PLZA's tile selected and no save: it launches the selected tile without moving the cursor (closing it first if it is running) and waits for the language list. Go Home When Done leaves HOME with PLZA's tile selected, ready for the next new game.",
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
    , PLAYER_NAME(
        false,
        "<b>Player Name:</b><br>New game only. Letters and digits; a capital only as the first letter.",
        LockMode::LOCK_WHILE_RUNNING,
        "Kosmos",
        "Kosmos"
    )
    , PLAYER_LOOK(
        "<b>Player Look:</b><br>New game only. 1 to 8: left to right along the top row, then the bottom row.",
        LockMode::LOCK_WHILE_RUNNING,
        1, 1, (uint8_t)LOOK_COUNT
    )
    , NAME_FIRST_LETTER_AUTO_CAPS(
        "<b>Keyboard Capitalizes the First Letter:</b><br>New game only. Tick if the name comes out in lower case: the name keyboard capitalizes by itself and Shift would undo it.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(PLAYER_LOOK);
    PA_ADD_OPTION(NAME_FIRST_LETTER_AUTO_CAPS);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

//  Checkpoints after the first start from a save (01.01 from the platform
//  autosave), so the run loads it first instead of assuming the game is
//  already there.
void load_start_save(
    ConsoleHandle& console, const StoryGame<ZonePose>& game, StoryBackend<ZonePose>& backend,
    const std::vector<StoryCheckpoint>& rows, size_t first
){
    if (first == 0){
        return;
    }
    const Outcome loaded = game.reload(backend);
    if (!loaded.succeeded()){
        throw OperationFailedException(
            ErrorReportMode::NO_ERROR_REPORT,
            std::format("AutoStory: could not load the save for {}: {}", rows[first].id, to_string(loaded))
        );
    }
    console.log(std::format("AutoStory: loaded the save for {}.", rows[first].id));
}

//  The story for this run: the built-in checkpoints, with those of
//  story_dev_folder() in their place, read now so a run after an edit
//  plays the edited walks. Logs each dev file it uses; a bad one fails the
//  run.
StoryData load_run_story(ConsoleHandle& console){
    const std::string folder = story_dev_folder();
    try{
        StoryData data = load_story_data(read_story_dev_files(folder));
        for (const auto& [name, path] : data.dev_files){
            console.log(std::format("AutoStory: using dev checkpoint {} from {}", name, path));
        }
        return data;
    }catch (ParseException& e){
        throw UserSetupError(console, std::format("AutoStory: bad dev checkpoint in {}: {}", folder, e.message()));
    }
}

} // namespace

NewGameChoices AutoStory::new_game_choices(ConsoleHandle& console, bool check) const{
    NewGameChoices choices{.name = PLAYER_NAME, .look = PLAYER_LOOK, .name_first_letter_auto_caps = NAME_FIRST_LETTER_AUTO_CAPS};
    const std::string problem = check ? new_game_problem(RUN.language(), choices) : std::string();
    if (!problem.empty()){
        throw UserSetupError(console, problem);
    }
    return choices;
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const StoryData story_data = load_run_story(env.console);
    const std::vector<StoryCheckpoint>& rows = story_data.checkpoints;
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= rows.size()){
        throw UserSetupError(env.console, "The start checkpoint must not come after the end checkpoint.");
    }
    require_player(env.console, context, BUTTON_L);

    StoryEnv story{
        .env = env, .context = context, .new_game = new_game_choices(env.console, first == 0),
        .language = RUN.language(), .battle_ai = BATTLE_AI
    };
    PlzaConsoleBackend backend(story);
    const StoryGame<ZonePose> game = make_plza_story_game(story, story_data.autosaves);
    StoryEngine<ZonePose> engine(game, backend);
    RunState state;
    load_start_save(env.console, game, backend, rows, first);

    if (const std::optional<VerifyPolicy> verify = RUN.VERIFY.policy()){
        const VerifyReport report = engine.verify_checkpoint(rows[first], *verify, state);
        if (!report.verified){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                std::format(
                    "AutoStory verify: {} {}/{} passed, {} needed{}", report.checkpoint, report.passes, report.runs,
                    verify->passes_needed, report.failure.succeeded() ? std::string() : std::format(": {}", to_string(report.failure))
                )
            );
        }
        send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
        return;
    }
    const std::span<const StoryCheckpoint> checkpoints(rows.data() + first, last - first + 1);
    const SegmentReport report = engine.run_segment("plza", checkpoints, RUN.policy().attempts_per_checkpoint, state);
    record_story_counts(report.story_counts(), env.current_stats<AutoStoryStats>());
    env.update_stats();
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

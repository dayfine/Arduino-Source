/*  Mail Glitch Activation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StoryPreflight.h"
#include "Pokemon/AutoStory/Pokemon_StoryRun.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Options/Pokemon_GameLanguages.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryFailureClasses.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryLaunch.h"
#include "PokemonFRLG_MailGlitchActivation.h"
#include "PokemonFRLG_MailGlitchTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


MailGlitchActivation_Descriptor::MailGlitchActivation_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:MailGlitchActivation",
        Pokemon::STRING_POKEMON + " FRLG", "Mail Glitch Activation",
        "",
        "Activate the FRLG Retro Mail glitch on MIMIEN (Knock Off + Recycle) from any outdoor Fly-capable save.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> MailGlitchActivation_Descriptor::make_stats() const{
    return std::make_unique<Pokemon::AutoStoryStats>();
}


namespace{

const StringSelectDatabase& mail_glitch_database(){
    static const std::vector<std::string> labels = []{
        std::vector<std::string> result;
        for (const StoryCheckpoint& checkpoint : mail_glitch_checkpoints()){
            result.push_back(Pokemon::story_checkpoint_label(checkpoint));
        }
        return result;
    }();
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(labels);
    return database;
}

Pokemon::StoryGameHooks mail_glitch_hooks(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::StoryGameHooks game;
    game.preflight = [&story]{
        return Pokemon::StoryPreflight::read(story.console, story.context);
    };
    game.load_save = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        return launch_frlg_save(story.console, story.context);
    };
    game.check_save_place = [&story]{
        return check_save_place(story, story.start_map);
    };
    game.after_load = [&story](Pokemon::RunState&){
        story.supplies = story.saved_supplies;
        story.supplies.counts_read = false;
        story.party.invalidate_species();
        story.party.users() = story.saved_field_moves.users();
        return Pokemon::outcome_ok();
    };
    game.log = [&env](const std::string& line){
        env.console.log(tag_failure_line(line));
    };
    game.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return game;
}

} // namespace


MailGlitchActivation::MailGlitchActivation()
    : RUN(mail_glitch_database(), Pokemon::FireRedLeafGreenLanguages_Database())
    , PLAYER_GENDER(
        "<b>Player:</b><br>Matches the player sprite for facing checks.",
        {
            {PlayerGender::BOY, "boy", "Boy"},
            {PlayerGender::GIRL, "girl", "Girl"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        PlayerGender::BOY
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({&NOTIFICATION_STATUS_UPDATE, &NOTIFICATION_PROGRAM_FINISH, &NOTIFICATION_ERROR_FATAL})
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(PLAYER_GENDER);
    PA_ADD_OPTION(NOTIFICATIONS);
}


void MailGlitchActivation::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& all = mail_glitch_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    if (first > last || last >= all.size()){
        throw UserSetupError(env.console, "Start checkpoint must not come after end checkpoint.");
    }

    std::vector<StoryCheckpoint> rows(all.begin() + first, all.begin() + last + 1);
    StoryEnv story{env.console, context, rows.front().supplies, rows.front().supplies, false, RUN.check_level()};
    story.language = RUN.language();
    story.player_gender = PLAYER_GENDER;
    story.start_frames = program_start_frames();
    story.pause = Pokemon::StoryPausePoints(RUN.breakpoints(), Pokemon::make_story_pause_hook(context));
    story.pause.set_log([&env](std::string_view line){
        env.console.log(std::string(line));
    });

    Pokemon::StoryGameHooks game = mail_glitch_hooks(env, story);
    Pokemon::RunState state;
    if (rows.front().live_start || !rows.front().start){
        const Pokemon::Outcome preflight = Pokemon::prepare_story_start(rows.front().id, Pokemon::StoryStart::LIVE, game, state);
        if (!preflight.succeeded()){
            throw UserSetupError(env.console, "Mail Glitch Activation preflight failed: " + preflight.detail);
        }
    }else{
        story.has_save = true;
        story.start_map = Pokemon::grid_pose_of(*rows.front().start).map;
        story.start_menu = rows.front().menu;
        const Pokemon::Outcome loaded = Pokemon::prepare_story_start(rows.front().id, Pokemon::StoryStart::FROM_SAVE, game, state);
        if (!loaded.succeeded()){
            throw OperationFailedException(
                ErrorReportMode::NO_ERROR_REPORT,
                "Could not load save for " + rows.front().id + ": " + Pokemon::to_string(loaded)
            );
        }
    }

    StoryExecutor executor(story);
    executor.set_pause_points(&story.pause);
    Pokemon::Segment segment = make_story_segment(rows, executor);
    Pokemon::SegmentHooks hooks = Pokemon::story_segment_hooks(game);
    const Pokemon::SegmentPolicy policy = RUN.policy();

    Pokemon::SegmentReport report;
    try{
        report = run_segment(Pokemon::SegmentRange{rows.front().id, rows.back().id}, policy, segment, hooks, state);
    }catch (OperationFailedException&){
        Pokemon::log_story_failure(game);
        throw;
    }
    Pokemon::finish_story_run(report, rows.back().id, env.current_stats<Pokemon::AutoStoryStats>(), game);
    env.update_stats();

    const std::string summary = std::format("Mail Glitch Activation: completed through {}.", rows.back().id);
    env.console.log(summary, COLOR_BLUE);
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH, summary);
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

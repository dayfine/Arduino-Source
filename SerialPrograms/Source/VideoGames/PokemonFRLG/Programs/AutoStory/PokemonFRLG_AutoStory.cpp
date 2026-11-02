/*  AutoStory
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_NamingScreenKeyboard.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:AutoStory",
        Pokemon::STRING_POKEMON + " FRLG", "AutoStory",
        "",
        "Play FireRed (English) from a new game to the Boulder Badge.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::make_unique<Pokemon::AutoStoryStats>();
}


namespace{

//  The start and end dropdowns' entries: the table's checkpoints, 00.01 to
//  00.11. The options hold a reference to it.
const StringSelectDatabase& checkpoint_database(){
    static const StringSelectDatabase database = Pokemon::story_checkpoint_database(story_checkpoints());
    return database;
}

}

AutoStory::AutoStory()
    : RUN(checkpoint_database())
    , NEW_GAME_FROM_TITLE(
        "<b>New game from the title screen:</b><br>The game is on its title screen with no save data. "
        "Needed to start at 00.01. Otherwise the program loads the save, which must be the one the start checkpoint begins from.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , PLAYER_NAME(
        false,
        "<b>Player name:</b><br>Typed on the new game's keyboard (00.01). Empty keeps the game's default name. "
        "The rival always keeps the default name.",
        LockMode::LOCK_WHILE_RUNNING,
        "Kosmos", ""
    )
    , STOP_FOR_SID_HELPER(
        "<b>Stop for SID Helper:</b><br>Off by default. At 00.01, stop on the player's keyboard with the name typed and OK not pressed, "
        "where the SID Helper program starts. After SID Helper, start AutoStory at 00.01b, with the game as SID Helper left it "
        "(the Trainer Card, or HOME): it closes the card, sets the options and saves in the bedroom.<br>"
        "SID Helper needs the text speed and button settings already saved. On a cartridge with that save, choose NEW GAME "
        "by hand and start AutoStory on the first message.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , STOP_FOR_STARTER_RNG(
        "<b>Stop for Starter RNG:</b><br>Off by default. At the end of 00.04, save south of the table facing Squirtle's ball "
        "and stop, where the Starter RNG program starts. After Starter RNG, save right after the rival's battle without moving "
        "and start AutoStory at 00.05.<br>"
        "With this off, 00.04 and 00.04b run as one checkpoint, as before.<br>"
        "With this on, the run lists the Starter RNG targets to try, in order, from the RNG plan options below.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , RNG_SOUND(
        "<b>RNG plan: Sound:</b><br>The in-game sound setting, as in Starter RNG. It picks the seeds.",
        {
            {SoundSetting::Mono, "mono", "Mono"},
            {SoundSetting::Stereo, "stereo", "Stereo"}
        },
        LockMode::LOCK_WHILE_RUNNING,
        SoundSetting::Mono
    )
    , RNG_TID(
        "<b>RNG plan: TID:</b><br>The Trainer ID SID Helper read.",
        LockMode::LOCK_WHILE_RUNNING,
        0
    )
    , RNG_SID(
        false,
        "<b>RNG plan: SID:</b><br>The SID, once known. Empty: the targets are planned for the SIDs SID Helper lists, "
        "from the two options below.",
        LockMode::LOCK_WHILE_RUNNING,
        "", ""
    )
    , SID_HELPER_ADVANCES(
        "<b>RNG plan: SID Helper's Target Advances:</b><br>As set in SID Helper.",
        LockMode::LOCK_WHILE_RUNNING,
        3001, 2275
    )
    , SID_HELPER_CANDIDATES(
        "<b>RNG plan: SID Helper's # Candidate SIDs:</b><br>As set in SID Helper. The middle one counts as the likeliest.",
        LockMode::LOCK_WHILE_RUNNING,
        5, 1, 11
    )
    , RNG_SHINY_REQUIRED(
        "<b>RNG plan: Shiny required:</b><br>Only targets shiny for a SID candidate, tried until one comes out shiny. "
        "Off: the quickest target that passes the nature and IV filters.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , RNG_NATURES(
        false,
        "<b>RNG plan: Natures:</b><br>Separated by commas, e.g. \"Modest, Timid\". Empty: any nature.",
        LockMode::LOCK_WHILE_RUNNING,
        "", "Modest, Timid"
    )
    , RNG_MIN_IVS(
        false,
        "<b>RNG plan: Minimum IVs:</b><br>HP/Atk/Def/SpA/SpD/Spe.",
        LockMode::LOCK_WHILE_RUNNING,
        "0/0/0/0/0/0", "0/0/0/0/0/0"
    )
    , RNG_MAX_ATTEMPTS(
        "<b>RNG plan: Max attempts:</b><br>The most targets to list.",
        LockMode::LOCK_WHILE_RUNNING,
        5, 1, 20
    )
    , NOTIFICATIONS({
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(RUN);
    PA_ADD_OPTION(NEW_GAME_FROM_TITLE);
    PA_ADD_OPTION(PLAYER_NAME);
    PA_ADD_OPTION(STOP_FOR_SID_HELPER);
    PA_ADD_OPTION(STOP_FOR_STARTER_RNG);
    PA_ADD_OPTION(RNG_SOUND);
    PA_ADD_OPTION(RNG_TID);
    PA_ADD_OPTION(RNG_SID);
    PA_ADD_OPTION(SID_HELPER_ADVANCES);
    PA_ADD_OPTION(SID_HELPER_CANDIDATES);
    PA_ADD_OPTION(RNG_SHINY_REQUIRED);
    PA_ADD_OPTION(RNG_NATURES);
    PA_ADD_OPTION(RNG_MIN_IVS);
    PA_ADD_OPTION(RNG_MAX_ATTEMPTS);
    PA_ADD_OPTION(NOTIFICATIONS);
}


//  The save loaded must be the one checkpoint `first` starts from: one left
//  by an earlier checkpoint (a run that failed before saving) would start
//  it in the wrong place (console B, 2026-11-01: 00.09 on the forest's
//  save). Checked on the SAVE card; a card that can't be read is let pass.
static void check_start_save(
    SingleSwitchProgramEnvironment& env, StoryEnv& story, const StoryCheckpoint& checkpoint
){
    if (!checkpoint.start){
        return;
    }
    const std::string error = check_save_place(story, checkpoint.start->map);
    if (!error.empty()){
        throw UserSetupError(env.console, error + " Start at the checkpoint " + checkpoint.id + " the save is for.");
    }
}

namespace{

//  Before the new game starts: a name the keyboard can't type is the user's to fix.
void check_player_name(ConsoleHandle& console, const std::string& name){
    if (name.empty()){
        return;
    }
    const std::string error = naming_text_error(name, NamingScreenKind::PLAYER);
    if (!error.empty()){
        throw UserSetupError(console, "Player name: " + error);
    }
}

//  A failed attempt reloads the last save, with the supplies it had; the
//  engine's lines go to the console's log.
Pokemon::SegmentHooks story_hooks(SingleSwitchProgramEnvironment& env, StoryEnv& story){
    Pokemon::SegmentHooks hooks;
    hooks.reload = [&story](Pokemon::RunState&){
        if (!story.has_save){
            return Pokemon::outcome_failed("no_save", "nothing saved yet");
        }
        //  The save reloaded must be where the checkpoint starts: a save
        //  that didn't happen leaves an older one (console B, 2026-11-02:
        //  00.02 walked the bedroom's route in Pewter City).
        try{
            soft_reset(story.console, story.context);
            const std::string error = check_save_place(story, story.start_map);
            if (!error.empty()){
                return Pokemon::outcome_failed("wrong_save", error);
            }
        }catch (OperationFailedException& e){
            return Pokemon::outcome_failed("reload_failed", e.message());
        }
        story.supplies = story.saved_supplies;
        return Pokemon::outcome_ok();
    };
    hooks.log = [&env](const std::string& line){
        env.console.log(line);
    };
    //  The checkpoint and the step under way, for whoever watches the stream.
    hooks.status = [&env](const std::string& line){
        env.console.overlay().add_log(line);
    };
    return hooks;
}


}

//  The options' problems, before anything is pressed.
static void check_setup(
    ConsoleHandle& console, const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last,
    bool new_game_from_title, const std::string& player_name
){
    if (first > last || last >= checkpoints.size()){
        throw UserSetupError(console, "The start checkpoint must not come after the end checkpoint.");
    }
    if (first == 0 && !new_game_from_title){
        throw UserSetupError(console, "Checkpoint 00.01 starts a new game: start on the title screen with no save.");
    }
    if (first == 0){
        check_player_name(console, player_name);
    }
}

static void throw_if_failed(const Pokemon::SegmentReport& report){
    if (report.completed){
        return;
    }
    throw OperationFailedException(
        ErrorReportMode::NO_ERROR_REPORT,
        std::format(
            "AutoStory stopped at {} {}: {}",
            report.failed_checkpoint, report.failed_event, Pokemon::to_string(report.failure)
        )
    );
}

//  Gets the game to where checkpoint `first` starts: a new game from the
//  title screen, the game as another program left it, or the checkpoint's
//  save loaded.
static void prepare_start(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, StoryEnv& story
){
    const StoryCheckpoint& checkpoint = checkpoints[first];
    if (!checkpoint.start){
        return;
    }
    if (checkpoint.live_start){
        env.console.log(checkpoint.id + " starts from the game as it is: no save to load.");
        return;
    }
    env.console.log("Loading the save for " + checkpoint.id + ".");
    soft_reset(env.console, context);
    story.has_save = true;
    check_start_save(env, story, checkpoints[first]);
}

//  The run stopped for an RNG program, as its option asked: the user's
//  turn, with what to do next, and the lines that go with it (the RNG
//  plan's attempts).
static void stop_for_handoff(
    SingleSwitchProgramEnvironment& env, EventNotificationOption& notification,
    const std::string& message, const std::vector<std::string>& lines = {}
){
    env.console.log(message, COLOR_BLUE);
    env.console.overlay().add_log(message);
    std::string full = message;
    for (const std::string& line : lines){
        env.console.log(line, COLOR_BLUE);
        env.console.overlay().add_log(line);
        full += "\n" + line;
    }
    send_program_finished_notification(env, notification, full);
}

std::vector<RngAttempt> AutoStory::plan_starter_attempts(ConsoleHandle& console) const{
    std::vector<SidCandidate> candidates;
    if (std::string(RNG_SID).empty()){
        candidates = sid_helper_candidates(RNG_TID, SID_HELPER_ADVANCES, SID_HELPER_CANDIDATES);
    }else{
        const std::optional<uint16_t> sid = parse_sid(RNG_SID);
        if (!sid){
            throw UserSetupError(console, "RNG plan: the SID must be a number from 0 to 65535, or empty.");
        }
        candidates.push_back({*sid, 1});
    }
    const std::optional<std::vector<Pokemon::AdvNature>> natures = parse_natures(RNG_NATURES);
    if (!natures){
        throw UserSetupError(console, "RNG plan: natures must be nature names separated by commas, e.g. \"Modest, Timid\".");
    }
    const std::optional<Pokemon::AdvIVs> min_ivs = parse_ivs(RNG_MIN_IVS);
    if (!min_ivs){
        throw UserSetupError(console, "RNG plan: minimum IVs must be six numbers from 0 to 31, e.g. \"0/0/0/31/0/31\".");
    }

    RngTargetFilters filters;
    filters.natures = *natures;
    filters.min_ivs = *min_ivs;
    filters.shiny_required = RNG_SHINY_REQUIRED;
    const std::vector<RngAttempt> attempts = plan_starter_rng(StarterRngPlanInputs{
        //  AutoStory plays FireRed only.
        .firered = true,
        .language = RUN.language(),
        .sound = RNG_SOUND,
        .tid = RNG_TID,
        .candidates = candidates,
        .filters = filters,
        .max_attempts = RNG_MAX_ATTEMPTS,
    });
    if (attempts.empty()){
        throw UserSetupError(console,
            "RNG plan: no Starter RNG target passes the natures, minimum IVs and shiny options for TID "
            + std::to_string(RNG_TID) + "."
        );
    }
    return attempts;
}

void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const std::vector<StoryCheckpoint>& checkpoints = story_checkpoints();
    const size_t first = RUN.start_index();
    const size_t last = RUN.end_index();
    const std::string player_name = PLAYER_NAME;
    check_setup(env.console, checkpoints, first, last, NEW_GAME_FROM_TITLE, player_name);
    const StoryRunPlan plan = plan_story_run(
        checkpoints, first, last, StoryHandoffs{.sid_helper = STOP_FOR_SID_HELPER, .starter_rng = STOP_FOR_STARTER_RNG}
    );
    if (plan.stop == StoryHandoff::SID_HELPER){
        //  SID Helper's steps (set_sid_from_name_screen() to read_tid() in
        //  PokemonFRLG_SidHelper.cpp) could run here instead of stopping;
        //  they are file-static there for now.
        play_new_game_to_player_name(env.console, context, player_name);
        stop_for_handoff(env, NOTIFICATION_PROGRAM_FINISH,
            "Stopped for SID Helper: the player's name is typed and OK is not pressed. "
            "Run SID Helper now. Enter the TID it reads, its Target Advances and its # Candidate SIDs "
            "in AutoStory's RNG plan options, then start AutoStory at 00.01b."
        );
        return;
    }

    //  The Starter RNG targets, worked out before anything is pressed so a
    //  setup problem shows now, not after 00.04.
    std::vector<std::string> rng_lines;
    if (plan.stop == StoryHandoff::STARTER_RNG){
        rng_lines = rng_attempt_lines(plan_starter_attempts(env.console));
        for (const std::string& line : rng_lines){
            env.console.log("Starter RNG plan: " + line);
        }
    }

    StoryEnv story{env.console, context, checkpoints[first].supplies, checkpoints[first].supplies, false, RUN.checks()};
    story.player_name = player_name;
    prepare_start(env, context, checkpoints, first, story);

    StoryExecutor executor(story);
    executor.set_checks(RUN.checks());
    executor.set_language(RUN.language());
    Pokemon::Segment segment = make_story_segment(plan.rows, executor);
    Pokemon::SegmentHooks hooks = story_hooks(env, story);

    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_segment(
        state, segment, Pokemon::SegmentRange{plan.rows.front().id, plan.rows.back().id}, RUN.policy(), hooks
    );
    Pokemon::record_story_stats(env.current_stats<Pokemon::AutoStoryStats>(), report);
    env.update_stats();
    throw_if_failed(report);
    if (plan.stop == StoryHandoff::STARTER_RNG){
        //  Starter RNG runs its own reset and calibration loop in its
        //  program(); the attempts are the user's to run until that loop can
        //  be called from here.
        stop_for_handoff(env, NOTIFICATION_PROGRAM_FINISH,
            "Stopped for Starter RNG: saved south of the table, facing Squirtle's ball. "
            "Run Starter RNG (Squirtle, FireRed, Sound " + std::string(RNG_SOUND == SoundSetting::Stereo ? "Stereo" : "Mono") + ") "
            "with attempt 1's Target Seed and Advances. If it hits the target and it isn't shiny, load this save "
            "and run the next attempt. If it is shiny, its SID is the one listed (or one of them): enter it as the "
            "RNG plan's SID. Then save right after the rival's battle, without moving, and start AutoStory at 00.05.",
            rng_lines
        );
        return;
    }
    env.console.log("AutoStory: done through " + plan.rows.back().id + ".");
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}

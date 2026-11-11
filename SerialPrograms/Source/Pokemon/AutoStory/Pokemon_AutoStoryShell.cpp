/*  AutoStory Program Shell
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_AutoStoryShell.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

void log_line(const std::string& line, StoryGameHooks& game){
    if (game.log){
        game.log(line);
    }
}

std::string story_stop_message(const SegmentReport& report){
    if (report.completed){
        return "";
    }
    return std::format(
        "AutoStory stopped at {} {}: {}",
        report.failed_checkpoint, report.failed_event, to_string(report.failure)
    );
}

std::string story_verify_summary(std::string_view checkpoint, const VerifyPolicy& policy, const VerifyReport& report){
    return std::format(
        "AutoStory verify: {} {}/{} passed, {} needed.", checkpoint, report.passes, policy.runs, policy.passes_needed
    );
}

Outcome load_story_save(std::string_view reason, StoryGameHooks& game, RunState& state){
    log_line(std::format("[Story] {}: loading the save", reason), game);
    Outcome outcome = game.load_save(state);
    if (outcome.succeeded() && game.check_save_place){
        const std::string error = game.check_save_place();
        if (!error.empty()){
            outcome = outcome_failed("wrong_save", error);
        }
    }
    if (outcome.succeeded() && game.after_load){
        outcome = game.after_load(state);
    }
    if (!outcome.succeeded()){
        log_line(std::format("[Story] {}", to_string(outcome)), game);
    }
    return outcome;
}

} // namespace

Outcome prepare_story_start(std::string_view checkpoint, StoryStart start, StoryGameHooks& game, RunState& state){
    switch (start){
    case StoryStart::NEW_GAME:
        log_line(std::format("[Story] {}: starts a new game, no save to load", checkpoint), game);
        return outcome_ok();
    case StoryStart::LIVE:
        log_line(std::format("[Story] {}: starts from the game as it is, no save to load", checkpoint), game);
        return outcome_ok();
    case StoryStart::FROM_SAVE:
        break;
    }
    Outcome outcome = load_story_save(checkpoint, game, state);
    if (outcome.succeeded()){
        state.flags.insert("save_loaded");
    }
    return outcome;
}

SegmentHooks story_segment_hooks(StoryGameHooks& game){
    SegmentHooks hooks;
    hooks.reload = [&game](RunState& state){
        return load_story_save("retry", game, state);
    };
    hooks.save = [&game](RunState& state){
        if (!game.save){
            return outcome_failed("no_save_hook", "this game's rows save at their start");
        }
        return game.save(state);
    };
    hooks.log = [&game](const std::string& line){
        log_line(line, game);
    };
    hooks.status = [&game](const std::string& line){
        if (game.status){
            game.status(line);
        }
    };
    return hooks;
}

void log_story_failure(StoryGameHooks& game){
    log_line(std::string(STORY_LEFT_WHERE_FAILED), game);
}

std::string finish_story_run(const SegmentReport& report, std::string_view last, AutoStoryStats& stats, StoryGameHooks& game){
    record_story_counts(report.story_counts(), stats);
    if (!report.completed){
        log_story_failure(game);
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, story_stop_message(report));
    }
    std::string done = std::format("AutoStory: done through {}.", last);
    log_line(done, game);
    return done;
}

std::string finish_story_verify(
    std::string_view checkpoint, const VerifyPolicy& policy, const VerifyReport& report, StoryGameHooks& game
){
    std::string summary = story_verify_summary(checkpoint, policy, report);
    if (!report.verified){
        log_story_failure(game);
        throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, summary);
    }
    log_line(summary, game);
    return summary;
}



} // namespace Pokemon
} // namespace PokemonAutomation

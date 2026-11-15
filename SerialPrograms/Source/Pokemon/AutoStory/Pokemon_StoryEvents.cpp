/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include <format>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}

std::string checkpoint_log_line(std::string_view checkpoint, std::string_view text){
    return std::format("[Checkpoint] {}: {}", checkpoint, text);
}


namespace{


//  The text of the exception being handled.
std::string current_exception_text(){
    try{
        throw;
    }catch (const Exception& e){
        return e.message();
    }catch (std::exception& e){
        return e.what();
    }catch (...){
        return "unknown exception";
    }
}

//  Runs event `number` of `checkpoint`; if it succeeded knowing where it
//  left the player, that becomes the run's pose. Logs its start and end,
//  and a throw as its and the checkpoint's failure before it goes on.
Outcome run_event(Checkpoint& checkpoint, size_t number, RunState& state, EventLogger& log){
    Event& event = checkpoint.events[number - 1];
    log(event_log_line(checkpoint.id, number, event.name, "start"));
    Outcome outcome;
    try{
        outcome = event.act(state);
    }catch (...){
        const std::string what = current_exception_text();
        log(event_log_line(checkpoint.id, number, event.name, std::format("failed: {}", what)));
        log(checkpoint_log_line(checkpoint.id, std::format("failed at {} {}: {}", number, event.name, what)));
        throw;
    }
    if (!outcome.succeeded()){
        log(event_log_line(checkpoint.id, number, event.name, std::format("failed: {}", to_string(outcome))));
        return outcome;
    }
    if (outcome.pose){
        state.pose = *outcome.pose;
    }
    log(event_log_line(checkpoint.id, number, event.name, "done"));
    return outcome;
}


} // namespace


CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log){
    CheckpointReport report;
    const std::string& id = checkpoint.id;
    log(checkpoint_start_line(id, checkpoint.events.size()));
    for (size_t c = 0; c < checkpoint.events.size(); c++){
        report.events_run++;
        Outcome outcome = run_event(checkpoint, c + 1, state, log);
        if (!outcome.succeeded()){
            report.failed_event = c + 1;
            report.failed_name = checkpoint.events[c].name;
            log(checkpoint_log_line(id, std::format("failed at {} {}: {}", c + 1, report.failed_name, to_string(outcome))));
            report.failure = std::move(outcome);
            return report;
        }
    }
    report.completed = true;
    log(checkpoint_log_line(id, std::format("done, {} run", report.events_run)));
    return report;
}



} // namespace Pokemon
} // namespace PokemonAutomation

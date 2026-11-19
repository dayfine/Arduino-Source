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

//  Runs event `number` (from 1) of `checkpoint`; a throw is logged and
//  passes through.
Outcome run_event(size_t number, Checkpoint& checkpoint, RunState& state, EventLogger& log){
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


CheckpointRunResult run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log){
    const std::string& id = checkpoint.id;
    log(checkpoint_start_line(id, checkpoint.events.size()));
    for (size_t c = 0; c < checkpoint.events.size(); c++){
        Outcome outcome = run_event(c + 1, checkpoint, state, log);
        if (!outcome.succeeded()){
            log(checkpoint_log_line(id, std::format("failed at {} {}: {}", c + 1, checkpoint.events[c].name, to_string(outcome))));
            return CheckpointRunResult{.failure = CheckpointFailure{.event_index = c, .outcome = std::move(outcome)}};
        }
    }
    log(checkpoint_log_line(id, std::format("done, {} run", checkpoint.events.size())));
    return CheckpointRunResult{};
}



} // namespace Pokemon
} // namespace PokemonAutomation

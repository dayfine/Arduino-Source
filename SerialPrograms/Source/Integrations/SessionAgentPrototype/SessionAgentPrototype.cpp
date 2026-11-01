/*  Program Session Agent - Live Prototype Adapter
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <exception>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Globals.h"
#include "Integrations/ProgramTracker.h"
#include "SessionAgentPrototype.h"

namespace PokemonAutomation{
namespace SessionAgentPrototype{
namespace{

const char* state_name(ProgramState state){
    switch (state){
    case ProgramState::NOT_READY:
        return "NOT_READY";
    case ProgramState::STOPPED:
        return "STOPPED";
    case ProgramState::RUNNING:
        return "RUNNING";
    case ProgramState::STOPPING:
        return "STOPPING";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown program state.");
}

Json list_live_sessions(){
    Json sessions = Json::array();
    for (const auto& [id, state] : ProgramTracker::instance().all_programs()){
        sessions.push_back({
            {"session_id", id}, {"program", state.program_name},
            {"state", state_name(state.state)}, {"stats", state.stats},
            {"console_ids", state.console_ids}
        });
    }
    return sessions;
}

}

Json handle_live_request(const std::string& text){
    Bindings bindings{
        .list = list_live_sessions,
        .start = [](uint64_t id){ return ProgramTracker::instance().start_program(id); },
        .stop = [](uint64_t id){ return ProgramTracker::instance().stop_program(id); },
    };
    try{
        const Json request = Json::parse(text);
        return dispatch(bindings, request);
    }catch (const Json::exception& e){
        return failure("invalid_request", e.what());
    }catch (Exception& e){
        return failure("operation_failed", e.message());
    }catch (const std::exception& e){
        return failure("operation_failed", e.what());
    }
}

}
}

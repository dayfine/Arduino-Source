/*  Program Session Agent - Throwaway Prototype
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Four JSON requests over existing live sessions. No transport or UI here.
 *  See README.md for the experiment and its limits.
 */

#ifndef PokemonAutomation_SessionAgentPrototype_H
#define PokemonAutomation_SessionAgentPrototype_H

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include "3rdParty-Core/nlohmann/json.hpp"

namespace PokemonAutomation{
namespace SessionAgentPrototype{

using Json = nlohmann::json;

struct Bindings{
    std::function<Json()> list;
    std::function<std::string(uint64_t)> start;
    std::function<std::string(uint64_t)> stop;
};

inline Json failure(const std::string& code, const std::string& message){
    return {{"ok", false}, {"error", {{"code", code}, {"message", message}}}};
}

inline Json status(Bindings& bindings, uint64_t id){
    for (const Json& session : bindings.list()){
        if (session.at("session_id").get<uint64_t>() == id){
            return {{"ok", true}, {"result", session}};
        }
    }
    return failure("session_not_found", "The session is no longer registered.");
}

//  Returns an immediate reply; accepted start/stop is not run completion.
//  Bindings own session lookup/lifetime protection. No session pointer escapes.
inline Json dispatch(Bindings& bindings, const Json& request){
    if (!request.is_object() || !request.contains("method") || !request["method"].is_string()){
        return failure("invalid_request", "Expected an object with a string method.");
    }
    const std::string method = request["method"].get<std::string>();
    if (method == "sessions.list"){
        return {{"ok", true}, {"result", bindings.list()}};
    }
    if (method != "sessions.status" && method != "sessions.start" && method != "sessions.stop"){
        return failure("unknown_method", "Use sessions.list, sessions.status, sessions.start or sessions.stop.");
    }
    if (!request.contains("session_id") || !request["session_id"].is_number_integer()){
        return failure("invalid_session_id", "Expected a positive integer session_id.");
    }
    if (!request["session_id"].is_number_unsigned() && request["session_id"].get<int64_t>() < 0){
        return failure("invalid_session_id", "Expected a positive integer session_id.");
    }
    const uint64_t id = request["session_id"].get<uint64_t>();
    //  ProgramTracker reserves UINT64_MAX for implicit selection. Never use it.
    if (id == 0 || id == std::numeric_limits<uint64_t>::max()){
        return failure("invalid_session_id", "Explicit live session_id required; implicit selection is disabled.");
    }
    if (method == "sessions.status"){
        return status(bindings, id);
    }
    const std::string error = method == "sessions.start" ? bindings.start(id) : bindings.stop(id);
    if (!error.empty()){
        return failure("operation_rejected", error);
    }
    return {{"ok", true}, {"result", {{"session_id", id}, {"accepted", true}}}};
}

//  Live app adapter. Input is JSON; exceptions become explicit error replies.
Json handle_live_request(const std::string& text);

}
}
#endif

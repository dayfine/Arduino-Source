/*  Program Session Agent - Hardware-Free Terminal Demo
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Simulated lifecycle only. The JSON dispatcher is shared with the live adapter.
 */

#include <iostream>
#include <map>
#include <string>
#include "SessionAgentPrototype.h"

using namespace PokemonAutomation::SessionAgentPrototype;

namespace{

struct DemoSession{
    std::string state = "STOPPED";
    std::string start_error;
};

std::string operate(std::map<uint64_t, DemoSession>& sessions, uint64_t id, const std::string& action){
    auto it = sessions.find(id);
    if (it == sessions.end()){
        return "Session ID not found.";
    }
    DemoSession& session = it->second;
    if (action == "start"){
        if (session.state != "STOPPED"){
            return "Program is " + session.state + ".";
        }
        if (!session.start_error.empty()){
            return session.start_error;
        }
        session.state = "RUNNING";
    }else{
        if (session.state != "RUNNING"){
            return "Program is " + session.state + ".";
        }
        session.state = "STOPPING";
    }
    return "";
}

Json frame(const std::map<uint64_t, DemoSession>& sessions){
    Json result = Json::array();
    for (const auto& [id, session] : sessions){
        result.push_back({
            {"session_id", id}, {"program", "Prototype:Session"},
            {"state", session.state}, {"start_error", session.start_error},
            {"console_ids", Json::array()}, {"stats", ""}
        });
    }
    return result;
}

Json demo_action(std::map<uint64_t, DemoSession>& sessions, Bindings& bindings, const std::string& line){
    if (line == "finish"){
        for (auto& [id, session] : sessions){
            if (session.state == "STOPPING"){
                session.state = "STOPPED";
            }
        }
        return {{"demo", "Stop completion simulated."}};
    }
    if (line == "unload"){
        sessions.erase(1);
        return {{"demo", "Session 1 unloaded; its ID is now stale."}};
    }
    try{
        return dispatch(bindings, Json::parse(line));
    }catch (const Json::exception& e){
        return failure("invalid_request", e.what());
    }
}

}

int main(int argc, char** argv){
    const bool plain = argc > 1 && std::string(argv[1]) == "--plain";
    std::map<uint64_t, DemoSession> sessions{
        {1, {.state = "STOPPED", .start_error = ""}},
        {2, {.state = "STOPPED", .start_error = "Example configuration is invalid."}},
    };
    Bindings bindings{
        .list = [&]{ return frame(sessions); },
        .start = [&](uint64_t id){ return operate(sessions, id, "start"); },
        .stop = [&](uint64_t id){ return operate(sessions, id, "stop"); },
    };
    Json reply;
    std::string line;
    while (true){
        if (!plain){
            std::cout << "\033[2J\033[H\033[1mTHROWAWAY PROTOTYPE: simulated sessions, no console\033[0m\n";
        }
        std::cout << "sessions: " << frame(sessions).dump(2) << "\nreply: " << reply.dump(2) << "\n";
        std::cout << "JSON method: sessions.list/status/start/stop; session_id for all except list\n"
            << "demo controls: finish | unload | quit\n" << std::flush;
        if (!std::getline(std::cin, line) || line == "quit"){
            break;
        }
        reply = demo_action(sessions, bindings, line);
    }
}

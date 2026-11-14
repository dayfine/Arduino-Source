/*  Lab State V1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "LabStateV1.h"

namespace PokemonAutomation{
namespace{


std::string panel_kind_name(LabStateV1::PanelKind kind){
    switch (kind){
    case LabStateV1::PanelKind::NONE:
        return "none";
    case LabStateV1::PanelKind::PROGRAM:
        return "program";
    case LabStateV1::PanelKind::SETTINGS:
        return "settings";
    case LabStateV1::PanelKind::OTHER:
        return "other";
    }
    return "other";
}

std::string program_state_name(LabStateV1::ProgramState state){
    switch (state){
    case LabStateV1::ProgramState::NO_PROGRAM:
        return "no_program";
    case LabStateV1::ProgramState::NOT_READY:
        return "not_ready";
    case LabStateV1::ProgramState::STOPPED:
        return "stopped";
    case LabStateV1::ProgramState::RUNNING:
        return "running";
    case LabStateV1::ProgramState::STOPPING:
        return "stopping";
    }
    return "not_ready";
}

std::string action_source_name(LabStateV1::ActionSource source){
    switch (source){
    case LabStateV1::ActionSource::NONE:
        return "none";
    case LabStateV1::ActionSource::UI:
        return "ui";
    case LabStateV1::ActionSource::LAB:
        return "lab";
    case LabStateV1::ActionSource::PROGRAM:
        return "program";
    case LabStateV1::ActionSource::ERROR:
        return "error";
    }
    return "none";
}

std::string video_state_name(LabStateV1::VideoState state){
    switch (state){
    case LabStateV1::VideoState::MISSING:
        return "missing";
    case LabStateV1::VideoState::STALE:
        return "stale";
    case LabStateV1::VideoState::LIVE:
        return "live";
    }
    return "missing";
}

std::string signal_state_name(LabStateV1::SignalState state){
    switch (state){
    case LabStateV1::SignalState::UNKNOWN:
        return "unknown";
    case LabStateV1::SignalState::OK:
        return "ok";
    case LabStateV1::SignalState::BLACK:
        return "black";
    case LabStateV1::SignalState::COLOR_BARS:
        return "colour_bars";
    }
    return "unknown";
}

std::string controller_state_name(LabStateV1::ControllerState state){
    switch (state){
    case LabStateV1::ControllerState::NOT_APPLICABLE:
        return "not_applicable";
    case LabStateV1::ControllerState::NONE:
        return "none";
    case LabStateV1::ControllerState::READY:
        return "ready";
    }
    return "none";
}

JsonValue optional_string(const std::optional<std::string>& value){
    return value ? JsonValue(*value) : JsonValue();
}

JsonObject make_program(const LabStateV1& state){
    JsonObject transition;
    transition["action"] = optional_string(state.transition_action);
    transition["at"] = optional_string(state.transition_at);
    transition["request_id"] = optional_string(state.transition_request_id);
    transition["source"] = action_source_name(state.transition_source);

    JsonObject program;
    program["last_transition"] = state.transition_source == LabStateV1::ActionSource::NONE
        ? JsonValue()
        : JsonValue(std::move(transition));
    program["run_id"] = optional_string(state.run_id);
    program["state"] = program_state_name(state.program_state);
    return program;
}

JsonObject make_video(const LabStateV1& state){
    JsonObject video;
    video["captured_at"] = optional_string(state.video_captured_at);
    video["frame_seq"] = state.video_frame_seq
        ? JsonValue(*state.video_frame_seq)
        : JsonValue();
    video["signal"] = signal_state_name(state.video_signal);
    video["state"] = video_state_name(state.video_state);
    return video;
}

JsonObject make_unknown_observation(){
    JsonObject game;
    game["observed_at"] = JsonValue();
    game["value"] = "unknown";
    JsonObject screen;
    screen["observed_at"] = JsonValue();
    screen["value"] = "unknown";

    JsonObject observation;
    observation["game"] = std::move(game);
    observation["screen"] = std::move(screen);
    return observation;
}


} // namespace

std::string lab_state_json_text(const LabStateV1& state){
    JsonObject instance;
    instance["id"] = state.instance_id;
    instance["pid"] = state.process_id;

    JsonObject panel;
    panel["id"] = state.panel_id;
    panel["kind"] = panel_kind_name(state.panel_kind);

    JsonObject controller;
    controller["state"] = controller_state_name(state.controller_state);

    JsonObject root;
    root["controller"] = std::move(controller);
    root["instance"] = std::move(instance);
    root["observation"] = make_unknown_observation();
    root["panel"] = std::move(panel);
    root["program"] = make_program(state);
    root["schema"] = 1;
    root["video"] = make_video(state);
    root["written_at"] = state.written_at;
    return root.dump(4);
}


} // namespace PokemonAutomation

/*  Lab State V1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The versioned JSON state published beside the legacy Lab/state file.
 *  Fields the app cannot observe remain unknown/null; readers must not infer
 *  success from their absence.
 *
 */

#ifndef PokemonAutomation_LabStateV1_H
#define PokemonAutomation_LabStateV1_H

#include <cstdint>
#include <optional>
#include <string>

namespace PokemonAutomation{


//  App-authored values in schema 1 of Lab/state.json.
struct LabStateV1{
    //  The kind of panel selected in the main window.
    enum class PanelKind{
        NONE,
        PROGRAM,
        SETTINGS,
        OTHER,
    };

    //  State of the selected program, including the absence of one.
    enum class ProgramState{
        NO_PROGRAM,
        NOT_READY,
        STOPPED,
        RUNNING,
        STOPPING,
    };

    //  Where a program state transition originated.
    enum class ActionSource{
        NONE,
        UI,
        LAB,
        PROGRAM,
        ERROR,
    };

    //  Whether the app is receiving frames from the video transport.
    enum class VideoState{
        MISSING,
        STALE,
        LIVE,
    };

    //  Visual signal classification, independent of transport freshness.
    enum class SignalState{
        UNKNOWN,
        OK,
        BLACK,
        COLOR_BARS,
    };

    //  Whether the selected panel has a controller ready for commands.
    enum class ControllerState{
        NOT_APPLICABLE,
        NONE,
        READY,
    };

    std::string written_at;
    std::string instance_id;
    uint64_t process_id = 0;

    std::string panel_id;
    PanelKind panel_kind = PanelKind::NONE;
    ProgramState program_state = ProgramState::NO_PROGRAM;
    std::optional<std::string> run_id;

    std::optional<std::string> transition_action;
    ActionSource transition_source = ActionSource::NONE;
    std::optional<std::string> transition_request_id;
    std::optional<std::string> transition_at;

    ControllerState controller_state = ControllerState::NOT_APPLICABLE;
    VideoState video_state = VideoState::MISSING;
    std::optional<uint64_t> video_frame_seq;
    std::optional<std::string> video_captured_at;
    SignalState video_signal = SignalState::UNKNOWN;
};

//  Serialize one complete schema-1 state object.
std::string lab_state_json_text(const LabStateV1& state);


} // namespace PokemonAutomation
#endif

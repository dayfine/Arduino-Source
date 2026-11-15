/*  Lab State JSON
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "Lab/state.json", version 1: the lab channel's state for the lab server,
 *  written beside the legacy "Lab/state" from the same LabState. The format
 *  is fixed by the server's contract (lab/server/contracts/v1/app_state on
 *  the fork's agents branch): keys in contract order, 2-space indent, "\n"
 *  at the end, UTC timestamps with milliseconds.
 *
 */

#ifndef PokemonAutomation_LabStateJson_H
#define PokemonAutomation_LabStateJson_H

#include <cstdint>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "LabRequest.h"
#include "LabTransition.h"

namespace PokemonAutomation{


//  "2026-11-14T06:10:12.124Z".
std::string lab_timestamp_text(WallClock time);


//  The newest frame the app received from the console's video.
struct LabFrameToken{
    uint64_t seq;           //  Counts frames since the app started.
    WallClock captured_at;
};

//  Whether frames arrive: transport freshness, not what they show.
enum class LabVideoState{
    LIVE,
    STALE,
    MISSING,
};

//  LIVE when the newest frame is at most `max_age` old at `now`.
LabVideoState lab_video_state(bool has_video, const std::optional<LabFrameToken>& frame, WallClock now, Milliseconds max_age);


//  What a detector says about the screen. Without one, the value is "unknown".
struct LabObservation{
    std::string value = "unknown";
    std::optional<WallClock> observed_at;
    std::optional<std::string> detector;
    std::optional<double> confidence;
};


//  Everything "Lab/state.json" says.
struct LabAppState{
    LabState legacy;    //  Also written to "Lab/state".
    WallClock written_at;

    std::string instance_id;
    uint64_t pid = 0;
    std::optional<std::string> profile;

    std::optional<LabTransition> last_transition;
    std::optional<std::string> paused;

    LabVideoState video = LabVideoState::MISSING;
    std::optional<LabFrameToken> frame;

    LabObservation game;
    LabObservation screen;
};

//  The text of "Lab/state.json".
std::string lab_state_json_text(const LabAppState& state);



}  // namespace PokemonAutomation
#endif

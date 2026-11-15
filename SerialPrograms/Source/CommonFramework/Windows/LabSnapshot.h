/*  Lab Snapshot
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "snapshot LABEL [TIMEOUT_MS]": the lab channel's read-only command that
 *  waits for a video frame newer than the call (LabFrames.h), saves it, and
 *  names that frame's sequence number and the app instance. Unlike the
 *  console's "shot", it needs no program state and no controller: only video.
 *
 *  The argument parsing and the journal lines live here, free of Qt and of
 *  the app, so they are unit tested. LabChannel saves the frame.
 *
 */

#ifndef PokemonAutomation_LabSnapshot_H
#define PokemonAutomation_LabSnapshot_H

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "LabStateJson.h"

namespace PokemonAutomation{


//  The arguments of a "snapshot" line.
struct LabSnapshotArgs{
    static constexpr Milliseconds TIMEOUT_DEFAULT = Milliseconds(5000);
    static constexpr Milliseconds TIMEOUT_MIN = Milliseconds(100);
    static constexpr Milliseconds TIMEOUT_MAX = Milliseconds(60000);

    std::string label;      //  The file is "<request>_<label>.png" in the outbox.
    Milliseconds timeout = TIMEOUT_DEFAULT;
};

//  The words of a "snapshot LABEL [TIMEOUT_MS]" line, or the error for the
//  journal. A label is 1 to 64 of [A-Za-z0-9_-], so it stays a file name.
std::expected<LabSnapshotArgs, std::string> parse_lab_snapshot(const std::vector<std::string>& words);


//  Why a snapshot has no frame to save, besides a timeout.
enum class LabSnapshotUnavailable{
    NO_VIDEO,           //  The selected panel has no video (no panel, or not a console program).
    PANEL_CHANGED,      //  The selected panel changed during the wait.
    APP_CLOSING,
    SAVE_FAILED,        //  The frame could not be converted or written.
};

//  What a snapshot prints into the journal, and the line's error ("" on success).
struct LabSnapshotReply{
    std::string line;
    std::string error;
};

//  "snapshot saved instance=<uuid> frame_seq=<n> captured_at=<time> after=<n> path=<path>".
//  The path is last: it runs to the end of the line, spaces included.
LabSnapshotReply lab_snapshot_saved(
    std::string_view instance_id, const LabFrameToken& frame, uint64_t after, std::string_view path
);

//  "snapshot timeout instance=<uuid> after=<n> timeout_ms=<ms>"; the line fails.
LabSnapshotReply lab_snapshot_timeout(std::string_view instance_id, uint64_t after, Milliseconds timeout);

//  "snapshot unavailable instance=<uuid> reason=<reason>"; the line fails.
LabSnapshotReply lab_snapshot_unavailable(std::string_view instance_id, LabSnapshotUnavailable reason);



}  // namespace PokemonAutomation
#endif

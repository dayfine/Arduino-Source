/*  Lab Snapshot
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include "LabSnapshot.h"

namespace PokemonAutomation{


namespace{

constexpr size_t LABEL_MAX = 64;

bool valid_label(std::string_view label){
    if (label.empty() || label.size() > LABEL_MAX){
        return false;
    }
    return std::all_of(label.begin(), label.end(), [](char c){
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

std::optional<Milliseconds> parse_timeout(std::string_view word){
    int64_t value = 0;
    const std::from_chars_result result = std::from_chars(word.data(), word.data() + word.size(), value);
    if (result.ec != std::errc() || result.ptr != word.data() + word.size()){
        return std::nullopt;
    }
    const Milliseconds timeout(value);
    if (timeout < LabSnapshotArgs::TIMEOUT_MIN || timeout > LabSnapshotArgs::TIMEOUT_MAX){
        return std::nullopt;
    }
    return timeout;
}

}  // namespace



std::expected<LabSnapshotArgs, std::string> parse_lab_snapshot(const std::vector<std::string>& words){
    if (words.size() < 2 || words.size() > 3){
        return std::unexpected("snapshot: expected snapshot LABEL [TIMEOUT_MS]");
    }
    LabSnapshotArgs args;
    args.label = words[1];
    if (!valid_label(args.label)){
        return std::unexpected(std::format("snapshot: LABEL must be 1 to 64 of A-Z a-z 0-9 _ -, got {}", args.label));
    }
    if (words.size() == 3){
        const std::optional<Milliseconds> timeout = parse_timeout(words[2]);
        if (!timeout){
            return std::unexpected(std::format(
                "snapshot: TIMEOUT_MS must be a whole number from {} to {}, got {}",
                LabSnapshotArgs::TIMEOUT_MIN.count(), LabSnapshotArgs::TIMEOUT_MAX.count(), words[2]
            ));
        }
        args.timeout = *timeout;
    }
    return args;
}



namespace{

std::string_view reason_name(LabSnapshotUnavailable reason){
    switch (reason){
    case LabSnapshotUnavailable::NO_VIDEO:
        return "no_video";
    case LabSnapshotUnavailable::PANEL_CHANGED:
        return "panel_changed";
    case LabSnapshotUnavailable::APP_CLOSING:
        return "app_closing";
    case LabSnapshotUnavailable::SAVE_FAILED:
        return "save_failed";
    }
    return "unknown";
}

}  // namespace


LabSnapshotReply lab_snapshot_saved(
    std::string_view instance_id, const LabFrameToken& frame, uint64_t after, std::string_view path
){
    return {
        std::format(
            "snapshot saved instance={} frame_seq={} captured_at={} after={} path={}\n",
            instance_id, frame.seq, lab_timestamp_text(frame.captured_at), after, path
        ),
        "",
    };
}
LabSnapshotReply lab_snapshot_timeout(std::string_view instance_id, uint64_t after, Milliseconds timeout){
    return {
        std::format("snapshot timeout instance={} after={} timeout_ms={}\n", instance_id, after, timeout.count()),
        std::format("timeout: no frame newer than frame_seq {} within {} ms", after, timeout.count()),
    };
}
LabSnapshotReply lab_snapshot_unavailable(std::string_view instance_id, LabSnapshotUnavailable reason){
    std::string_view why;
    switch (reason){
    case LabSnapshotUnavailable::NO_VIDEO:
        why = "the selected panel has no video";
        break;
    case LabSnapshotUnavailable::PANEL_CHANGED:
        why = "the selected panel changed during the wait";
        break;
    case LabSnapshotUnavailable::APP_CLOSING:
        why = "the app is closing";
        break;
    case LabSnapshotUnavailable::SAVE_FAILED:
        why = "the frame could not be saved";
        break;
    }
    return {
        std::format("snapshot unavailable instance={} reason={}\n", instance_id, reason_name(reason)),
        std::format("unavailable: {}", why),
    };
}



}  // namespace PokemonAutomation

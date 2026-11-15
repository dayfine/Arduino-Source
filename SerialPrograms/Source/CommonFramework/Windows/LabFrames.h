/*  Lab Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Counts the frames of the selected console's video for the lab channel:
 *  the "frame_seq" of Lab/state.json, and the frame "snapshot" saves. Free of
 *  Qt (a frame is only passed along), so it is unit tested.
 *
 */

#ifndef PokemonAutomation_LabFrames_H
#define PokemonAutomation_LabFrames_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "LabStateJson.h"

namespace PokemonAutomation{


//  A frame and its place in the app's sequence.
struct LabFrame{
    LabFrameToken token;
    std::shared_ptr<const VideoFrame> frame;
};

//  What LabFrames::wait_for_next_frame() found.
struct LabFrameWait{
    uint64_t after = 0;     //  Frames counted when the wait began: the newest frame_seq, 0 before the first.
    std::optional<LabFrame> frame;  //  The first frame newer than `after`, if one came.
    bool cancelled = false;
};

//  Counts frames per app instance, over every panel, and hands out one newer
//  than a call. Frames are kept only while someone waits, so the camera's
//  buffers are not held between snapshots.
class LabFrames{
public:
    //  Called for every frame, on the video's thread.
    void on_frame(WallClock captured_at, std::shared_ptr<const VideoFrame> frame);

    std::optional<LabFrameToken> latest() const;

    //  Waits until a frame newer than the call arrives or `deadline` passes.
    //  Calls `cancelled` (without the lock) about every `slice`, and stops
    //  waiting when it returns true.
    LabFrameWait wait_for_next_frame(
        WallClock deadline, Milliseconds slice, const std::function<bool()>& cancelled
    );

private:
    mutable Mutex m_lock;
    ConditionVariable m_cv;
    uint64_t m_count = 0;
    std::optional<LabFrameToken> m_latest;
    size_t m_waiters = 0;
    std::shared_ptr<const VideoFrame> m_latest_frame;   //  Only while m_waiters > 0.
};



}  // namespace PokemonAutomation
#endif

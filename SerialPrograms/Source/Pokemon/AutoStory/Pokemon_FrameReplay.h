/*  Frame Replay
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Recorded console frames, replayed through the real detectors on a
 *  virtual clock, so a console failure becomes a deterministic test. The
 *  story sims script what a screen shows (a label per frame); a replay
 *  shows what the console showed and lets the game's own watchers and
 *  ScreenEventObserver judge it.
 *
 *  A recording is a folder of frames named by their time in ms from its
 *  start ("0.png", "1250.png"), as the captured transitions
 *  (Pokemon_Transitions_Tests.cpp). A frame shows until the next one.
 *  Recordings are too big for this repo; tests read them from
 *  CommandLineTests/<game>/FrameReplay/<case>/ and skip when it is missing.
 *
 *  From the stream history (an error report's Video.mp4, kept when the
 *  "Stream History" setting is on), cut the seconds around the failure at
 *  20 frames a second, then rename frame N to its time, (N - 1) * 50 ms:
 *      ffmpeg -ss <start s> -t <seconds> -i Video.mp4 -vf fps=20 %d.png
 *
 *  A failed AutoStory event saves its own (Pokemon_AutoStoryFailureCapture.h)
 *  in USER_FILE_PATH()/<game>/AutoStoryFailures/<time>-<checkpoint>-<event>/,
 *  whatever the error-report settings. Its Video.mp4 ends when the event
 *  failed, so the last seconds are the case; Failure.json names the event
 *  and its last log lines, which say where in those seconds each action
 *  started. For the last 20 s, into the case folder:
 *      ffmpeg -sseof -20 -i Video.mp4 -vf fps=20 %d.png
 *      for f in *.png; do n=${f%.png}; mv "$f" "x$(( (n - 1) * 50 )).png"; done
 *      for f in x*.png; do mv "$f" "${f#x}"; done
 *
 *  One thread, no real time: the caller says which window of the recording
 *  each action watched, and the replay feeds those frames at the inference
 *  period, as run_until / wait_until would.
 *
 */

#ifndef PokemonAutomation_Pokemon_FrameReplay_H
#define PokemonAutomation_Pokemon_FrameReplay_H

#include <memory>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One recorded frame: when it showed, from the recording's start, and the
//  image, or the file it loads from on first use.
struct ReplayFrame{
    Milliseconds at = Milliseconds(0);
    std::string file = {};
    std::shared_ptr<const ImageRGB32> image = {};
};


//  Recorded frames, served by virtual time. Not thread-safe: images load
//  on first use.
class ReplayFrameSource{
public:
    explicit ReplayFrameSource(std::vector<ReplayFrame> frames);

    //  Every "<ms>.png" in `folder`. Throws FileException if it has none.
    static ReplayFrameSource from_folder(const std::string& folder);

    size_t size() const{
        return m_frames.size();
    }
    //  When the last frame showed.
    Milliseconds last_frame() const;

    //  The frame showing at `at`: the last one at or before it, stamped
    //  `origin + at`. None before the first frame.
    VideoSnapshot frame_at(Milliseconds at, WallClock origin) const;

private:
    mutable std::vector<ReplayFrame> m_frames;
};


//  How often a replay looks: the inference loop's default period.
constexpr Milliseconds REPLAY_PERIOD = Milliseconds(50);

//  Part of a recording an action watched, in ms from its start, end
//  excluded: the hold of a leg run_until watched, or a wait_until after it.
struct ReplayWindow{
    Milliseconds start = Milliseconds(0);
    Milliseconds end = Milliseconds(0);
};

//  The first callback that stopped a replay, and when.
struct ReplayHit{
    //  Its index in the callbacks; -1 if none did.
    int index = -1;
    Milliseconds at = Milliseconds(0);
};

//  Feeds `callbacks` the frames of `window`, one look per `period`, each
//  callback in order on each look, until one returns true: wait_until on a
//  recording. Frames are stamped `origin + time`.
ReplayHit replay_until(
    const ReplayFrameSource& source, ReplayWindow window,
    const std::vector<VisualInferenceCallback*>& callbacks,
    WallClock origin, Milliseconds period = REPLAY_PERIOD
);

//  What `observer` sees over `window`, one look per `period`, as a
//  ScreenEventWatcher's log; the judges (Pokemon_Interrupts.h) read it.
ScreenEventLog replay_events(
    const ReplayFrameSource& source, ReplayWindow window,
    ScreenEventObserver& observer,
    WallClock origin, Milliseconds period = REPLAY_PERIOD
);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

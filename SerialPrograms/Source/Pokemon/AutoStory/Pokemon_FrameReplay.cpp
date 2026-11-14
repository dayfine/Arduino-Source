/*  Frame Replay
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Pokemon_FrameReplay.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  The time a frame file's name gives ("1250.png" -> 1250), or -1 when the
//  name is not all digits or the file not a .png.
long long frame_time_of(const Filesystem::Path& file){
    if (file.extension().string() != ".png"){
        return -1;
    }
    const std::string stem = file.stem().string();
    if (stem.empty() || stem.size() > 9){
        return -1;
    }
    for (char c : stem){
        if (!std::isdigit((unsigned char)c)){
            return -1;
        }
    }
    return std::stoll(stem);
}

} // namespace


ReplayFrameSource::ReplayFrameSource(std::vector<ReplayFrame> frames)
    : m_frames(std::move(frames))
{
    std::stable_sort(m_frames.begin(), m_frames.end(), [](const ReplayFrame& a, const ReplayFrame& b){
        return a.at < b.at;
    });
}

ReplayFrameSource ReplayFrameSource::from_folder(const std::string& folder){
    std::vector<ReplayFrame> frames;
    if (Filesystem::is_directory(folder)){
        for (const Filesystem::Path& file : Filesystem::list_directory(folder)){
            const long long ms = frame_time_of(file);
            if (ms >= 0){
                frames.push_back(ReplayFrame{.at = Milliseconds(ms), .file = file.string()});
            }
        }
    }
    if (frames.empty()){
        throw FileException(nullptr, PA_CURRENT_FUNCTION, "No <ms>.png frames to replay.", folder);
    }
    return ReplayFrameSource(std::move(frames));
}

Milliseconds ReplayFrameSource::last_frame() const{
    return m_frames.empty() ? Milliseconds(0) : m_frames.back().at;
}

VideoSnapshot ReplayFrameSource::frame_at(Milliseconds at, WallClock origin) const{
    auto after = std::upper_bound(m_frames.begin(), m_frames.end(), at, [](Milliseconds time, const ReplayFrame& frame){
        return time < frame.at;
    });
    if (after == m_frames.begin()){
        return VideoSnapshot();
    }
    ReplayFrame& frame = *(after - 1);
    if (!frame.image){
        frame.image = std::make_shared<const ImageRGB32>(frame.file);
    }
    VideoSnapshot out;
    out.frame = frame.image;
    out.timestamp = origin + at;
    return out;
}


ReplayHit replay_until(
    const ReplayFrameSource& source, ReplayWindow window,
    const std::vector<VisualInferenceCallback*>& callbacks,
    WallClock origin, Milliseconds period
){
    for (Milliseconds now = window.start; now < window.end; now += period){
        const VideoSnapshot frame = source.frame_at(now, origin);
        if (!frame){
            continue;
        }
        for (size_t c = 0; c < callbacks.size(); c++){
            if (callbacks[c]->process_frame(frame)){
                return ReplayHit{.index = (int)c, .at = now};
            }
        }
    }
    return ReplayHit{};
}

ScreenEventLog replay_events(
    const ReplayFrameSource& source, ReplayWindow window,
    ScreenEventObserver& observer,
    WallClock origin, Milliseconds period
){
    ScreenEventLog log;
    for (Milliseconds now = window.start; now < window.end; now += period){
        const VideoSnapshot frame = source.frame_at(now, origin);
        log.record(observer.observe(frame, origin + now), origin + now);
    }
    return log;
}



} // namespace Pokemon
} // namespace PokemonAutomation

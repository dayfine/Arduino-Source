/*  MgbaLink Keys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include "MgbaLink_Keys.h"

namespace PokemonAutomation{
namespace MgbaLink{


namespace{

//  "duration" in (fractional) GBA frames.
double to_frames(WallDuration duration){
    double microseconds = (double)std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    return microseconds * FrameClock::GBA_FRAMES_PER_SECOND / 1000000.0;
}

uint64_t offset_frame(const FrameClock& clock, double frames){
    double frame = (double)clock.anchor_frame() + frames;
    return frame <= 0 ? 0 : (uint64_t)frame;
}

}  // namespace



uint64_t frames_for_duration(WallDuration duration){
    if (duration <= WallDuration::zero()){
        return 0;
    }
    return std::max<uint64_t>(1, (uint64_t)std::llround(to_frames(duration)));
}

uint64_t frame_at(const FrameClock& clock, WallClock time){
    return offset_frame(clock, std::round(to_frames(time - clock.anchor_time())));
}

uint64_t grant_frame(const FrameClock& clock, WallClock now, uint64_t lead_frames){
    return offset_frame(clock, std::floor(to_frames(now - clock.anchor_time()))) + lead_frames;
}



std::optional<KeysCommand> KeySchedule::push(WallClock now, uint16_t mask, WallDuration duration){
    if (duration <= WallDuration::zero()){
        return std::nullopt;
    }
    uint64_t start_frame = m_end_frame;
    WallClock start_time = m_end_time;
    uint64_t earliest = frame_at(m_clock, now) + LEAD_FRAMES;
    if (start_frame < earliest){
        start_frame = earliest;
        start_time = m_clock.timestamp(start_frame);
    }

    m_end_time = start_time + duration;
    m_end_frame = std::max(start_frame + 1, frame_at(m_clock, m_end_time));

    if (m_held_mask == mask){
        return std::nullopt;
    }
    m_held_mask = mask;
    return KeysCommand{start_frame, mask};
}



void InputLog::record(const KeysCommand& sent, uint64_t frames_late){
    m_entries.emplace_back(Entry{sent.frame + frames_late, sent.mask});
}

std::string InputLog::to_text() const{
    std::string text;
    for (const Entry& entry : m_entries){
        text += std::format("{} {}\n", entry.frame, entry.mask);
    }
    return text;
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation

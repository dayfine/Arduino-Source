/*  Lab Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "LabFrames.h"

namespace PokemonAutomation{


void LabFrames::on_frame(WallClock captured_at, std::shared_ptr<const VideoFrame> frame){
    std::lock_guard<Mutex> lg(m_lock);
    m_count++;
    m_latest = LabFrameToken{m_count, captured_at};
    if (m_waiters > 0){
        m_latest_frame = std::move(frame);
        m_cv.notify_all();
    }
}
std::optional<LabFrameToken> LabFrames::latest() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_latest;
}

LabFrameWait LabFrames::wait_for_next_frame(
    WallClock deadline, Milliseconds slice, const std::function<bool()>& cancelled
){
    std::unique_lock<Mutex> lg(m_lock);
    LabFrameWait wait;
    wait.after = m_count;
    m_waiters++;
    while (true){
        if (m_count > wait.after){
            wait.frame = LabFrame{*m_latest, m_latest_frame};
            break;
        }
        const WallClock now = current_time();
        if (now >= deadline){
            break;
        }
        lg.unlock();
        const bool stop = cancelled();
        lg.lock();
        if (stop){
            wait.cancelled = true;
            break;
        }
        if (m_count > wait.after){
            continue;
        }
        m_cv.wait_for(lg, std::min<WallDuration>(slice, deadline - now));
    }
    m_waiters--;
    if (m_waiters == 0){
        m_latest_frame.reset();
    }
    return wait;
}



}  // namespace PokemonAutomation

/*  MgbaLink Keys
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The input side of MgbaLink v1 (lab/mgba/PROTOCOL.md on the fork's agents
 *  branch): GBA key masks, the conversion of app time to emulated frames,
 *  the frames that "KEYS" and "GRANT" name, and the input log. Pure logic;
 *  the controller that sends the commands holds the connection.
 *
 *  Programs keep milliseconds; this is the boundary that turns them into
 *  frames (design: docs/design/Lab-mGBA-Console.md, section 2).
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Keys_H
#define PokemonAutomation_Controllers_MgbaLink_Keys_H

#include <stdint.h>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "MgbaLink_Protocol.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  GBA KEYINPUT bits, as mGBA's C.GBA_KEY.
enum GbaKey : uint16_t{
    GBA_KEY_NONE    = 0,
    GBA_KEY_A       = 1 << 0,
    GBA_KEY_B       = 1 << 1,
    GBA_KEY_SELECT  = 1 << 2,
    GBA_KEY_START   = 1 << 3,
    GBA_KEY_RIGHT   = 1 << 4,
    GBA_KEY_LEFT    = 1 << 5,
    GBA_KEY_UP      = 1 << 6,
    GBA_KEY_DOWN    = 1 << 7,
    GBA_KEY_R       = 1 << 8,
    GBA_KEY_L       = 1 << 9,
};


//  How many frames a state held for "duration" lasts:
//  max(1, round(duration * 59.7275 / 1000)), so 17 ms is 1 frame and 80 ms
//  is 5. A zero duration holds no frame.
uint64_t frames_for_duration(WallDuration duration);

//  The frame app time "time" lands on: F0 + round((time - t0) * 59.7275 / 1000),
//  never below frame 0.
uint64_t frame_at(const FrameClock& clock, WallClock time);

//  The frame to "GRANT" at "now" in paced mode:
//  F0 + floor((now - t0) * 59.7275 / 1000) + lead_frames.
uint64_t grant_frame(const FrameClock& clock, WallClock now, uint64_t lead_frames);


//  One "KEYS <frame> <mask>" command.
struct KeysCommand{
    uint64_t frame = 0;
    uint16_t mask = 0;

    friend bool operator==(const KeysCommand&, const KeysCommand&) = default;
};


//  Turns the controller's stream of states, each held for a duration, into
//  "KEYS" commands on frames. States follow each other without gaps: a state
//  starts on the frame the previous one ended, unless that frame has already
//  passed (the controller was idle), in which case it starts LEAD_FRAMES
//  after "now". The end of each state is placed from app time, so rounding
//  does not add up over many states, but every state lasts at least one
//  frame. Not thread-safe.
class KeySchedule{
public:
    //  The pacer grants this far ahead, so commands for the next frames
    //  still reach the bridge before it runs them.
    static constexpr uint64_t LEAD_FRAMES = 2;

    explicit KeySchedule(FrameClock clock)
        : m_clock(clock)
        , m_end_time(clock.anchor_time())
        , m_end_frame(clock.anchor_frame())
    {}

    //  Appends "mask" held for "duration". Returns the command to send, or
    //  nullopt if the keys do not change (or "duration" is zero, which holds
    //  nothing and is dropped).
    std::optional<KeysCommand> push(WallClock now, uint16_t mask, WallDuration duration);

    //  The frame the last state ends on: the next state starts there.
    uint64_t end_frame() const{
        return m_end_frame;
    }

private:
    FrameClock m_clock;
    WallClock m_end_time;
    uint64_t m_end_frame;
    std::optional<uint16_t> m_held_mask;
};


//  What landed on which frame: one entry per "KEYS" sent, at the frame the
//  bridge applied it (the frame named, plus how late it was). With the start
//  state this replays the run in mGBA without the app. Not thread-safe.
class InputLog{
public:
    struct Entry{
        uint64_t frame = 0;
        uint16_t mask = 0;

        friend bool operator==(const Entry&, const Entry&) = default;
    };

    void record(const KeysCommand& sent, uint64_t frames_late);

    const std::vector<Entry>& entries() const{
        return m_entries;
    }

    //  One "<frame> <mask>" line per entry, in decimal.
    std::string to_text() const;

private:
    std::vector<Entry> m_entries;
};


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif

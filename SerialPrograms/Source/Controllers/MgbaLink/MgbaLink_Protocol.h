/*  MgbaLink Protocol
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reply parsing, line framing and the frame clock of MgbaLink v1, the text
 *  protocol spoken by "bridge.lua" inside mGBA (lab/mgba/PROTOCOL.md on the
 *  fork's agents branch). Pure functions; the socket lives in
 *  MgbaLink_Connection.
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Protocol_H
#define PokemonAutomation_Controllers_MgbaLink_Protocol_H

#include <stdint.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  The fields of a "HELLO 1" reply.
struct HelloReply{
    std::string game_code;      //  "BPRE" for FireRed US.
    std::string bridge_version; //  "bridge-0.1"
    std::string mode;           //  "paced" or "free"
};

//  The fields of a "FRAME" reply (0.10.5: the frame is a PNG on disk).
struct FrameReply{
    uint64_t frame = 0;
    size_t width = 0;
    size_t height = 0;
    std::string path;           //  PNG written by the bridge; may contain spaces.
};


//  Each parser returns nullopt when the reply does not have the expected
//  shape (including any "ERR ..." reply).

//  "OK 1 <code> <version> <mode>"
std::optional<HelloReply> parse_hello_reply(std::string_view reply);

//  "OK <frame>" (GRANT, SAVE, LOAD)
std::optional<uint64_t> parse_ok_frame_reply(std::string_view reply);

//  KEYS: "OK" -> 0, "LATE <n>" -> n (frames late).
std::optional<uint64_t> parse_keys_reply(std::string_view reply);

//  "FRAME <frame> <w> <h> <path>"
std::optional<FrameReply> parse_frame_reply(std::string_view reply);



//  Splits a byte stream into lines: LF ends a line, a CR right before it is
//  dropped. A line longer than "max_line" bytes sets "overflowed()" and
//  drops everything buffered so far; the caller should then disconnect.
class LineSplitter{
public:
    //  The bridge drops a client whose command line is longer than this;
    //  replies are bounded the same way (a FRAME path is the longest reply).
    static constexpr size_t MAX_LINE_BYTES = 4096;

    explicit LineSplitter(size_t max_line = MAX_LINE_BYTES)
        : m_max_line(max_line)
    {}

    //  Appends "data" and returns the lines it completed, in order.
    std::vector<std::string> push(std::string_view data);

    bool overflowed() const{
        return m_overflowed;
    }

private:
    size_t m_max_line;
    std::string m_buffer;
    bool m_overflowed = false;
};



//  Maps emulated frame numbers to app time. Anchored at a known pair
//  (t0, F0): frame F happened at t0 + (F - F0) * 1000 / 59.7275 ms, so a
//  frame's timestamp comes from its number, not from when the app read it.
class FrameClock{
public:
    //  GBA frame rate: 16.78 MHz / 280,896 cycles per frame.
    static constexpr double GBA_FRAMES_PER_SECOND = 59.7275;

    //  The time a frame is stamped with, and whether the clock had to catch
    //  up with a paused or slow mGBA to get it.
    struct Stamp{
        WallClock time{};
        bool reanchored_after_lag = false;
    };

    FrameClock(WallClock t0, uint64_t f0)
        : m_t0(t0)
        , m_f0(f0)
    {}

    WallClock timestamp(uint64_t frame) const;

    //  The stamp for "frame", read from the bridge at "now". While mGBA runs
    //  free (no GRANT pacing yet), its clock drifts against the app's and
    //  stops while it is paused, so the clock re-anchors at (now, frame)
    //  when "frame" maps after "now" (mGBA running ahead: a frame cannot be
    //  stamped later than it was read) or more than "max_lag" before it
    //  (mGBA paused or slow). Stamps never go backwards: a frame that maps
    //  before the previous stamp (a LOAD) gets the previous stamp.
    Stamp stamp(WallClock now, uint64_t frame, WallDuration max_lag);

    WallClock anchor_time() const{
        return m_t0;
    }
    uint64_t anchor_frame() const{
        return m_f0;
    }

private:
    WallClock m_t0;
    uint64_t m_f0;
    std::optional<WallClock> m_last_stamp;
};


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif

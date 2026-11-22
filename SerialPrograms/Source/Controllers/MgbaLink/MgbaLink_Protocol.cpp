/*  MgbaLink Protocol
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <cmath>
#include "MgbaLink_Protocol.h"

namespace PokemonAutomation{
namespace MgbaLink{


namespace{

//  Splits on single spaces, at most "max_parts" parts; the last part keeps
//  the rest of the line (a FRAME path may contain spaces).
std::vector<std::string_view> split_spaces(std::string_view text, size_t max_parts){
    std::vector<std::string_view> parts;
    while (parts.size() + 1 < max_parts){
        size_t space = text.find(' ');
        if (space == std::string_view::npos){
            break;
        }
        parts.emplace_back(text.substr(0, space));
        text.remove_prefix(space + 1);
    }
    parts.emplace_back(text);
    return parts;
}

//  Digits only: no sign, no spaces, no empty string.
std::optional<uint64_t> parse_unsigned(std::string_view text){
    uint64_t value = 0;
    if (text.empty()){
        return std::nullopt;
    }
    std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size()){
        return std::nullopt;
    }
    return value;
}

}  // namespace



std::optional<HelloReply> parse_hello_reply(std::string_view reply){
    std::vector<std::string_view> parts = split_spaces(reply, 6);
    if (parts.size() != 5 || parts[0] != "OK" || parts[1] != "1"){
        return std::nullopt;
    }
    for (std::string_view part : parts){
        if (part.empty()){
            return std::nullopt;
        }
    }
    return HelloReply{std::string(parts[2]), std::string(parts[3]), std::string(parts[4])};
}

std::optional<uint64_t> parse_ok_frame_reply(std::string_view reply){
    std::vector<std::string_view> parts = split_spaces(reply, 3);
    if (parts.size() != 2 || parts[0] != "OK"){
        return std::nullopt;
    }
    return parse_unsigned(parts[1]);
}

std::optional<uint64_t> parse_keys_reply(std::string_view reply){
    if (reply == "OK"){
        return 0;
    }
    std::vector<std::string_view> parts = split_spaces(reply, 3);
    if (parts.size() != 2 || parts[0] != "LATE"){
        return std::nullopt;
    }
    return parse_unsigned(parts[1]);
}

std::optional<FrameReply> parse_frame_reply(std::string_view reply){
    std::vector<std::string_view> parts = split_spaces(reply, 5);
    if (parts.size() != 5 || parts[0] != "FRAME" || parts[4].empty()){
        return std::nullopt;
    }
    std::optional<uint64_t> frame = parse_unsigned(parts[1]);
    std::optional<uint64_t> width = parse_unsigned(parts[2]);
    std::optional<uint64_t> height = parse_unsigned(parts[3]);
    if (!frame || !width || !height){
        return std::nullopt;
    }
    return FrameReply{*frame, (size_t)*width, (size_t)*height, std::string(parts[4])};
}



std::vector<std::string> LineSplitter::push(std::string_view data){
    std::vector<std::string> lines;
    if (m_overflowed){
        return lines;
    }
    while (!data.empty()){
        size_t newline = data.find('\n');
        std::string_view piece = data.substr(0, newline);
        if (m_buffer.size() + piece.size() > m_max_line + 1){
            //  "+ 1" leaves room for the CR that is dropped below.
            m_buffer.clear();
            m_overflowed = true;
            return lines;
        }
        m_buffer.append(piece);
        if (newline == std::string_view::npos){
            break;
        }
        if (!m_buffer.empty() && m_buffer.back() == '\r'){
            m_buffer.pop_back();
        }
        if (m_buffer.size() > m_max_line){
            m_buffer.clear();
            m_overflowed = true;
            return lines;
        }
        lines.emplace_back(std::move(m_buffer));
        m_buffer.clear();
        data.remove_prefix(newline + 1);
    }
    return lines;
}



WallClock FrameClock::timestamp(uint64_t frame) const{
    double frames = frame >= m_f0
        ? (double)(frame - m_f0)
        : -(double)(m_f0 - frame);
    double microseconds = std::round(frames * 1000000.0 / GBA_FRAMES_PER_SECOND);
    return m_t0 + std::chrono::duration_cast<WallDuration>(
        std::chrono::microseconds((int64_t)microseconds)
    );
}


FrameClock::Stamp FrameClock::stamp(WallClock now, uint64_t frame, WallDuration max_lag){
    Stamp result;
    WallClock time = timestamp(frame);
    if (time > now || now - time > max_lag){
        result.reanchored_after_lag = time < now;
        m_t0 = now;
        m_f0 = frame;
        time = now;
    }
    if (m_last_stamp && time < *m_last_stamp){
        time = *m_last_stamp;
    }
    m_last_stamp = time;
    result.time = time;
    return result;
}

}  // namespace MgbaLink
}  // namespace PokemonAutomation

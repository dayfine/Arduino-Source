/*  AutoStory Checkpoint Id
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_CheckpointId_H
#define PokemonAutomation_Pokemon_CheckpointId_H

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  An AutoStory checkpoint, written "SS.CC": segment 03, checkpoint 08 is
//  "03.08". Ids order by segment, then by checkpoint.
struct CheckpointId{
    uint8_t segment;
    uint8_t checkpoint;

    //  Reads exactly two digits, a dot and two digits. Anything else
    //  ("3.08", "03.08b", "03-08", "03.08.01") returns nullopt.
    static std::optional<CheckpointId> parse(std::string_view text);

    //  "03.08".
    std::string to_string() const;
    //  "03".
    std::string segment_string() const;

    auto operator<=>(const CheckpointId&) const = default;
};


//  Whether reaching `next` starts a segment: true for the first checkpoint of
//  a run (`previous` is nullopt) and when the segment differs from the last
//  checkpoint's. A caller adds one to AutoStoryStats::m_segments_started then.
bool starts_segment(std::optional<CheckpointId> previous, CheckpointId next);



}  // namespace Pokemon
}  // namespace PokemonAutomation
#endif

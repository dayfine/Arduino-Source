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


//  An AutoStory checkpoint, written "SS.CC" with an optional lowercase
//  letter: segment 03, checkpoint 08 is "03.08". A letter splits a checkpoint
//  without renumbering the ones after it, e.g. FRLG's "12.02b" for a boss
//  battle cut out of 12.02. Ids order by segment, then checkpoint, then
//  letter: 12.02 < 12.02b < 12.02c < 12.03. parse() is the only way to build
//  one, so every id is valid.
class CheckpointId{
public:
    //  Reads two digits, a dot, two digits and at most one lowercase letter.
    //  Anything else ("3.08", "03.08B", "03.08bc", "03.08b1", "03-08",
    //  "03.08.01") returns nullopt.
    static std::optional<CheckpointId> parse(std::string_view text);

    //  "03.08", or "03.08b" with a suffix.
    std::string to_string() const;
    //  "03".
    std::string segment_string() const;

    uint8_t segment() const{ return m_segment; }
    uint8_t checkpoint() const{ return m_checkpoint; }
    //  'a' to 'z'.
    std::optional<char> suffix() const{ return m_suffix; }

    auto operator<=>(const CheckpointId&) const = default;

private:
    CheckpointId(uint8_t segment, uint8_t checkpoint, std::optional<char> suffix)
        : m_segment(segment)
        , m_checkpoint(checkpoint)
        , m_suffix(suffix)
    {}

    uint8_t m_segment = 0;
    uint8_t m_checkpoint = 0;
    std::optional<char> m_suffix;
};


//  Whether reaching `next` starts a segment: true for the first checkpoint of
//  a run (`previous` is nullopt) and when the segment differs from the last
//  checkpoint's. The suffix does not matter.
bool starts_segment(std::optional<CheckpointId> previous, CheckpointId next);



}  // namespace Pokemon
}  // namespace PokemonAutomation
#endif

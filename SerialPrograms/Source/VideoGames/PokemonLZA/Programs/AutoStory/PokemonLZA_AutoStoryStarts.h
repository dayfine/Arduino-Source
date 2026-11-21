/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Start-signature matching used to identify or confirm which checkpoint
 *  the live game is positioned at before execution.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStarts_H
#define PokemonAutomation_PokemonLZA_AutoStoryStarts_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Observable screen fields identifying a checkpoint start; std::nullopt means don't-care.
struct StartSignature{
    std::optional<bool> talk_prompt = std::nullopt;
    std::optional<bool> wild_zone = std::nullopt;
    std::optional<size_t> party = std::nullopt;
    std::optional<std::string> minimap = std::nullopt;

    bool operator==(const StartSignature&) const = default;
};

//  Maps a checkpoint ID to the screen signature expected at its start.
struct CheckpointStart{
    std::string checkpoint = {};
    StartSignature signature = {};
};

//  Formats `signature` as a human-readable comma-separated summary (or "nothing read").
std::string to_string(const StartSignature& signature);

//  Returns the CheckpointStart entry matching `id`, or nullptr if none is registered.
const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id);

//  Matches `read` against `asked` or an unambiguous earlier checkpoint start; returns std::nullopt if none matches.
std::optional<size_t> checkpoint_at(
    std::span<const Pokemon::StoryCheckpoint> checkpoints, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  True when every field specified in `known` is present in `read` and equals it.
bool start_confirmed(const StartSignature& known, const StartSignature& read);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

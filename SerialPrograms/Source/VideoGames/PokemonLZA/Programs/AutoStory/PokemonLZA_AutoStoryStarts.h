/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a run is before it plays a checkpoint: a loaded save can be at an
 *  earlier checkpoint's start than the one asked for. Plans name what the
 *  screen shows at their start (execution "start_signature").
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStarts_H
#define PokemonAutomation_PokemonLZA_AutoStoryStarts_H

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMapPinReader.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What the screen shows where a checkpoint starts, one cheap reading per
//  field; none: not known (a plan) or not read (a reading).
struct StartSignature{
    //  The backup card's "Registered in the Pokedex".
    std::optional<size_t> registered = std::nullopt;
    std::optional<bool> talk_prompt = std::nullopt;
    //  In Wild Zone 1, by the map pin: the lead Pokemon's panel also shows on
    //  the street outside the zone.
    std::optional<bool> wild_zone = std::nullopt;
    //  Party bar slots: every Main Mission 03 catch is a new species.
    std::optional<size_t> party = std::nullopt;
    //  The matching_minimap() reference's name. An unmatched minimap reads as
    //  not read, so a start naming one matches only that reading.
    std::optional<std::string> minimap = std::nullopt;

    bool operator==(const StartSignature&) const = default;
};

//  "registered 1, talk prompt, not in a wild zone, minimap 03.04-start";
//  "nothing read" when it has no field.
std::string to_string(const StartSignature& signature);

//  A plan's execution "start_signature".
struct CheckpointStart{
    std::string checkpoint;
    StartSignature signature = {};
};

//  Null if `starts` names none for `id`.
const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id);

//  `asked` (an index of `rows`) when its start matches `read`, else the one
//  earlier checkpoint whose start names a place (registered, wild_zone or
//  minimap) and matches; party and prompt alone read the same at a dozen
//  starts. A match shares a field and agrees on all shared fields. None when
//  no checkpoint, or more than one earlier one, matches.
std::optional<size_t> checkpoint_at(
    std::span<const Pokemon::StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  checkpoint_at(), or `asked` when no plan names its start. None: the run
//  must stop ("start_position_unknown").
std::optional<size_t> run_start(
    std::span<const Pokemon::StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  For a chained run: every field `known` names but `registered` (backup
//  card only) is read and agrees. Stricter than checkpoint_at(), which an
//  unread field lets match.
bool start_confirmed(const StartSignature& known, const StartSignature& read);

//  None when the pin was not read.
std::optional<bool> wild_zone_from_map(const std::optional<MapPoint>& pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

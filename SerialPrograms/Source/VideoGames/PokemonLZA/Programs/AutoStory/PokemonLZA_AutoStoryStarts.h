/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a run is before it plays a checkpoint. A loaded save can be at an
 *  earlier checkpoint's start than the one asked for: on console B,
 *  2026-11-16, a run from 03.08 loaded a save at the Center counter
 *  (03.06's start) and searched Wild Zone 1 inside the Center. Each plan
 *  can name what the screen shows at its start (execution
 *  "start_signature"); the run reads the screen after the load and plays
 *  from the checkpoint whose start it matches, or stops. The backup card a
 *  rewind opens is read the same way, through the same table.
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
    //  The backup card's "Registered in the Pokedex" (the title's Up + B +
    //  X), for a backup save that loads there.
    std::optional<size_t> registered = std::nullopt;
    //  An A prompt over the scene, as at the Center counter ("A Talk").
    std::optional<bool> talk_prompt = std::nullopt;
    //  In Wild Zone 1, by the player's pin on the full map
    //  (wild_zone_from_map()). Not the lead Pokemon's panel at the bottom
    //  left: it shows on the street outside the zone too, and 03.08b's menu
    //  save there read as in the zone (console B, 2026-11-17 01:06).
    std::optional<bool> wild_zone = std::nullopt;
    //  The overworld party bar's filled slots (PartyBarDetector): every
    //  catch in Main Mission 03 is a new species, so the party tells 03.08's
    //  start (3) from a save after a catch (4) or after the fifth (5).
    std::optional<size_t> party = std::nullopt;
    //  The built-in minimap reference the minimap matches
    //  (matching_minimap()), by name: the place, wherever the player and
    //  camera face. A start that names one matches only a reading of that
    //  one: a minimap that matches no reference reads the same as one not
    //  read, and neither is a sign of being there. Station Front (03.04's
    //  start) and the roof before it read the same party and prompt.
    std::optional<std::string> minimap = std::nullopt;

    bool operator==(const StartSignature&) const = default;
};

//  "registered 1, talk prompt, not in a wild zone, minimap 03.04-start": the
//  fields `signature` has; "nothing read" when it has none.
std::string to_string(const StartSignature& signature);

//  A checkpoint whose start shows `signature`: a plan's execution
//  "start_signature".
struct CheckpointStart{
    std::string checkpoint;
    StartSignature signature = {};
};

//  The start `starts` names for checkpoint `id`, if any.
const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id);

//  The index of the checkpoint the player is at, by the reading `read`:
//  `asked` (an index of `rows`) when its start matches, else the one
//  checkpoint before it whose start matches. A start matches when it and
//  `read` share a field and agree on every field they share, and `read`
//  has the start's minimap when it names one. None when no
//  checkpoint up to `asked`, or more than one before it, matches: the
//  position is unknown, and no checkpoint may run from there.
std::optional<size_t> checkpoint_at(
    std::span<const Pokemon::StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  The checkpoint a run asked to start at `asked` (an index of `rows`)
//  plays from, by the reading `read` taken after the load:
//  checkpoint_at(). When no plan names `asked`'s start, there is nothing
//  to check it against: `asked`. An earlier start matching is no sign of
//  being there: 03.09 to 03.11 all read as party 5 outside a wild zone, and
//  Mable's office (03.11) ran 03.09's legs (console B, 2026-11-16 21:10).
//  None: the run must stop ("start_position_unknown").
std::optional<size_t> run_start(
    std::span<const Pokemon::StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
);

//  Whether `read`, the live screen a run chains on from, shows the start
//  `known`: every field `known` names is read and agrees, but
//  `registered`, which only the backup card shows. Stricter than a
//  reload's match (checkpoint_at()), which needs only the fields both
//  have: 03.08b's save in Wild Zone 1 read as 03.09's start (party 5) with
//  the zone not read (console B, 2026-11-18 13:37). Starts may share a
//  signature for this check; only a reload's match needs one checkpoint.
bool start_confirmed(const StartSignature& known, const StartSignature& read);

//  The signature's wild_zone by the player's pin on the full map
//  (read_map_pin()): whether it is in Wild Zone 1 (pin_in_zone_1()).
//  None when the pin was not read.
std::optional<bool> wild_zone_from_map(const std::optional<MapPoint>& pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Offline Story Replay Input Boundary
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_StoryReplay_IO_H
#define PokemonAutomation_PokemonFRLG_StoryReplay_IO_H
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryEvents.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryReplayInternal{
//  Owned input documents and the native maps whose raw bytes match their pins.
struct ReplayData{
    StoryCheckpoint row;
    std::vector<MapData> maps;
    JsonValue binding;
    JsonValue candidate;
    JsonValue native_unbound;
    std::string binding_hash;
    std::string source_revision;
};
//  Strict object boundary: all listed fields required, unknown fields rejected.
const JsonObject& fields(const JsonValue& value, std::initializer_list<std::string_view> names);
//  SHA-256 of exact input bytes, not a reformatted JSON document.
std::string digest(std::string_view text);
//  Direction vocabulary of the shared checkpoint format.
Direction direction(std::string_view value);
//  The named map must be bound, never an implicit resource fallback.
const MapData& map_named(const std::vector<MapData>& maps, std::string_view name);
//  Read a scripted runtime-local pose, checking bounds, occupants and elevation.
OverworldPose read_pose(const JsonValue& value, const std::vector<MapData>& maps);
//  Resolve a known one-tile authored pose; this does not create evidence.
OverworldPose authored_pose(const Pokemon::Pose& value, const std::vector<MapData>& maps);
//  JSON snapshot of a known model pose, or a candidate with unknown facing.
JsonValue pose_json(const OverworldPose& pose, std::optional<Direction> facing);
//  Reject bad hashes, capabilities and policies before any executor action.
ReplayData read_data(std::string_view capture, std::string_view binding, std::string_view candidate);
//  A scripted screen between two steps: its frames, `ms` after the look
//  starts, and how long the look lasts.
struct ReplayScreen{
    std::vector<std::pair<int, Pokemon::ScreenEvents>> frames;
    int look_ms = 0;
};
//  The screen an interrupt shows between steps, by the scenario's kind:
//  "dialog", "wild-encounter", "trainer-sight" or "warp".
ReplayScreen between_steps_screen(std::string_view kind);
//  Validate every scripted signal against its event's capability/parameters,
//  and every interrupt between steps against its event and kind.
void validate_scenario(const ReplayData& data, const JsonValue& scenario);
} // namespace StoryReplayInternal
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

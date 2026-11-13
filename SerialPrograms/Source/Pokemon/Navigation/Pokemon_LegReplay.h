/*  Leg Replay
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A console failure as a small fault file (the files of a game's ConsoleFailures folder beside
 *  a game's simulation tests), made from a console log's [Leg] records and
 *  raw walk facts by devtools/maps/leg_replay.py. The file keeps only what
 *  the simulation cannot work out from its map: where the run started if
 *  not on its checkpoint's start, the detections it missed, the walks that
 *  went further than counted, and the people the map lacks. Trainers, NPCs
 *  and walls come from the map. Where the failure happened and the frame it
 *  was checked against ride along for the reader.
 *
 *      {"checkpoint": "01.02",
 *       "faults": [{"kind": "hold_through_warp", "step": "past_curtis",
 *                   "direction": "east", "fade": "unseen", "far_pose_of": "into_oreburgh"}],
 *       "failed": {"step": "into_oreburgh", "error": "no_banner"},
// code-health: allow header_symbols -- the fault file model and its reader are one unit, a split would scatter it
 */

#ifndef PokemonAutomation_Pokemon_LegReplay_H
#define PokemonAutomation_Pokemon_LegReplay_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon_Direction.h"
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A tile of a named map.
struct ReplayTile{
    std::string map;
    TilePos tile;
};

//  What went differently on the console than the program counted on.
enum class ReplayFaultKind{
    FADE_MISSED,        //  the `index`th fade of the run goes unseen
    BANNER_MISSED,      //  the `index`th map banner of the run goes unseen
    MOVED_FURTHER,      //  the step's counted taps went `tiles` further (onto a warp, `far_pose_of`)
    HOLD_THROUGH_WARP,  //  the step's hold toward `direction` ran on through a warp (`far_pose_of`)
    SCENE_ENDS_FURTHER, //  the player stood `tiles` toward `direction` past where the scene step leaves them
    PERSON_STANDS,      //  `name`, a person of the map (who wanders or stays put), stood on `tile`
};
//  "fade_missed", "banner_missed", "moved_further", "hold_through_warp", "scene_ends_further", "person_stands".
std::string replay_fault_kind_name(ReplayFaultKind kind);

//  How the walk saw the fade of a warp it went through.
enum class ReplayFade{
    UNSEEN,
    BLACK,
    DIM,
    WHITE,
};

//  One fault; the fields of its kind (above) are the ones that count.
struct ReplayFault{
    ReplayFaultKind kind = ReplayFaultKind::FADE_MISSED;
    //  FADE_MISSED, BANNER_MISSED: counted within the checkpoint, from 0.
    size_t index = 0;
    //  MOVED_FURTHER, HOLD_THROUGH_WARP, SCENE_ENDS_FURTHER.
    std::string step;
    Direction direction = Direction::EAST;
    size_t tiles = 1;
    ReplayFade fade = ReplayFade::UNSEEN;
    bool banner_up = false;
    //  The step of the checkpoint whose pose after it is where the warp lands.
    std::string far_pose_of;
    //  PERSON_STANDS.
    std::string name;
    TilePos tile;
};

//  A console failure: the checkpoint it ran, its faults and what it failed at.
struct ConsoleFailure{
    std::string name;
    std::string checkpoint;
    //  Where the run starts if not on the checkpoint's start.
    std::optional<ReplayTile> start;
    std::optional<Direction> start_facing;
    std::vector<ReplayFault> faults;
    //  What the console did: the step and the start of its error.
    std::string failed_step;
    std::string failed_error;
    //  The frame the failure was checked against, a file name in the lab's
    //  failure screenshots.
    std::string frame;
    //  The log lines the faults were worked out from.
    std::vector<std::string> evidence;
};

//  Reads a fault file; throws on a missing or unknown field.
ConsoleFailure parse_console_failure(const std::string& name, const std::string& text);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif

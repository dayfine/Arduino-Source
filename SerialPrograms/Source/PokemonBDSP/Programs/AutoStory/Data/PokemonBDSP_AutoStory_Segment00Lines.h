/*  BDSP AutoStory - Segment 00's walked lines
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checkpoints 00.01 to 00.03 (the house, Twinleaf, Route 201, the Verity
 *  Lakefront, the lake, and back to Sandgem) replay recorded stick holds
 *  today. These are the same walks as lines on the maps: corner to corner,
 *  each stretch one straight leg, ending on the tile that starts the next
 *  scene. The scenes' trigger rectangles come from the Diamond decomp's
 *  events files, so a test can show each line steps into its scene on its
 *  last tile and not before. Nothing runs them on the console yet (task
 *  #22); the segment keeps its replays until a new-game run has checked
 *  the lines.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00Lines_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00Lines_H

#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_StoryWorld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace Segment00Lines{


//  Map names in segment00_maps(); Route 201 and Sandgem Town go by
//  StoryMaps' MAP_ROUTE_201 and MAP_SANDGEM_TOWN.
extern const char* const MAP_TWINLEAF_TOWN;
extern const char* const MAP_VERITY_LAKEFRONT;

//  Twinleaf Town, Route 201, the Verity Lakefront and Sandgem Town joined on
//  one grid at their world offsets from the decomp. Lake Verity is not in
//  it: its own matrix, and the scene walks us in and the game warps us out,
//  so no line is walked there.
const StoryWorld& segment00_maps();

//  A rectangle of tiles that starts a scene when the player steps in while
//  the story is in `when`. From the decomp's coord events, as local tiles.
struct SceneTrigger{
    std::string name;
    std::string map;
    Pokemon::TilePos corner;    //  north-west tile
    int width;
    int height;
    std::string when;           //  the story state it fires in
};
const std::vector<SceneTrigger>& scene_triggers();
bool trigger_covers(const SceneTrigger& trigger, const MapTile& tile);

//  A line walked corner to corner: every stretch between two corners is one
//  straight open leg (whole tiles, as the D-pad walks them).
struct WalkedLine{
    std::string checkpoint;
    std::string name;
    std::vector<MapTile> corners;      //  the first is where the line starts
    std::string ends_in;                        //  the scene the last corner steps into, "" for none
    std::vector<std::string> live_triggers;     //  the scenes armed while this line is walked
};
const std::vector<WalkedLine>& lines();

//  The route records start and end on named points of the maps (Twinleaf
//  Town's house_doorstep, Route 201's barry_warning, ...), all on the lines.


}
}
}
}
#endif

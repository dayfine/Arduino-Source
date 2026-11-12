/*  Map Gates
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The barriers a map's story opens (card-key doors, switch-driven walls) and
 *  the statues that press the switches. Pokemon_MapData.h holds them in a
 *  map's `gates` and `switches`; Pokemon_GateState.h says which are open.
 *
 */

#ifndef PokemonAutomation_Pokemon_MapGates_H
#define PokemonAutomation_Pokemon_MapGates_H

#include <string>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_MapTiles.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What opens a gate.
enum class GateKind{
    CARD_KEY,   //  "card_key_face_a": faced from a side tile and opened with A, once the player holds the Card Key
    SWITCH,     //  a switch's state (MapSwitch): open while the switch is `open_when_on`
};

//  A barrier the story opens or closes: some tiles of the map that are `G`
//  in `tiles`. Open, they read as `open_tile`; closed, as a wall.
struct MapGate{
    std::string id;
    std::vector<TilePos> tiles;
    TileKind open_tile = TileKind::OPEN;
    //  Whether a new game has it open.
    bool open_at_new_game = false;
    GateKind kind = GateKind::CARD_KEY;
    //  CARD_KEY: the event flag the game sets once it is opened.
    std::string flag = {};
    //  SWITCH: the switch (a key of MapData::switches, shared by the
    //  maps in its `floors`), and the state in which this gate is open.
    std::string switch_id = {};
    bool open_when_on = false;
};

//  A statue that presses a switch: the player stands on `face_from`, faces
//  `facing` and presses A. The statue is the wall tile `tile`.
struct MapStatue{
    TilePos tile;
    TilePos face_from;
    Direction facing = Direction::NORTH;
};

//  A switch the map's gates follow. One state is shared by every map in
//  `floors` (Pokemon Mansion's statue switch, an event flag): pressing a
//  statue flips it, and each floor redraws its gates when it loads.
//  `statues` are the statues on this map only.
struct MapSwitch{
    std::string id;
    std::string flag;
    bool toggle = true;
    std::vector<std::string> floors;
    std::vector<MapStatue> statues;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

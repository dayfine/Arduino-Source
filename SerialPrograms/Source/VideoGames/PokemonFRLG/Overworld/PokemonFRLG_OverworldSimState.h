/*  Overworld Sim State
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The simulated game's script and state, and what the simulated walker
 *  saw: the parts of OverworldSim its tests set up and check.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSimState_H
#define PokemonAutomation_PokemonFRLG_OverworldSimState_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSimScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a test sets up beyond the maps.
struct SimScript{
    std::vector<SimEncounter> encounters;
    //  The START menu is open at the start: the D-pad moves its cursor.
    bool start_menu_open = false;
    //  The hold numbered by the key (from 1) stops this many tiles short,
    //  with nothing on screen (the forest drift, console B, 2026-11-01).
    std::map<int, int> short_holds;
    //  Maps where every step can start a wild battle, also on tiles the maps
    //  don't mark as grass: Mt. Moon B2F's tiles closed on one side ('8')
    //  are cave floor too (console B, 2026-11-03).
    std::set<std::string> wild_floor_maps;
    //  Whether the game holds the player up a tile on a step onto a
    //  connected map. It did north onto Nugget Bridge (console B,
    //  2026-11-03, rows 00.16 and 00.20), not south from Route 6 into
    //  Vermilion City (row 00.24).
    bool connections_stall = true;
    SimTiming timing;
};

//  The player and the screen, and what went wrong that the game would show.
struct SimState{
    MapPosition position;
    Direction facing = Direction::SOUTH;
    SimScreen screen = SimScreen::OVERWORLD;
    //  Whether black shows below the map: the player stands near the bottom
    //  of a map with nothing joined to it (a small room or a gate).
    bool black_below = false;
    int steps = 0;
    int warps = 0;
    int signs_read = 0;
    std::vector<std::string> trainers_beaten;
    //  Things the walker did that the game wouldn't let pass: D-pad holds
    //  under a battle, a trainer's battle fled, a battle fought that wasn't
    //  there.
    std::vector<std::string> violations;
    std::vector<std::string> log;
};

//  A watcher that fired: which (an index into the watchers asked for), and
//  how long after the hold or watch started.
struct SimSeen{
    size_t watcher = 0;
    Milliseconds elapsed{0};
};

//  How the walker handles the battle on screen.
enum class SimBattleChoice{
    RUN,
    FIGHT,
};
//  How a simulated battle concluded.
enum class SimBattleEnd{
    ENDED,
    CANT_RUN,       //  a trainer's battle: "No! There's no running from a TRAINER battle!"
    NO_BATTLE,      //  there was none, on screen or about to show
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

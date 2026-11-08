/*  BDSP AutoStory - Disturbances on the grid simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One seed's disturbances (Pokemon::DisturbancePlan) played on a GridSim,
 *  as the console disturbs a run:
 *
 *    - A wild battle on tall grass, or anywhere in a cave (Oreburgh Mine,
 *      the Ravaged Path), once the player has stepped onto the tile. It is
 *      fled, and the input goes on, as the console's walks go on.
 *    - Drift: a stick press or a hold stopped short of a wall ends up to
 *      DisturbanceRates::drift_per_tile off along its way, a D-pad tap up
 *      to DisturbanceRates::tap_drift_per_tile (none by default). Within DisturbanceRates::fit of the tile the player still
 *      fits a one-tile gap; past it, the grid sim has them part of a tile
 *      off, where a door frame or a rock's corner stops them. A walk that
 *      stops at a wall ends on its tile, and on the grid along that way.
 *    - The people who wander on the maps (MapObject::wander) walk about,
 *      each from a seed and a start of the plan's.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_SimDisturbance_H
#define PokemonAutomation_PokemonBDSP_AutoStory_SimDisturbance_H

#include <map>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryMonteCarlo.h"
#include "PokemonBDSP_AutoStory_GridSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A seed's disturbances on a GridSim. Not thread-safe; one per run.
class SimDisturbance{
public:
    explicit SimDisturbance(Pokemon::DisturbancePlan plan);

    const Pokemon::DisturbancePlan& plan() const{
        return m_plan;
    }
    //  Wild battles drawn so far.
    size_t battles() const{
        return m_battles;
    }
    //  Encounter tiles stepped onto so far.
    struct EncounterTiles{
        size_t grass = 0;
        size_t cave = 0;
    };
    const EncounterTiles& encounter_tiles() const{
        return m_tiles;
    }
    //  Whether stepping onto world tile `pos` starts a wild battle.
    bool battle_at(const StoryWorld& world, const Pokemon::TilePos& pos);
    //  The player moved `thousandths` of a tile toward `direction` and did
    //  not stop at a wall (or, `at_wall`, stopped at one): drifts them by up
    //  to `drift_per_tile` thousandths a tile walked.
    void walked(GridSim& sim, Pokemon::Direction direction, int thousandths, bool at_wall, int drift_per_tile);
    //  The player was put on a tile: no drift.
    void reset();
    //  The people who wander on `world`, each with a seed and start drawn.
    std::vector<SimWanderer> wanderers(const StoryWorld& world);

private:
    //  How far off the player is along one axis, in thousandths of a tile:
    //  `drift` in all, `applied` of it on the grid sim's position.
    struct Axis{
        int drift = 0;
        int applied = 0;
    };
    void sync(const GridSim& sim);
    void materialize(GridSim& sim, bool across);
    const std::vector<SimWanderer>& people(const StoryWorld& world);
    void add_people(std::vector<SimWanderer>& people, const std::string& map, const Pokemon::TilePos& corner, const Pokemon::TilePos& local);

    Pokemon::DisturbancePlan m_plan;
    size_t m_battles = 0;
    EncounterTiles m_tiles;
    Axis m_x;
    Axis m_y;
    std::map<const StoryWorld*, std::vector<SimWanderer>> m_people;
};


//  A hold on the grid sim (GridSim::hold()), a tile at a time with wild
//  battles and drift from `noise` if given.
struct DisturbedHold{
    SimMove move;
    size_t battles = 0;
    //  A battle on the hold's first tile: the console cannot tell it from
    //  one that began before the hold (WalkOrigin::MAY_PRECEDE_LEG).
    bool first_tile = false;
};
//  Holds toward `direction` for up to `limit`; see DisturbedHold.
DisturbedHold disturbed_hold(GridSim& sim, SimDisturbance* noise, Pokemon::Direction direction, Milliseconds limit, SimPace pace);


//  GridSimRouteIO, disturbed by `noise` (none: GridSimRouteIO itself).
class DisturbedRouteIO : public GridSimRouteIO{
public:
    DisturbedRouteIO(GridSim& sim, std::vector<std::string>& notes, SimDisturbance* noise)
        : GridSimRouteIO(sim, notes)
        , m_sim(sim)
        , m_notes(notes)
        , m_noise(noise)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual void press(const RouteInput& input) override;

private:
    GridSim& m_sim;
    std::vector<std::string>& m_notes;
    SimDisturbance* m_noise;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

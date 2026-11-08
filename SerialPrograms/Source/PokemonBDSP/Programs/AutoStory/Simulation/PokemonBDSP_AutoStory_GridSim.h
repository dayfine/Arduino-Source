/*  BDSP AutoStory - Grid simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A player walking on a story world, by the movement measured on the
 *  console, so walks that go wrong only on the Switch go wrong in tests:
 *
 *    - A D-pad tap of 40 to 80 ms only turns; 160 ms steps a whole tile.
 *    - The D-pad held walks whole tiles until something stops it.
 *    - A stick tap moves part of a tile (100 ms about 0.6 tile across), and
 *      later D-pad moves keep that offset: the player is off the tile grid.
 *    - Walls, signs and people on their tiles stop the player. Off the grid
 *      the player covers two tiles, and both must be open: a door frame or
 *      a tree corner stops a player a part of a tile off.
 *    - Walking onto a door warps, but only from its exact column or row.
 *    - A trainer whose sight line the player steps into stops the walk with
 *      their dialogue; so does a scene's trigger tile.
 *
 *  Positions are in thousandths of a tile, so parts of tiles add up exactly.
 *  GridSimRouteIO is the walker's RouteIO on a GridSim.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_GridSim_H
#define PokemonAutomation_PokemonBDSP_AutoStory_GridSim_H

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A tile, in the simulation's units.
inline constexpr int SIM_TILE = 1000;

//  A world position in thousandths of a tile: the player covers the square
//  from (x, y) to (x + SIM_TILE, y + SIM_TILE).
struct SimPos{
    int x = 0;
    int y = 0;
    bool operator==(const SimPos& other) const = default;
};
//  Converts a TilePos to its fixed-point fractional simulation coordinate.
SimPos sim_pos(const Pokemon::TilePos& tile);
//  Whether the player stands exactly on a tile.
bool on_grid(const SimPos& pos);
//  The tile the player mostly stands on.
Pokemon::TilePos nearest_tile(const SimPos& pos);
//  "(12.375, 5)".
std::string to_string(const SimPos& pos);

//  A scene that starts when the player steps onto one of `tiles` (a coord
//  event): its dialogue opens and the player stops. It fires once.
struct SimTrigger{
    std::string name;
    std::vector<Pokemon::TilePos> tiles;
};

//  Where the simulated player walks: the world (which must outlive the
//  simulation), its trainers and its scene triggers, in world tiles; and
//  the tiles that warp like doors though the maps draw them as walls (a
//  house's stairs, entered from beside them); and, if the caller knows,
//  whether B runs from here on (the Running Shoes, 00.03).
//  A person who wanders pseudo-randomly within their wander box.
struct SimWanderer{
    std::string name;
    Pokemon::TileBox box;                  //  wander boundary in world tiles
    Pokemon::TilePos pos;                  //  current position
    uint64_t seed = 0;                     //  PRNG seed for deterministic walk
    Milliseconds step_interval = Milliseconds(2000);   //  how often the NPC attempts a move
    Milliseconds elapsed = Milliseconds(0);            //  time accumulated towards next step
    Milliseconds time_offset = Milliseconds(0);        //  initial start time offset
};

struct SimWorld{
    const StoryWorld* world = nullptr;
    std::vector<TrainerSight> trainers = {};
    std::vector<SimTrigger> triggers = {};
    std::vector<Pokemon::TilePos> doors = {};
    std::optional<bool> can_run = std::nullopt;
    std::vector<SimWanderer> wanderers = {};
};

//  Reason why simulated player movement stopped.
enum class SimStop{
    DONE,       //  the input finished
    BLOCKED,    //  stopped by a wall, a person, or a door off its column
    DIALOG,     //  a trainer saw us or a scene started; the player is frozen
    WARP,       //  stepped onto a door
    LIMIT,      //  a held walk ran out of time
};
//  Returns the debug string for a SimStop reason.
const char* sim_stop_name(SimStop stop);

//  What one input did.
struct SimMove{
    SimStop stop = SimStop::DONE;
    int distance = 0;               //  thousandths of a tile
    Milliseconds time = Milliseconds(0);
};

//  The player as the simulation has them.
struct SimPlayer{
    SimPos pos;
    Pokemon::Direction facing = Pokemon::Direction::NORTH;
    //  Whether B runs: the Running Shoes (00.03).
    bool can_run = false;
    //  A dialogue is open: a trainer's, a scene's or a talk's.
    bool dialog = false;
    //  The trainer whose dialogue is open, if one is.
    std::optional<std::string> trainer;
    //  The door the last warp stepped onto.
    std::optional<Pokemon::TilePos> warped;
    std::vector<std::string> fought = {};
    std::vector<std::string> fired = {};
};

//  How long a held walk takes per tile in `direction`.
enum class SimPace{
    WALK,
    RUN,    //  B held: a run once the player has the Running Shoes
};

//  Simulates grid-based player movement and collisions in BDSP.
class GridSim{
public:
    GridSim(SimWorld world, const Pokemon::TilePos& start, Pokemon::Direction facing, bool can_run);

    //  One press toward `direction`: the D-pad or the stick, held for `hold`.
    SimMove tap(Pokemon::Direction direction, Milliseconds hold, bool stick);
    //  Holds toward `direction` until something stops the player or `limit`
    //  runs out. Moves whole tiles, from wherever the player stands.
    SimMove hold(Pokemon::Direction direction, Milliseconds limit, SimPace pace);
    //  A: talks to a person or a sign on the faced tile. Returns whether a
    //  dialogue is open.
    bool press_a();
    //  The open dialogue ends: a trainer's battle is won and they see us no more.
    void end_dialogue();
    //  Puts the player on `start` of `world`: after a warp or a scene.
    void place(SimWorld world, const Pokemon::TilePos& start, std::optional<Pokemon::Direction> facing);

    const SimPlayer& player() const{
        return m_player;
    }
    const StoryWorld& world() const{
        return *m_world.world;
    }

private:
    friend void sim_advance_time(GridSim& sim, Milliseconds time);
    friend const std::vector<SimWanderer>& sim_wanderers(const GridSim& sim);

    SimMove advance(Pokemon::Direction direction, int distance);
    std::optional<SimStop> enter_next(Pokemon::Direction direction, int part, int& moved);
    std::optional<SimStop> arrive();
    void advance_wanderer(SimWanderer& wanderer, Milliseconds time);
    void step_wanderer(SimWanderer& wanderer);
    bool can_wander_to(const SimWanderer& wanderer, const Pokemon::TilePos& next) const;
    bool wanderer_at(const Pokemon::TilePos& pos) const;

    SimWorld m_world;
    SimPlayer m_player;
    std::vector<SimWanderer> m_wanderers;
};

//  Advances simulated time for world events and wandering NPCs.
void sim_advance_time(GridSim& sim, Milliseconds time);
//  The wandering NPCs on the simulation's world.
const std::vector<SimWanderer>& sim_wanderers(const GridSim& sim);

//  Whether the player could take a step toward `direction` from where they stand.
bool sim_can_step(const GridSim& sim, Pokemon::Direction direction);


//  A deterministic, already-cleared encounter at a tap boundary. This is
//  normalized movement/escape evidence, not a model of image detectors or
//  battle mechanics. BEFORE leaves the tile unchanged; AFTER completes it.
enum class SimEncounterPhase{ BEFORE_MOVEMENT, AFTER_MOVEMENT };
//  Match one tap and supply its cleared encounter phase and origin evidence.
struct SimWildTap{
    std::string label;
    Pokemon::Direction direction = Pokemon::Direction::NORTH;
    size_t requested_tiles = 0;
    size_t tap = 0;  //  one-based within the matching walk_taps call
    SimEncounterPhase phase = SimEncounterPhase::AFTER_MOVEMENT;
    WalkOrigin origin = WalkOrigin::CURRENT_LEG;
};

//  The walker's RouteIO on a GridSim. A failure throws GridSimFailure.
class GridSimRouteIO : public RouteIO{
public:
    //  Every line noted goes to `notes`, which must outlive this.
    GridSimRouteIO(GridSim& sim, std::vector<std::string>& notes, std::vector<SimWildTap> encounters = {})
        : m_sim(sim)
        , m_notes(notes)
        , m_encounters(std::move(encounters))
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual void press(const RouteInput& input) override;
    virtual bool dialog_within(Milliseconds wait, Color color) override;
    virtual bool holding_against_wall(Pokemon::Direction direction, const std::string& label) override;
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>& at) override;
    virtual void note(const std::string& text, RouteNote kind) override;

private:
    std::optional<SimWildTap> take_encounter(Pokemon::Direction direction, size_t tiles, size_t tap, const std::string& label);

    GridSim& m_sim;
    std::vector<std::string>& m_notes;
    std::vector<SimWildTap> m_encounters;
};

//  What GridSimRouteIO throws for RouteNote::FAILURE.
struct GridSimFailure{
    std::string message;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Overworld Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A whole-tile model of FireRed's overworld, for running the walker
 *  (run_route(), Pokemon::walk_leg_with()) without a console. The player
 *  moves a tile per 16 frames while the D-pad is held, by the maps' step
 *  rules (classify_step()), and also hops ledges the way they face, takes
 *  warps with a fade, reads signs walked into moving north or south, and is
 *  seen by trainers along their sight. Wild battles start on the steps a
 *  test chooses. Each of these shows a screen at a time after the step's
 *  press, which the walker's watchers see or miss as the console's do.
 *
 *  Every console failure of the walker becomes a test here (red first).
 *  OverworldSimWalker is the console side of run_route() (RouteCalls) on
 *  the sim, step for step as walk_overworld_route_with_recovery() does it,
 *  with the walker's rules as switches, so a test can show a failure with
 *  the rule that was missing and its absence with the rule.
 *
 *  A step onto a connected map waits a tile first, as the console does
 *  while the game loads the map.
 *
 *  Not modelled: Surf, bikes, running, HP and status. Wandering people
 *  stand on their tile.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSim_H
#define PokemonAutomation_PokemonFRLG_OverworldSim_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_OverworldSimState.h"
#include "PokemonFRLG_OverworldSimScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Simulates overworld movement, encounters and dialogs.
class OverworldSim{
public:
    OverworldSim(std::vector<MapData> maps, MapPosition start, Direction facing, SimScript script = {});

    const SimState& state() const{ return m_state; }

    //  Holds `direction` for `duration` then lets go for `release`, while
    //  `watchers` watch until `watch` (at least the hold and release) from
    //  the start. Returns the first that saw its screen, as run_until().
    std::optional<SimSeen> hold(
        Direction direction, Milliseconds duration, Milliseconds release, Milliseconds watch,
        const std::vector<SimWatcher>& watchers
    );

    //  A short press of `direction`: turns the player, or steps if they
    //  already face that way.
    void tap(Direction direction);

    //  Watches `watchers` for `duration`, as wait_until().
    std::optional<SimSeen> watch(const std::vector<SimWatcher>& watchers, Milliseconds duration);

    //  Runs from or fights the battle on screen, back to the overworld. A
    //  trainer's battle can't be run from; a battle that isn't there is
    //  waited out (the story's battle loop gives up after 15 s).
    SimBattleEnd handle_battle(SimBattleChoice choice);

    //  Closes the message on screen with B. Returns whether a trainer's
    //  battle follows it.
    bool close_message();

    //  Cuts the tree the player faces (an object named "cut_tree") with
    //  YES to the game's question: it is gone until the player next warps,
    //  as its hide flag is a FLAG_TEMP. Returns whether there was one.
    bool cut_tree();

private:
    //  A screen that shows at `at`; `unfreeze`: the player can move again.
    struct Pending{
        Milliseconds at;
        SimScreen screen;
        bool unfreeze;
    };
    //  Where one press of the D-pad takes the player.
    struct Step{
        int map = 0;
        PlayerState player;
        bool warped = false;
        bool walked_out = false;    //  the warp ended with the game walking the player a tile south
        bool ledge = false;
    };

    void advance_to(Milliseconds time);
    void show(SimScreen screen, Milliseconds at);
    void schedule(Milliseconds at, SimScreen screen, bool unfreeze);
    bool sees(SimWatcher watcher, SimScreen screen) const;
    std::optional<Milliseconds> first_seen(SimWatcher watcher, Milliseconds from, Milliseconds to) const;
    std::optional<SimSeen> watch_until(const std::vector<SimWatcher>& watchers, Milliseconds from, Milliseconds to);
    void walk_hold(Direction direction, Milliseconds duration, int short_by);
    void stuck_hold(Direction direction);
    void bump(Direction direction, Milliseconds pressed);
    std::optional<Step> press(Direction direction) const;
    std::optional<Step> take_warp(int map_index, const TilePos& tile) const;
    std::optional<Step> cross_edge(Direction direction) const;
    bool after_step(const Step& step, Milliseconds pressed);
    bool wild_battle(Milliseconds pressed);
    bool trainer_sees(Milliseconds pressed);
    void update_view();
    void note(const std::string& line);
    void violation(const std::string& line);
    const MapData& map() const{ return m_maps[m_map]; }

    std::vector<MapData> m_maps;
    SimScript m_script;
    int m_map = 0;
    PlayerState m_player;
    Milliseconds m_now{0};
    std::vector<Pending> m_pending;     //  in time order
    //  The game has stopped the player: a battle, a message, a warp.
    bool m_frozen = false;
    //  A battle is on screen or about to show.
    bool m_battle_pending = false;
    //  The trainer (an object of the player's map) whose message is up or
    //  about to show, and whose battle follows it.
    std::optional<size_t> m_trainer_speaking;
    //  The trainer whose battle is on screen or about to show.
    std::optional<size_t> m_trainer;
    //  The trees cut since the last warp, and their maps.
    std::vector<std::pair<int, MapObject>> m_cut_trees;
    int m_grass_steps = 0;
    int m_holds = 0;
    SimState m_state;
};


//  The walker's rules that console failures added, each on unless a test
//  turns it off to show the failure it fixed.
struct SimWalkerRules{
    //  Fail before a step unless the overworld is up (START menu left open,
    //  console B, 2026-11-01, Viridian Forest).
    bool check_overworld = true;
    //  A battle needs an HP box or the battle menu: black below a small room
    //  isn't one (console B, 2026-11-01, Viridian Forest's north gate).
    bool battle_needs_hp_box = true;
    //  A trainer's battle after their message is fought, not taken for a
    //  wild one and run from (console B, 2026-11-01, Viridian Forest).
    bool fight_trainer_after_message = true;
    //  How long the walker watches after a leg through grass for a battle
    //  whose flashes it missed. It watched 500 ms, and walked the next leg
    //  into the battle (console B, 2026-11-01, Route 1, run 7 and r14).
    Milliseconds after_wild_leg_watch = OverworldWalkSteps::AFTER_WILD_LEG_WATCH;
    //  A leg to a wall held again after an interrupt stops when it is
    //  interrupted again right away: by a sign it is held into. Without it
    //  the walker read the sign 6 times (console B, 2026-11-01, r16).
    bool stop_if_blocked = true;
    //  How long a hold's watch goes on past the hold and its release. 0 as
    //  now; the tile-at-a-time build watched each step for 900 ms, and saw
    //  the warp's fade on the step onto the forest's north gate as a battle.
    Milliseconds watch_past_hold{0};
    //  A leg onto a warp whose fade was a wild battle's (its wipe passes for
    //  the fade) stops before the warp, and the player is found again on it.
    //  Without it the walker stopped as off the route (console B,
    //  2026-11-03, Mt. Moon 1F, row 00.13).
    bool find_player_after_warp_battle = true;
    //  After a leg's watch saw nothing, a battle's text box on screen (its
    //  HP boxes still to come) is watched on for, and the leg taken as
    //  stopped part-way. Without it the walker took "south 15 (to wall)"
    //  as walked and the next leg into the battle (console B, 2026-11-03,
    //  Mt. Moon B1F, row 00.13).
    bool watch_on_for_unseen_battle = true;
    //  The same after a leg the maps see no wild tiles on (RouteStep::wild),
    //  which the leg and its watch don't watch for battles: on Mt. Moon
    //  B2F's tiles closed on one side, which the maps don't mark as wild, a
    //  battle on "east 7 (to wall)" left black after the leg, the leg was
    //  taken as walked and the next one held from where the battle was
    //  (console B, 2026-11-03, row 00.13).
    bool watch_on_after_unmarked_leg = true;
    //  Before a step, a battle on its way (its text box or wipe) is watched
    //  on for, and the step before it taken as stopped part-way, not the
    //  step held from where the battle left the player.
    bool battle_before_step = true;
    //  A hold over a step onto a connected map is a tile longer for it
    //  (RouteStep::connection_steps). Without it the open leg north from
    //  Cerulean City onto Nugget Bridge ended a tile short (console B,
    //  2026-11-03, rows 00.16 and 00.20).
    bool hold_over_connections = true;
    //  An open leg after whose hold a wild battle was seen is placed by the
    //  battle's first timed screen event (place_late_stop()). Without it the
    //  player may be anywhere on the leg: on Route 10, "east 11" was found
    //  again from all 12 of its tiles (console B, 2026-11-09, 00.27 run 2).
    bool place_late_battle = true;
};

//  The console side of run_route() on an OverworldSim, as
//  walk_overworld_route_with_recovery() does it, with the story's
//  interrupts: a wild battle (flashes, or seen late) is fought or run from,
//  a message is closed and a trainer's battle after it fought.
class OverworldSimWalker{
public:
    //  `interrupts`: whose detection delays place a stopped leg, the
    //  walker's own or a caller's (story_interrupts()). Each set lists a
    //  wild battle, one seen late and a message for `wild`, and a message
    //  for `other`, in that order.
    OverworldSimWalker(
        OverworldSim& sim, SimWalkerRules rules = {},
        const OverworldInterruptSet& interrupts = default_overworld_interrupts()
    );

    //  RouteCalls whose `fail` throws OperationFailedException, as the
    //  console's does.
    RouteCalls calls();

    //  run_route() from the sim's facing, planned with `planning`. Returns
    //  the failure's message, or empty if the route was walked.
    std::string run(const std::vector<RouteStep>& route, const RoutePlanning& planning);

    const std::vector<std::string>& log() const{ return m_log; }

private:
    //  One of the story's interrupts (story_interrupts()), with the
    //  walker's detection delay for it.
    struct Interrupt{
        std::string name;
        SimWatcher watcher;
        Milliseconds delay;
        bool battle;
    };
    StepOutcome walk(const RouteStep& step, std::optional<Direction> facing);
    std::optional<WalkedTiles> late_stop(const RouteStep& step);
    std::optional<WalkedTiles> walk_step(const RouteStep& step, std::optional<Direction> facing);
    bool after_warp(const RouteStep& step, int warps_before);
    [[noreturn]] void fail_off_route(const RouteStep& step);
    std::optional<SimSeen> hold_watching(const RouteStep& step, Milliseconds hold, const std::vector<Interrupt>& interrupts);
    size_t clear_late(const std::vector<Interrupt>& interrupts, Milliseconds watch, size_t max);
    size_t clear_unseen_battle(const RouteStep& step);
    bool battle_on_its_way() const;
    void clear(const Interrupt& interrupt);
    void hold_to_warp(const RouteStep& step);
    StepEnd watch_for_trainer(const RouteStep& step, int approach_tiles);
    bool moved_past_wall(const RouteStep& step);
    void check_overworld(const RouteStep& step);
    const std::vector<Interrupt>& interrupts(const RouteStep& step) const;
    std::vector<SimWatcher> watchers(const std::vector<Interrupt>& interrupts) const;
    [[noreturn]] void fail(const std::string& message);

    OverworldSim& m_sim;
    SimWalkerRules m_rules;
    std::vector<Interrupt> m_wild;
    std::vector<Interrupt> m_other;
    std::vector<std::string> m_log;
    //  When the last WALK leg's first hold started, on the sim's clock.
    Milliseconds m_leg_start{0};
};


//  Register overworld simulation unit tests.
void add_tests_OverworldSim(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

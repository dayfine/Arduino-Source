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
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What the screen shows, as the walker's detectors see it.
enum class SimScreen{
    OVERWORLD,
    MESSAGE,        //  a white message box: a sign, a trainer
    START_MENU,
    BATTLE_FLASH,   //  a wild battle's opening flashes
    BATTLE_WIPE,    //  black, as a wild battle's wipe ends, before its text box
    BATTLE_OPENING, //  a battle's text box, empty, before the HP boxes
    BATTLE,         //  a battle's text box with the HP boxes
    FADE,           //  a warp's fade out and in
};
//  The human-readable name of a simulated screen.
const char* sim_screen_name(SimScreen screen);

//  What the walker watches the screen with.
enum class SimWatcher{
    FLASH,          //  BattleFlashWatcher: a battle's flashes, and a warp's fade
    BATTLE_SCREEN,  //  BattleScreenWatcher: the battle menu or an HP box
    //  BattleDialogDetector, which the walker used before BattleScreenWatcher:
    //  the battle text box alone, which black below a small room passes for.
    BATTLE_TEXT_BOX,
    MESSAGE,        //  WhiteDialogWatcher
    FADE,           //  BlackScreenWatcher: a warp's fade
};

//  A wild battle on the player's `grass_step`th step onto tall grass (from
//  1), or onto any tile of a map in SimScript::wild_floor_maps. Seen late:
//  its flashes are missed, and only the battle screen shows.
struct SimEncounter{
    int grass_step = 1;
    bool seen_late = false;
};

//  How long after a step's press each thing shows. The walker's own
//  detection delays (OverworldInterrupt::detection_delay()) are its guesses
//  at these.
struct SimTiming{
    Milliseconds tile{267};
    Milliseconds flash{300};
    Milliseconds battle_screen{3700};
    //  A wild battle's text box, before the HP boxes (battle_screen). Not
    //  set: not modelled, the battle screen shows at once.
    std::optional<Milliseconds> battle_opening;
    //  Black, from the end of a wild battle's wipe until its text box
    //  (battle_opening) or HP boxes. Not set: not modelled.
    std::optional<Milliseconds> battle_wipe;
    Milliseconds message{400};
    Milliseconds fade{1000};            //  from the step's end to the new map
    Milliseconds trainer_notice{1000};  //  "!" over the trainer, before they walk up
    Milliseconds trainer_battle{2000};  //  after the trainer's message closes
};

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
};


//  Register overworld simulation unit tests.
void add_tests_OverworldSim(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

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
 *  Not modelled: running, HP and status, and wild battles on the water.
 *  Surf and Fly are (SimField::SURF, SimField::FLY). Wandering people stand on
 *  their tile. The Bicycle is: its pace, its taps (each a tile, some
 *  dropped when they come too fast), SELECT and doors taking the player
 *  off it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSim_H
#define PokemonAutomation_PokemonFRLG_OverworldSim_H

#include <functional>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonFRLG_MovementMechanics.h"
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_BikeSpinMechanics.h"
#include "PokemonFRLG_OverworldBikeTaps.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSimField.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_OverworldSpin.h"
#include "PokemonFRLG_OverworldSimState.h"
#include "PokemonFRLG_OverworldSimScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Simulates overworld movement, encounters and dialogs.
// code-health: allow public_methods -- the Card Key pickup is a story event the row sim reports to the one sim, beside its other inputs
class OverworldSim{
public:
    OverworldSim(std::vector<MapData> maps, MapPosition start, Direction facing, SimScript script = {});

    const SimState& state() const{ return m_state; }

    //  SimScript::hidden_facing_reads.
    size_t hidden_facing_reads() const{ return m_script.hidden_facing_reads; }

    //  Whether the player holds the Card Key (SimScript::card_key_held).
    bool card_key_held() const{ return m_script.card_key_held; }
    //  The player picks up the Card Key: A on a card-key gate opens it from now on.
    void give_card_key(){ m_script.card_key_held = true; }

    //  Whether the walker's screen check reads its last step tap as a
    //  step: one that moved the player from `before`, but for
    //  SimScript::unseen_step_taps, or a turn tap since `turn_taps_before`
    //  (SimState::turn_taps) that SimScript::seen_turn_taps names.
    bool reads_step_tap_as_step(const MapPosition& before, int turn_taps_before);

    //  Holds `direction` for `duration` then lets go for `release`, while
    //  `watchers` watch until `watch` (at least the hold and release) from
    //  the start. Returns the first that saw its screen, as run_until().
    std::optional<SimSeen> hold(
        Direction direction, Milliseconds duration, Milliseconds release, Milliseconds watch,
        const std::vector<SimWatcher>& watchers
    );

    //  A press of `direction` for `duration` (at most MovementMechanics::
    //  LONGEST_TAP), then `release`, as the console measured it: a player
    //  facing another way only turns; one facing that way steps once,
    //  unless the tap starts inside the lockout after the last step tap
    //  (STEP_TAP_LOCKOUT), which the game ignores, as it does any tap
    //  inside the lockout after a battle (BATTLE_EXIT_LOCKOUT); holds
    //  aren't held up by that lockout yet. A tap that starts as
    //  the last one ends, with no release between, holds it on. A turn may
    //  be dropped by fault injection (SimScript::dropped_turn_taps).
    void tap(
        Direction direction,
        Milliseconds duration = MovementMechanics::TURN_TAP,
        Milliseconds release = MovementMechanics::TURN_SETTLE
    );

    //  Watches `watchers` for `duration`, as wait_until().
    std::optional<SimSeen> watch(const std::vector<SimWatcher>& watchers, Milliseconds duration);

    //  Runs from or fights the battle on screen, back to the overworld. A
    //  trainer's battle can't be run from; a battle that isn't there is
    //  waited out (the story's battle loop gives up after 15 s).
    SimBattleEnd handle_battle(SimBattleChoice choice);

    //  Closes the message on screen with B. Returns whether a trainer's
    //  battle follows it.
    bool close_message();

    //  A on what the player faces, as `field` says (`floor`: the lift's).
    //  Returns whether it was there. (PokemonFRLG_OverworldSimField.cpp)
    bool use_field(SimField field, size_t floor = 0);
    //  What stands on the current map now: a pushed boulder is where it was
    //  pushed to, a cut tree is gone.
    const std::vector<MapObject>& objects() const{
        return m_maps[m_map].objects;
    }

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
        //  The way the warp left the player facing (arrival_facing()), if
        //  not the way they moved.
        std::optional<Direction> facing = std::nullopt;
        bool ledge = false;
        //  The step landed on a spinner, which slid the player to `player`.
        std::optional<SpinSlide> spin = {};
    };

    void advance_to(Milliseconds time);
    void show(SimScreen screen, Milliseconds at);
    void schedule(Milliseconds at, SimScreen screen, bool unfreeze);
    bool sees(SimWatcher watcher, SimScreen screen) const;
    std::optional<Milliseconds> first_seen(SimWatcher watcher, Milliseconds from, Milliseconds to) const;
    std::optional<SimSeen> watch_until(const std::vector<SimWatcher>& watchers, Milliseconds from, Milliseconds to);
    //  A tap, for the next one's lockout and to hold on: when it started
    //  and ended, the player's facing before it and the steps it took.
    struct TapPress{
        Direction direction = Direction::NORTH;
        Milliseconds start{0};
        Milliseconds end{0};
        Direction facing_before = Direction::NORTH;
        int steps = 0;
    };
    void walk_hold(Direction direction, Milliseconds duration, int short_by);
    void take_steps(Direction direction, Milliseconds pressed, Milliseconds end, int left);
    void step_tap(Direction direction, Milliseconds duration);
    void hold_on(Direction direction, Milliseconds end);
    void stuck_hold(Direction direction);
    void bump(Direction direction, Milliseconds pressed);
    std::optional<Step> press(Direction direction) const;
    std::optional<Step> take_warp(int map_index, const TilePos& tile) const;
    std::optional<Step> cross_edge(Direction direction) const;
    //  The lift's door to the floor picked, if `map_index` is a lift and
    //  one was picked.
    std::optional<Step> lift_warp(int map_index) const;
    //  A step that landed on a spinner slides on to the slide's end; false
    //  if the slide never stops.
    bool slide_on(Step& step) const;
    //  The slide's time after `step`, and its facing; 0 for no slide.
    Milliseconds after_slide(const Step& step);
    //  What the maps can't say, from code (add_coded_objects()).
    void add_coded_field();
    bool cut_tree();
    bool choose_lift_floor(size_t floor);
    bool push_poster_switch();
    //  SimField::PUSH_BOULDER. (PokemonFRLG_OverworldSimField.cpp)
    bool push_boulder();
    //  A boulder fell through `hole`: the boulder the game hid on the floor
    //  below (a pushable_boulder with a hide_flag) is shown.
    void land_fallen_boulder(const MapPosition& hole);
    bool start_surf();
    //  A on the gate or the statue the player faces; false, as a violation,
    //  if there is none or the Card Key is missing.
    bool open_gate();
    bool press_switch();
    //  Reads each map's gate tiles through the gates of m_state.
    void read_gates();
    bool fly_to_town(size_t town);
    //  The trees cut come back, as after a warp.
    void regrow_cut_trees();
    bool after_step(const Step& step, Milliseconds pressed);
    bool wild_battle(Milliseconds pressed);
    std::optional<SimEncounter> next_encounter();
    bool rolls_encounter(int rate);
    bool drops_turn_tap();
    bool press_select();
    //  Whether the Bicycle tap starting at `start` is dropped, too soon
    //  after the last one.
    bool drops_bike_tap(Milliseconds start);
    //  How long a tile takes: on the water, on the Bicycle or on foot.
    Milliseconds tile_time() const{
        if (m_player.surfing){
            return m_script.timing.surf_tile;
        }
        return m_state.on_bike ? m_script.timing.bike_tile : m_script.timing.tile;
    }
    bool trainer_sees(Milliseconds pressed);
    void update_view();
    void note(const std::string& line);
    void violation(const std::string& line);
    const MapData& map() const{ return m_maps[m_map]; }

    std::vector<MapData> m_maps;
    //  The maps as loaded, with the gates closed or open as the map says.
    std::vector<MapData> m_source_maps;
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
    //  The floor picked on the lift's panel, if any.
    std::optional<size_t> m_lift_floor;
    int m_grass_steps = 0;
    //  The game's wild encounter state (SimScript::encounter_rates).
    std::mt19937 m_encounter_rng;
    int m_steps_since_encounter = 0;
    int m_encounter_rate_buff = 0;
    //  The turn taps so far, for SimScript::dropped_turn_taps.
    //  Steps onto a warp so far, and the fault on one
    //  (SimScript::black_without_warp): its black without the warp.
    int m_warp_steps = 0;
    bool black_without_warp(Milliseconds pressed);
    //  A message after the step on the water just taken
    //  (SimScript::messages_at_surf_steps).
    bool message_on_water(Milliseconds pressed);
    //  The last tap, until a hold.
    std::optional<TapPress> m_last_tap;
    //  When the last Bicycle tap started, and how many came too soon.
    std::optional<Milliseconds> m_last_bike_tap;
    int m_fast_bike_taps = 0;
    //  A hold of `direction` for `duration`: counts the leg a tap on the
    //  Bicycle starts (SimState::bike_legs), and takes the player off the
    //  Bicycle if SimScript::off_bike_at_legs names it.
    void count_bike_leg(Direction direction, Milliseconds duration);
    //  The way of the last Bicycle tap, unless another press came since.
    std::optional<Direction> m_bike_leg_direction;
    //  When the last tap that stepped started.
    std::optional<Milliseconds> m_last_step_tap;
    //  The end of the lockout after the last battle.
    Milliseconds m_battle_lockout_end{0};
    Milliseconds m_wild_battle_from{0};
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
    //  2026-11-03, Mt. Moon 1F, row 04.05).
    bool find_player_after_warp_battle = true;
    //  After a leg's watch saw nothing, a battle's text box on screen (its
    //  HP boxes still to come) is watched on for, and the leg taken as
    //  stopped part-way. Without it the walker took "south 15 (to wall)"
    //  as walked and the next leg into the battle (console B, 2026-11-03,
    //  Mt. Moon B1F, row 04.05).
    bool watch_on_for_unseen_battle = true;
    //  The same after a leg the maps see no wild tiles on (RouteStep::wild),
    //  which the leg and its watch don't watch for battles: on Mt. Moon
    //  B2F's tiles closed on one side, which the maps don't mark as wild, a
    //  battle on "east 7 (to wall)" left black after the leg, the leg was
    //  taken as walked and the next one held from where the battle was
    //  (console B, 2026-11-03, row 04.05).
    bool watch_on_after_unmarked_leg = true;
    //  Before a step, a battle on its way (its text box or wipe) is watched
    //  on for, and the step before it taken as stopped part-way, not the
    //  step held from where the battle left the player.
    bool battle_before_step = true;
    //  A hold over a step onto a connected map is a tile longer for it
    //  (RouteStep::connection_steps). Without it the open leg north from
    //  Cerulean City onto Nugget Bridge ended a tile short (console B,
    //  2026-11-03, rows 05.05 and 05.09).
    bool hold_over_connections = true;
    //  An open leg after whose hold a wild battle was seen is placed by the
    //  battle's first timed screen event (place_late_stop()). Without it the
    //  player may be anywhere on the leg: on Route 10, "east 11" was found
    //  again from all 12 of its tiles (console B, 2026-11-09, 07.02 run 2).
    bool place_late_battle = true;
    //  A message after the hold of an open leg that ends below a sign
    //  (RouteStep::sign_ahead) places the player on its end. Without it
    //  the walker found the player at walls and held the same leg into the
    //  sign 9 times (console B, 2026-11-12, 11.03, Safari Zone West).
    bool place_by_sign = true;
    //  The watch for a warp's fade starts with the hold onto the warp
    //  (Pokemon::WatchAlong), not when it ends. Without it a fade over
    //  before the hold ended was missed, and the walk stopped on "no fade in
    //  and out after walking onto the warp" though it had warped (console B,
    //  2026-11-09 01:41, 07.02, the warp to CeruleanCity: the watch started
    //  about 1.3 s into the hold).
    bool watch_fade_along_hold = true;
    //  A warp's fade is taken as the warp only once the screen after it
    //  shows the player off the map they held from (warp_left_scene()).
    //  Without it a black that cleared with the player where they were was
    //  taken as the warp (console B, 2026-11-10 01:44, 09.01 run 4).
    bool confirm_warp_destination = true;
    //  A warp whose fade was over before the watch for it began is taken by
    //  the screen after it: another scene than before the hold, held still
    //  (OverworldWalkSteps::arrived_without_fade()). Without it the walker
    //  tapped twice into the Department Store lift's back wall and failed
    //  on "no fade in and out after walking onto the warp" with the player
    //  inside the lift (console B, 2026-11-12 00:39, 10.01 lift_to_1f): the
    //  lift room is mostly black, so a watch started after the fade never
    //  sees black end.
    bool scene_confirms_missed_fade = true;
    //  A turn tap is checked by the facing on screen, and tapped again
    //  while the player faces another way (OverworldWalkSteps::turn_to()).
    //  Without it a dropped tap left a one-tile leg turning the player
    //  only, taken as walked (console B, 2026-11-09, 08.01 run 2).
    bool confirm_turn = true;
    //  The speed the walker goes at: its turn taps
    //  (OverworldWalkSteps::turns_first()) and its holds' tile time.
    MoveSpeed speed = MoveSpeed::WALK;
    //  An open leg on the Bicycle is counted out a tap at a time, each a
    //  tile (count_bike_taps()), and one that doesn't move finds the player
    //  ahead of a block. Without it the leg is one hold, and a start off
    //  along the leg ends it off with nothing to show it (console B,
    //  2026-11-09, 08.03: three runs 3 tiles east after Route 8's legs).
    bool count_bike_taps = true;
    //  How far apart, start to start, the counted taps are
    //  (MovementMechanics::BIKE_TAP_PERIOD).
    Milliseconds bike_tap_period = MovementMechanics::BIKE_TAP_PERIOD;
    //  After a warp, which takes the player off the Bicycle, the next leg
    //  where it may be ridden gets on again with SELECT. Without it the
    //  walker held on-foot legs at the Bicycle's pace.
    bool get_back_on_bike = true;
    //  A dark cave's overworld isn't a battle on its way
    //  (OverworldWalkSteps::dark_cave_screen()). Without it the check
    //  before each step watched for a battle on it (console B, 2026-11-09,
    //  08.01: "running from a wild Pokemon", then "not a battle").
    bool dark_cave_not_battle = true;
    //  A stop on an open leg that started in a dark cave may be anywhere on
    //  the leg (whole_leg()). Without it the stop is placed by the dark
    //  screen's black, read as the battle at the hold's start: a battle at
    //  the end of "east 12" on Rock Tunnel B1F was placed 0 to 3 tiles in,
    //  and the player was found again at a wall 5 tiles off (console B,
    //  2026-11-09, 08.01).
    bool whole_leg_in_dark = true;
    //  The end of a trainer's battle the walker fought, as the story's
    //  battle loop takes it on the screen the sim shows after it: its
    //  failure, or empty. By default the battle just ends.
    std::function<std::string(const OverworldSim&)> trainer_battle_end = [](const OverworldSim&){
        return std::string();
    };
};

//  OverworldWalkSteps::turn_to() on `sim`, which reads the facing on the
//  overworld only, but for SimScript::hidden_facing_reads; with `confirm`
//  false, the one tap before it. Returns false where turn_to() would fail
//  the leg (unread_turn_taken()), the tile taken as known.
bool sim_turn_to(OverworldSim& sim, Direction direction, bool confirm = SimWalkerRules{}.confirm_turn);

//  AutoStory's step_once() on `sim`: turns the player if they face another
//  way, then one step tap.
void sim_step_once(OverworldSim& sim, Direction direction);

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
    StepOutcome walk_ridden(const RouteStep& step, std::optional<Direction> facing);
    //  count_bike_taps() on the sim.
    StepOutcome walk_bike_taps(const RouteStep& step);
    BikeTap bike_tap(const RouteStep& step);
    //  count_bike_taps() on foot (counts_step_taps()).
    StepOutcome walk_step_taps(const RouteStep& step);
    BikeTap step_tap(const RouteStep& step, int tap);
    std::optional<WalkedTiles> late_stop(const RouteStep& step);
    //  stop_tiles(); without SimWalkerRules::whole_leg_in_dark, the dark
    //  screen places the stop at the hold's start.
    WalkedTiles dark_stop(const RouteStep& step, WalkedTiles timed) const{
        if (m_leg_dark && !m_rules.whole_leg_in_dark){
            return walked_near(step, 0, MovementMechanics::TILE_TIME);
        }
        return stop_tiles(step, timed, m_leg_dark);
    }
    std::optional<WalkedTiles> walk_step(const RouteStep& step, std::optional<Direction> facing);
    //  start_surf(): turned to the water, A, YES, and the jump onto it.
    //  Nothing: a Surf is never stopped part-way.
    //  (PokemonFRLG_OverworldSimField.cpp)
    std::optional<WalkedTiles> surf(const RouteStep& step);
    //  OPEN_GATE, PRESS_SWITCH: turned to the gate or statue, A (and YES),
    //  as use_field(). (PokemonFRLG_OverworldSimField.cpp)
    std::optional<WalkedTiles> use_gate(const RouteStep& step);
    //  The hold of a WALK step. (PokemonFRLG_OverworldSimField.cpp)
    Pokemon::LegSpec leg_spec(const RouteStep& step) const;
    bool after_warp(const RouteStep& step, int warps_before, Milliseconds watched_from);
    bool fade_over_since(Milliseconds from) const;
    //  after_warp() on a fade seen with no warp: fails, or with
    //  SimWalkerRules::confirm_warp_destination off, takes it as the warp.
    bool black_without_warp(const RouteStep& step, int warps_before, Milliseconds watched_from);
    [[noreturn]] void fail_off_route(const RouteStep& step);
    //  sim_turn_to() for `step`, failing as turn_to() would.
    void turn_or_fail(const RouteStep& step);
    std::optional<SimSeen> hold_watching(const RouteStep& step, Milliseconds hold, const std::vector<Interrupt>& interrupts);
    size_t clear_late(const std::vector<Interrupt>& interrupts, Milliseconds watch, size_t max);
    size_t clear_unseen_battle(const RouteStep& step);
    bool battle_on_its_way() const;
    void clear(const Interrupt& interrupt);
    void hold_to_warp(const RouteStep& step);
    StepEnd watch_for_trainer(const RouteStep& step, int approach_tiles);
    bool moved_past_wall(const RouteStep& step);
    void check_overworld(const RouteStep& step);
    //  After a leg onto a spinner: the slide's time.
    void wait_out_slide(const RouteStep& step);
    const std::vector<Interrupt>& interrupts(const RouteStep& step) const;
    std::vector<SimWatcher> watchers(const std::vector<Interrupt>& interrupts) const;
    [[noreturn]] void fail(const std::string& message);

    OverworldSim& m_sim;
    SimWalkerRules m_rules;
    std::vector<Interrupt> m_wild;
    std::vector<Interrupt> m_other;
    std::vector<std::string> m_log;
    //  When the last WALK leg's first hold started, on the sim's clock,
    //  and whether its map was dark then.
    Milliseconds m_leg_start{0};
    bool m_leg_dark = false;
    //  Whether the walker takes the player to ride the Bicycle, and the
    //  speed of the step under way.
    bool m_riding = false;
    MoveSpeed m_speed = MoveSpeed::WALK;
};


//  Register overworld simulation unit tests.
void add_tests_OverworldSim(UnitTestDatabase& database);
//  Register the tests of a warp's fade over before the hold onto it ends.
void add_tests_OverworldWarpFade(UnitTestDatabase& database);
//  Register the tests of the way a warp leaves the player facing.
void add_tests_OverworldStairs(UnitTestDatabase& database);
//  Register the tests of gated routes, and of the sim opening gates.
void add_tests_OverworldGates(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

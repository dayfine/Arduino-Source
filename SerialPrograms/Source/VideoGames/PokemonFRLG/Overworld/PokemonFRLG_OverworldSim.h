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
 *  Not modelled: Surf, running, HP and status. Wandering people stand on
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
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonFRLG_MovementMechanics.h"
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
    Milliseconds tile = MovementMechanics::TILE_TIME;
    //  A tile on the Bicycle.
    Milliseconds bike_tile = MovementMechanics::BIKE_TILE_TIME;
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
    //  From a battle's end on, the game ignores taps this long.
    Milliseconds battle_exit_lockout = MovementMechanics::BATTLE_EXIT_LOCKOUT;
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
    //  Whether the game holds the player up a tile on a step onto a
    //  connected map. It did north onto Nugget Bridge (console B,
    //  2026-11-03, rows 00.16 and 00.20), not south from Route 6 into
    //  Vermilion City (row 00.24).
    bool connections_stall = true;
    //  Wild battles at the game's own odds instead of `encounters`, on the
    //  maps named here, with their land encounter rate (pokefirered
    //  src/data/wild_encounters.json), rolled from `encounter_seed`.
    std::map<std::string, int> encounter_rates;
    uint32_t encounter_seed = 0;
    //  Fault injection: turn taps (taps while the player faces another way)
    //  the game misses, by number from 1; the player goes on facing the old
    //  way. The game drops none from a standstill (MovementMechanics: 70 of
    //  70 turned); these test the walker's check of the turn
    //  (SimWalkerRules::confirm_turn) after 00.28 run 2 (console B,
    //  2026-11-09), where a one-tile leg only turned the player.
    std::set<int> dropped_turn_taps;
    //  The player rides the Bicycle: a tap facing another way takes a step
    //  too, a hold through a turn loses no tile
    //  (MovementMechanics::BIKE_TAP_ONLY_TURNS), and no step tap is
    //  ignored: a 40ms turn tap and a 56ms hold 340ms after it took two
    //  tiles (console B, 2026-11-09, 00.30). At the start: SELECT gets on
    //  and off (OverworldSim::press_select()), a warp gets off. Each tap is
    //  a tile; one in MovementMechanics::BIKE_FAST_TAP_DROP_EVERY of those
    //  that start sooner than BIKE_TAP_DROP_PERIOD after the last is
    //  dropped. Its pace is timing.bike_tile.
    bool on_bike = false;
    //  The Bicycle is registered (Key Items, REGISTER): SELECT gets on. A
    //  reload of a save made before it was registered loses it.
    bool bike_registered = false;
    //  Caves dark without Flash (Rock Tunnel): the overworld is black but
    //  for a lit circle on the player, which the walker's battle text box
    //  check and screen observer read as a battle on its way (console B,
    //  2026-11-09, 00.28), unless SimWalkerRules::dark_cave_not_battle.
    std::set<std::string> dark_maps;
    //  A lift's map, and where its door lets the player out for each floor
    //  on its panel's menu, top first (pokefirered setdynamicwarp in
    //  RocketHideout_Elevator's scripts): the door's own warp is the
    //  game's placeholder. The hideout's: rocket_hideout_lift_room() under
    //  LIFT_MAP, with rocket_hideout_lift_doors().
    std::map<std::string, std::vector<MapPosition>> lift_floors = {};
    SimTiming timing;
};

//  A screen the sim showed, and when, from its start.
struct SimShown{
    Milliseconds at{0};
    SimScreen screen = SimScreen::OVERWORLD;
};

//  The player and the screen, and what went wrong that the game would show.
struct SimState{
    MapPosition position;
    Direction facing = Direction::SOUTH;
    SimScreen screen = SimScreen::OVERWORLD;
    //  The sim's clock, from its start.
    Milliseconds time{0};
    //  Each screen as it showed, oldest first, from `screen` at the start.
    std::vector<SimShown> shown;
    //  Whether black shows below the map: the player stands near the bottom
    //  of a map with nothing joined to it (a small room or a gate).
    bool black_below = false;
    //  Whether the player's map is one of SimScript::dark_maps.
    bool dark = false;
    int steps = 0;
    int warps = 0;
    int wild_battles = 0;
    //  From each wild battle's step to the overworld after it, together.
    Milliseconds in_wild_battles{0};
    int signs_read = 0;
    int turn_taps_dropped = 0;
    //  On the Bicycle (SimScript::on_bike).
    bool on_bike = false;
    //  Bicycle taps the game dropped, too soon after the last one.
    int bike_taps_dropped = 0;
    //  Step taps the game ignored, inside the lockout after the last one
    //  (MovementMechanics::STEP_TAP_LOCKOUT).
    int step_taps_ignored = 0;
    std::vector<std::string> trainers_beaten;
    //  Things the walker did that the game wouldn't let pass: D-pad holds
    //  under a battle, a trainer's battle fled, a battle fought that wasn't
    //  there.
    std::vector<std::string> violations;
    std::vector<std::string> log;
};

//  A time on the sim's clock as a WallClock, for the shared judges.
WallClock sim_wall_clock(Milliseconds time);

//  The screens `state` showed from `from` until its time, as the walker's
//  screen observer (FrlgScreenObserver) reports them: a message's box, a
//  battle's text box with or without its HP boxes, and the black of a
//  battle's wipe or a warp's fade. A battle's flashes are no uniform
//  screen, and black below a small room, which the observer's battle text
//  box detector may take for a box, isn't modelled.
Pokemon::ScreenEventLog screen_event_log(const SimState& state, Milliseconds from);

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

//  What A does on the overworld, facing something (OverworldSim::use_field()).
enum class SimField{
    //  Cuts the tree the player faces (an object named "cut_tree") with
    //  YES to the game's question: it is gone until the player next warps,
    //  as its hide flag is a FLAG_TEMP.
    CUT_TREE,
    //  The poster the player faces (an object named "poster"): the switch
    //  behind it opens the stairs, which takes away the objects named
    //  "poster_stairs" for good (pokefirered
    //  CeladonCity_GameCorner_EventScript_Poster's setmetatile). The sim
    //  adds both to the Game Corner from code (add_coded_objects()).
    POSTER_SWITCH,
    //  The panel of the lift the player stands in (SimScript::lift_floors):
    //  the floor picked, the lift's door now lets out there.
    LIFT_FLOOR,
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

    //  SELECT on the overworld: with the Bicycle registered, gets on it
    //  where running is allowed, or off it, taking about
    //  MovementMechanics::BIKE_SELECT_TIME. Returns whether it did.
    bool press_select();
    //  REGISTER on the Bicycle in the bag (SimScript::bike_registered).
    void register_bike(){ m_script.bike_registered = true; }

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
    bool after_step(const Step& step, Milliseconds pressed);
    bool wild_battle(Milliseconds pressed);
    std::optional<SimEncounter> next_encounter();
    bool rolls_encounter(int rate);
    bool drops_turn_tap();
    //  Whether the Bicycle tap starting at `start` is dropped, too soon
    //  after the last one.
    bool drops_bike_tap(Milliseconds start);
    //  How long a tile takes: on the Bicycle or on foot.
    Milliseconds tile_time() const{
        return m_state.on_bike ? m_script.timing.bike_tile : m_script.timing.tile;
    }
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
    //  The floor picked on the lift's panel, if any.
    std::optional<size_t> m_lift_floor;
    int m_grass_steps = 0;
    //  The game's wild encounter state (SimScript::encounter_rates).
    std::mt19937 m_encounter_rng;
    int m_steps_since_encounter = 0;
    int m_encounter_rate_buff = 0;
    //  The turn taps so far, for SimScript::dropped_turn_taps.
    int m_turn_taps = 0;
    //  The last tap, until a hold.
    std::optional<TapPress> m_last_tap;
    //  When the last Bicycle tap started, and how many came too soon.
    std::optional<Milliseconds> m_last_bike_tap;
    int m_fast_bike_taps = 0;
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
    //  The watch for a warp's fade starts with the hold onto the warp
    //  (Pokemon::WatchAlong), not when it ends. Without it a fade over
    //  before the hold ended was missed, and the walk stopped on "no fade in
    //  and out after walking onto the warp" though it had warped (console B,
    //  2026-11-09 01:41, 00.27, the warp to CeruleanCity: the watch started
    //  about 1.3 s into the hold).
    bool watch_fade_along_hold = true;
    //  A turn tap is checked by the facing on screen, and tapped again
    //  while the player faces another way (OverworldWalkSteps::turn_to()).
    //  Without it a dropped tap left a one-tile leg turning the player
    //  only, taken as walked (console B, 2026-11-09, 00.28 run 2).
    bool confirm_turn = true;
    //  The speed the walker goes at: its turn taps
    //  (OverworldWalkSteps::turns_first()) and its holds' tile time.
    MoveSpeed speed = MoveSpeed::WALK;
    //  An open leg on the Bicycle is counted out a tap at a time, each a
    //  tile (count_bike_taps()), and one that doesn't move finds the player
    //  ahead of a block. Without it the leg is one hold, and a start off
    //  along the leg ends it off with nothing to show it (console B,
    //  2026-11-09, 00.30: three runs 3 tiles east after Route 8's legs).
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
    //  00.28: "running from a wild Pokemon", then "not a battle").
    bool dark_cave_not_battle = true;
    //  A stop on an open leg that started in a dark cave may be anywhere on
    //  the leg (whole_leg()). Without it the stop is placed by the dark
    //  screen's black, read as the battle at the hold's start: a battle at
    //  the end of "east 12" on Rock Tunnel B1F was placed 0 to 3 tiles in,
    //  and the player was found again at a wall 5 tiles off (console B,
    //  2026-11-09, 00.28).
    bool whole_leg_in_dark = true;
    //  The end of a trainer's battle the walker fought, as the story's
    //  battle loop takes it on the screen the sim shows after it: its
    //  failure, or empty. By default the battle just ends.
    std::function<std::string(const OverworldSim&)> trainer_battle_end = [](const OverworldSim&){
        return std::string();
    };
};

//  OverworldWalkSteps::turn_to() on `sim`, which reads the facing on the
//  overworld only; with `confirm` false, the one tap before it.
void sim_turn_to(OverworldSim& sim, Direction direction, bool confirm = SimWalkerRules{}.confirm_turn);

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
    bool after_warp(const RouteStep& step, int warps_before, Milliseconds watched_from);
    bool fade_over_since(Milliseconds from) const;
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
//  Register the tests of spinner tiles, the lift's panel and the poster
//  switch on made-up maps.
void add_tests_OverworldSpin(UnitTestDatabase& database);
//  Register the tests of the Bicycle's taps and SELECT on a made-up map.
void add_tests_OverworldBike(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

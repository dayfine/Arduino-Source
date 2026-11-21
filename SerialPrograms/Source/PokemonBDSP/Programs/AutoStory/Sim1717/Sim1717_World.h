/*  BDSP AutoStory PR #1717 Simulation Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Drives PR #1717's real ProControllerContext and VideoStream against the
 *  verified 22-map BDSP world with console-measured movement physics,
 *  sub-tile analog joystick motion, wandering NPCs, tall-grass/cave wild
 *  encounters, trainer sightlines, coord-event triggers, Repel steps,
 *  geometric landmark matching, and fault injection matching dev-bdsp's
 *  FaultSweep_Tests plus analog-stick timing variance sweeps.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_World_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_World_H

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Controllers/ControllerTypes.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_OverworldMatchers.h"
#include "Sim1717_Frames.h"
#include "Sim1717_Hooks.h"
#include "Sim1717_Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

//  Fault injection & physics configuration (parity with dev-bdsp FaultSweep
//  + analog-stick timing & matcher reliability sweeps).
struct Sim1717FaultKnobs{
    //  Physics calibration (console-measured BDSP_MOVE_MECHANICS defaults).
    double walk_ms_across = 160.0;
    double walk_ms_up_down = 240.0;
    double run_ms_across = 100.0;
    double run_ms_up_down = 150.0;
    double stick_start_delay_ms = 0.0;  //  Measured delay before stick motion begins.

    //  Global speed multiplier (1.0 = nominal; 0.95 = 5% slower; 1.05 = 5% faster).
    double speed_scale = 1.0;

    //  Per-joystick-hold random duration jitter std-dev in ms (0 = deterministic).
    double hold_jitter_ms = 0.0;

    //  Probability a single D-pad tap drops (lost input).
    double dpad_drop_prob = 0.0;

    //  Wild encounters:
    //  grass_every > 0: deterministic encounter every N tall-grass tiles (8 = dev-bdsp GRASS_BATTLES).
    int grass_every = 0;
    //  cave_every > 0: deterministic encounter every N cave tiles when Repel is inactive.
    int cave_every = 0;
    //  Stochastic encounter probability per grass/cave tile (used in Monte Carlo when > 0).
    double grass_prob = 0.0;
    double cave_prob = 0.0;

    //  Warp fade faults:
    //  Extra fade-in duration after warps (SLOW_FADE_IN). Nominal fade = 600ms.
    int fade_duration_ms = 600;
    int slow_fade_in_extra_ms = 0;
    //  If >= 0, suppress the Nth black/white warp fade completely (MISSED_FADE).
    int missed_fade_index = -1;
    //  If >= 0, suppress the Nth Town Banner completely (MISSED_BANNER).
    int missed_banner_index = -1;

    //  Battle opening delay (ms of black/dialog before battle menu appears).
    int battle_opening_delay_ms = 0;

    //  Scene end pose jitter: shift player by (dx, dy) tiles after scenes/battles (SCENE_END_POSE).
    int scene_end_shift_x = 0;
    int scene_end_shift_y = 0;
    std::string scene_end_target_event; //  Empty = apply to all non-trivial scenes, or named event.

    //  Scene early trigger: expand coord event triggers by N tiles outward (SCENE_EARLY).
    int scene_early_expand = 0;

    //  Wandering NPCs:
    bool enable_wanderers = false;
    uint64_t wanderer_seed = 1;

    //  Landmark matcher faults:
    double landmark_dropout_prob = 0.0; //  Probability a single frame fails to locate landmark.
    double landmark_noise_tiles = 0.0;  //  Position readout noise in tiles.

    //  Random seed for all stochastic knobs (Monte Carlo).
    uint64_t rng_seed = 42;

    //  Virtual-time watchdog per run to detect unbounded while-loop hangs.
    int64_t max_sim_time_ms = 600'000;

    //  Verbose trace logging of movement and events.
    bool trace = false;
};

struct Sim1717State{
    std::string map;
    //  Continuous position in tiles (tile (x, y) center is at (x, y); integer = on-grid).
    double x = 0.0;
    double y = 0.0;
    SimDir facing = SimDir::SOUTH;

    bool has_running_shoes = false;
    bool has_repels = false;
    int repel_steps = 0;
    bool repel_expiring = false;
    bool has_escape_rope = false;
    bool ravaged_rock_smashed = false;

    //  Story progression flags & triggered events.
    std::set<std::string> fired_events;
    std::set<std::string> defeated_trainers;
    std::map<std::string, SimTile> trainer_moved_pos; //  "map:npc_name" -> post-battle standing tile
    std::set<std::string> collected_items;
    std::set<std::string> coupons;

    //  Screen state machine.
    SimScreenKind screen = SimScreenKind::OVERWORLD;
    int64_t screen_timer_ms = 0;        //  Countdown for temporary states (fade, banner, dialog).
    int64_t ledge_lock_ms = 0;          //  BDSP ledge-hop animation lock (~450ms).
    SimScreenKind screen_after_timer = SimScreenKind::OVERWORLD;
    int dialog_lines_left = 0;          //  Number of A/B presses to finish current dialog.
    bool dialog_has_prompt = false;     //  If true, last dialog line shows Yes/No prompt.
    bool prompt_yes_selected = true;    //  Current cursor on Yes/No prompt.
    std::string active_event;           //  Name of currently active dialog/battle/scene.
    std::string active_trainer;         //  Name of active trainer during trainer battle.
    bool in_battle = false;
    bool in_wild_battle = false;
    bool in_trainer_battle = false;
    bool in_starly_battle = false;
    int battle_cursor_row = 0;          //  0=FIGHT, 1=POKEMON, 2=BAG, 3=RUN.
    bool menu_open = false;

    //  Queued post-dialog script transitions (e.g., Rowan -> Home fade, Lake exit fade).
    bool pending_post_dialog_fade = false;
    std::string pending_warp_map;
    SimTile pending_warp_tile{0, 0};
    SimDir pending_warp_facing = SimDir::SOUTH;
    bool pending_post_warp_dialog = false;
    std::string pending_post_warp_event;
    int pending_post_warp_lines = 2;

    //  Banner overlay state (shows briefly after map border crossing, with realistic slide-in delay).
    int64_t pending_banner_ms = 0;
    int64_t banner_timer_ms = 0;
    int catch_demo_presses = 0;
    int fades_emitted = 0;
    int banners_emitted = 0;

    //  Counters & telemetry.
    int64_t sim_time_ms = 0;
    int wild_battles = 0;
    int trainer_battles = 0;
    int warps_taken = 0;
    int joystick_calls = 0;
    int dpad_calls = 0;
    int grass_steps = 0;
    int cave_steps = 0;
    int wall_collisions = 0;
    int off_grid_door_snags = 0;
    int fade_in_moves = 0;              //  Inputs sent while fade was still active!
    std::vector<std::string> trace_log;
};

struct Sim1717WandererState{
    std::string map;
    std::string name;
    SimWanderBox box;
    SimTile pos;
    uint64_t rng = 0;
    int64_t elapsed_ms = 0;
};

class Sim1717Engine;

//  Virtual ProController that drives Sim1717Engine instead of hardware.
class Sim1717ProController final : public ProController{
public:
    Sim1717ProController(Logger& logger, Sim1717Engine& engine);

    virtual Logger& logger() override{ return m_logger; }
    virtual bool is_ready() const override{ return true; }
    virtual ControllerPerformanceClass performance_class() const override{
        return ControllerPerformanceClass::SerialPABotBase_Wired;
    }
    virtual Milliseconds ticksize() const override{ return Milliseconds(8); }
    virtual Milliseconds cooldown() const override{ return Milliseconds(8); }
    virtual Milliseconds timing_variation() const override{ return Milliseconds(0); }
    virtual bool atomic_multibutton() const override{ return true; }

    virtual void cancel_all_commands() override{}
    virtual void replace_on_next_command() override{}
    virtual bool cancel_all_and_wait(Milliseconds) override{ return true; }
    virtual void wait_for_all(Cancellable* cancellable) override;

    virtual void issue_barrier(Cancellable* cancellable) override{}
    virtual void issue_nop(Cancellable* cancellable, Milliseconds duration) override;
    virtual void issue_buttons(Cancellable* cancellable, Milliseconds delay, Milliseconds hold, Milliseconds cooldown, Button button) override;
    virtual void issue_dpad(Cancellable* cancellable, Milliseconds delay, Milliseconds hold, Milliseconds cooldown, DpadPosition position) override;
    virtual void issue_left_joystick(Cancellable* cancellable, Milliseconds delay, Milliseconds hold, Milliseconds cooldown, const JoystickPosition& position) override;
    virtual void issue_right_joystick(Cancellable* cancellable, Milliseconds delay, Milliseconds hold, Milliseconds cooldown, const JoystickPosition& position) override;
    virtual void issue_gyro_accel_x(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_gyro_accel_y(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_gyro_accel_z(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_gyro_rotate_x(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_gyro_rotate_y(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_gyro_rotate_z(Cancellable*, Milliseconds, Milliseconds, Milliseconds, int16_t) override{}
    virtual void issue_full_controller_state(Cancellable* cancellable, bool enable_logging, Milliseconds hold, Button button, DpadPosition position, const JoystickPosition& left_joystick, const JoystickPosition& right_joystick) override;
    virtual void issue_mash_button(Cancellable* cancellable, Milliseconds duration, Button button, Milliseconds delay, Milliseconds hold, Milliseconds cooldown) override;
    virtual void issue_mash_button(Cancellable* cancellable, Milliseconds duration, Button button0, Button button1) override;
    virtual void issue_mash_AZs(Cancellable* cancellable, Milliseconds duration) override;
    virtual void issue_system_scroll(Cancellable* cancellable, Milliseconds delay, Milliseconds hold, Milliseconds cooldown, DpadPosition direction) override;

private:
    Logger& m_logger;
    Sim1717Engine& m_engine;
};

//  Virtual VideoFeed returning current synthetic frame stamped with virtual WallClock.
class Sim1717VideoFeed final : public VideoFeed{
public:
    explicit Sim1717VideoFeed(Sim1717Engine& engine) : m_engine(engine){}

    virtual void add_frame_listener(VideoFrameListener&) override{}
    virtual void remove_frame_listener(VideoFrameListener&) override{}
    virtual void reset() override{}
    virtual VideoSnapshot snapshot_latest_blocking() override;
    virtual VideoSnapshot snapshot_recent_nonblocking(WallClock min_time) override;
    virtual double fps_source() const override{ return 60.0; }
    virtual double fps_display() const override{ return 60.0; }

private:
    Sim1717Engine& m_engine;
};

//  Main simulation engine implementing Sim1717Hooks.
class Sim1717Engine final : public Sim1717Hooks{
public:
    Sim1717Engine(const Sim1717MapDB& db, Sim1717FaultKnobs knobs = {});

    void reset(const std::string& map, double x, double y, SimDir facing, bool has_running_shoes = false);
    Sim1717State& state(){ return m_state; }
    const Sim1717State& state() const{ return m_state; }
    const Sim1717FaultKnobs& knobs() const{ return m_knobs; }
    const Sim1717MapDB& db() const{ return m_db; }

    VideoSnapshot current_snapshot();

    //  Called by Sim1717ProController.
    void step_wait(Cancellable* cancellable, Milliseconds duration);
    void step_button(Cancellable* cancellable, Button button, Milliseconds hold, Milliseconds cooldown);
    void step_dpad(Cancellable* cancellable, DpadPosition pos, Milliseconds hold, Milliseconds cooldown);
    void step_joystick(Cancellable* cancellable, double jx, double jy, Milliseconds hold, Milliseconds cooldown);
    void step_mash(Cancellable* cancellable, Button button, Milliseconds duration);

    //  Sim1717Hooks overrides.
    virtual std::chrono::steady_clock::time_point steady_now() override;
    virtual WallClock wall_now() override;
    virtual int wait_until(
        VideoStream& stream, CancellableScope& scope, Milliseconds timeout,
        const std::vector<PeriodicInferenceCallback>& callbacks, Milliseconds default_video_period
    ) override;
    virtual int run_until(
        VideoStream& stream, CancellableScope& scope,
        std::function<void(CancellableScope&)>&& command,
        const std::vector<PeriodicInferenceCallback>& callbacks, Milliseconds default_video_period
    ) override;

    virtual std::optional<OverworldMatchResult> match_overworld(const char* matcher_name) override;
    virtual std::optional<Sim1717MineCenterOffset> mine_center_offset() override;
    virtual std::optional<Sim1717MinePictureScore> mine_picture_score(const char* filename) override;
    virtual std::optional<Sim1717MineRoarkScore> mine_roark_score() override;

    virtual bool on_set_time() override;
    virtual bool on_verify_settings() override;
    virtual bool on_fight_starly() override;
    virtual bool on_fight_trainer(const std::string& label) override;
    virtual bool on_catch_1_pokemon() override;
    virtual bool on_battle_1_pokemon() override;
    virtual bool on_use_potion(uint8_t* level) override;
    virtual bool on_buy_items() override;
    virtual bool on_activate_repel() override;
    virtual bool on_use_escape_rope() override;
    virtual bool on_heal_pokemon(const std::string& label, bool exit_center) override;
    virtual bool on_use_hm(const std::string& label) override;
    virtual bool on_checkpoint_save() override;
    virtual bool on_reload_backup_save() override;
    virtual bool on_select_starter(size_t right_presses) override;

private:
    struct ActiveWatch{
        CancellableScope* scope = nullptr;
        const std::vector<PeriodicInferenceCallback>* callbacks = nullptr;
        int triggered_index = -1;
    };

    void advance_time_ms(int64_t ms, Cancellable* cancellable);
    void poll_active_watches();
    bool can_stand_on(const std::string& map_name, const SimTile& tile) const;
    bool wanderer_at(const std::string& map_name, const SimTile& tile) const;
    void advance_wanderers(int64_t ms);
    void move_continuous(double vx, double vy, int64_t dt_ms, bool running);
    void step_one_tile_dpad(SimDir dir);
    void on_tile_entered(const SimTile& tile, SimDir dir);
    void trigger_warp(const SimWarpRule& warp);
    void start_fade(bool white, int64_t duration_ms, SimScreenKind after);
    void start_dialog(const std::string& event, int lines, bool has_prompt = false, bool default_yes = true);
    void start_wild_battle(const std::string& reason);
    void start_trainer_battle(const std::string& name);
    void apply_scene_end_shift(const std::string& event_name);
    void handle_press_a();
    void handle_press_b();
    void handle_press_x();
    void record(const std::string& msg);
    double rand_unit();
    double rand_normal();

    const Sim1717MapDB& m_db;
    Sim1717FaultKnobs m_knobs;
    Sim1717State m_state;
    std::vector<Sim1717WandererState> m_wanderers;
    std::vector<ActiveWatch*> m_watch_stack;
    uint64_t m_rng = 1;
    std::chrono::steady_clock::time_point m_steady_epoch;
    WallClock m_wall_epoch;
    int64_t m_snapshot_seq = 0;
};

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

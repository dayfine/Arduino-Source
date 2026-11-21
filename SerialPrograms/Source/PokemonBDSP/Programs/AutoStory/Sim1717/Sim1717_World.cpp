/*  BDSP AutoStory PR #1717 Simulation Engine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <stdexcept>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Sim1717_World.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

constexpr double PI = std::numbers::pi;
//  Player collision half-box in tiles (matches Unity capsule & off-grid doorway snagging).
constexpr double PLAYER_HALF_BOX = 0.20;

//  Maps a DpadPosition enum to SimDir.
std::optional<SimDir> dpad_to_dir(DpadPosition pos){
    switch (pos){
    case DPAD_UP:    return SimDir::NORTH;
    case DPAD_RIGHT: return SimDir::EAST;
    case DPAD_DOWN:  return SimDir::SOUTH;
    case DPAD_LEFT:  return SimDir::WEST;
    default:         return std::nullopt;
    }
}

//  Maps a joystick vector to its dominant SimDir.
SimDir vec_to_dir(double jx, double jy){
    if (std::abs(jx) >= std::abs(jy)){
        return jx >= 0.0 ? SimDir::EAST : SimDir::WEST;
    }
    return jy >= 0.0 ? SimDir::NORTH : SimDir::SOUTH;
}

bool is_standable_tile(SimTileKind k){
    return k == SimTileKind::OPEN || k == SimTileKind::TALL_GRASS;
}

bool ledge_matches(SimTileKind k, SimDir d){
    return (k == SimTileKind::LEDGE_NORTH && d == SimDir::NORTH) ||
           (k == SimTileKind::LEDGE_SOUTH && d == SimDir::SOUTH) ||
           (k == SimTileKind::LEDGE_EAST  && d == SimDir::EAST)  ||
           (k == SimTileKind::LEDGE_WEST  && d == SimDir::WEST);
}

uint32_t lcg_u32(uint64_t& state){
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    return static_cast<uint32_t>(state >> 32);
}

//  OreburghMine picture stage target tiles on OreburghMine.json (32x32).
//  Each entry matches the exact position along Butters' 19-stage mine route.
struct MineStageTarget{
    const char* filename;
    double tx;
    double ty;
    double radius_x;
    double radius_y;
};

const std::vector<MineStageTarget>& mine_stage_targets(){
    static const std::vector<MineStageTarget> targets = {
        //  mine_path_01: standing pressed against the central rock (15,10) at (15, 9.22),
        //  clear of wall (16, 8) so moving right onto (16, 9) succeeds.
        {"mine_path_01.png",        15.0,  9.22, 0.55, 0.25},
        {"mine_path_01_1.png",      16.0,  9.22, 0.22, 0.35},
        {"mine_path_01_2.png",      17.0,  9.22, 0.22, 0.35},
        {"mine_path_02_1.png",      16.0, 11.22, 0.55, 0.30},
        {"mine_path_02_2.png",      17.0, 11.22, 0.55, 0.30},
        {"mine_path_03.png",        14.5, 11.22, 0.65, 0.45},
        {"mine_path_05.png",        15.0, 17.00, 1.20, 0.65},
        {"mine_path_06.png",         4.3, 17.00, 0.85, 0.75},
        {"mine_path_07.png",         4.3, 20.00, 0.85, 0.90},
        {"mine_path_08.png",         2.1, 20.00, 0.20, 0.85},
        {"mine_path_09.png",         2.1, 24.20, 0.35, 0.22},
        {"mine_path_10.png",         4.9, 24.20, 0.22, 0.35},
        {"mine_path_11.png",         4.9, 26.50, 0.35, 0.50},
        {"mine_path_12.png",         5.1, 26.50, 0.35, 0.50},
        {"mine_path_13.png",         5.1, 28.00, 0.35, 0.35},
        {"mine_path_14.png",         4.8, 28.00, 0.40, 0.35},
        {"mine_path_15_actual.png",  7.0, 28.00, 0.35, 0.35},
        {"mine_path_16.png",         7.0, 29.10, 0.40, 0.28},
    };
    return targets;
}

} // namespace

// ============================================================================
//  Sim1717ProController
// ============================================================================

Sim1717ProController::Sim1717ProController(Logger& logger, Sim1717Engine& engine)
    : ProController(logger)
    , m_logger(logger)
    , m_engine(engine)
{}

void Sim1717ProController::wait_for_all(Cancellable* cancellable){
    if (cancellable){
        cancellable->throw_if_cancelled();
    }
}

void Sim1717ProController::issue_nop(Cancellable* cancellable, Milliseconds duration){
    m_engine.step_wait(cancellable, duration);
}

void Sim1717ProController::issue_buttons(
    Cancellable* cancellable,
    Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
    Button button
){
    //  Note: pbf_press_button(ctx, btn, hold, release) passes (delay=hold+release, hold=hold, cooldown=...)
    //  Wait: let's check how pbf_press_button maps arguments!
    //  In NintendoSwitch_Commands_PushButtons.cpp:
    //    issue_buttons(cancellable, delay = hold + release, hold = hold, cooldown = 0 or 24ms, button)
    //  So total elapsed time for the command is max(delay, hold) (or hold + release = delay).
    const Milliseconds release = delay > hold ? (delay - hold) : cooldown;
    m_engine.step_button(cancellable, button, hold, release);
}

void Sim1717ProController::issue_dpad(
    Cancellable* cancellable,
    Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
    DpadPosition position
){
    const Milliseconds release = delay > hold ? (delay - hold) : cooldown;
    m_engine.step_dpad(cancellable, position, hold, release);
}

void Sim1717ProController::issue_left_joystick(
    Cancellable* cancellable,
    Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
    const JoystickPosition& position
){
    const Milliseconds release = delay > hold ? (delay - hold) : cooldown;
    m_engine.step_joystick(cancellable, position.x, position.y, hold, release);
}

void Sim1717ProController::issue_right_joystick(
    Cancellable* cancellable,
    Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
    const JoystickPosition&
){
    const Milliseconds total = std::max(delay, hold + cooldown);
    m_engine.step_wait(cancellable, total);
}

void Sim1717ProController::issue_full_controller_state(
    Cancellable* cancellable,
    bool,
    Milliseconds hold,
    Button button,
    DpadPosition position,
    const JoystickPosition& left_joystick,
    const JoystickPosition&
){
    if (std::hypot(left_joystick.x, left_joystick.y) > 0.2){
        m_engine.step_joystick(cancellable, left_joystick.x, left_joystick.y, hold, Milliseconds(0));
    }else if (position != DPAD_NONE){
        m_engine.step_dpad(cancellable, position, hold, Milliseconds(0));
    }else if (button != BUTTON_NONE){
        m_engine.step_button(cancellable, button, hold, Milliseconds(0));
    }else{
        m_engine.step_wait(cancellable, hold);
    }
}

void Sim1717ProController::issue_mash_button(
    Cancellable* cancellable,
    Milliseconds duration,
    Button button,
    Milliseconds, Milliseconds, Milliseconds
){
    m_engine.step_mash(cancellable, button, duration);
}

void Sim1717ProController::issue_mash_button(
    Cancellable* cancellable,
    Milliseconds duration,
    Button button0,
    Button
){
    m_engine.step_mash(cancellable, button0, duration);
}

void Sim1717ProController::issue_mash_AZs(Cancellable* cancellable, Milliseconds duration){
    m_engine.step_mash(cancellable, BUTTON_A, duration);
}

void Sim1717ProController::issue_system_scroll(
    Cancellable* cancellable,
    Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
    DpadPosition direction
){
    const Milliseconds release = delay > hold ? (delay - hold) : cooldown;
    m_engine.step_dpad(cancellable, direction, hold, release);
}

// ============================================================================
//  Sim1717VideoFeed
// ============================================================================

VideoSnapshot Sim1717VideoFeed::snapshot_latest_blocking(){
    return m_engine.current_snapshot();
}

VideoSnapshot Sim1717VideoFeed::snapshot_recent_nonblocking(WallClock){
    return m_engine.current_snapshot();
}

// ============================================================================
//  Sim1717Engine
// ============================================================================

Sim1717Engine::Sim1717Engine(const Sim1717MapDB& db, Sim1717FaultKnobs knobs)
    : m_db(db)
    , m_knobs(std::move(knobs))
    , m_rng(m_knobs.rng_seed ? m_knobs.rng_seed : 1)
    , m_steady_epoch(std::chrono::steady_clock::now())
    , m_wall_epoch(current_time())
{}

void Sim1717Engine::reset(
    const std::string& map_name, double x, double y, SimDir facing, bool has_running_shoes
){
    m_state = Sim1717State{};
    m_state.map = map_name;
    m_state.x = x;
    m_state.y = y;
    m_state.facing = facing;
    m_state.has_running_shoes = has_running_shoes;
    m_state.screen = (map_name == "RavagedPath") ? SimScreenKind::OVERWORLD_CAVE : SimScreenKind::OVERWORLD;
    m_rng = m_knobs.rng_seed ? m_knobs.rng_seed : 1;
    m_snapshot_seq = 0;

    //  Initialize wandering NPCs from maps.
    m_wanderers.clear();
    if (m_knobs.enable_wanderers){
        uint64_t wseed = m_knobs.wanderer_seed ? m_knobs.wanderer_seed : 1;
        for (const std::string& mname : {
            "TwinleafTown", "Route201", "SandgemTown", "Route202", "JubilifeCity",
            "Route203", "OreburghCity", "OreburghMine", "Route204South", "Route204North", "FloaromaTown"
        }){
            if (!m_db.has_map(mname)){
                continue;
            }
            for (const auto& npc : m_db.map(mname).npcs){
                if (npc.wander){
                    Sim1717WandererState ws;
                    ws.map = mname;
                    ws.name = npc.name;
                    ws.box = *npc.wander;
                    ws.pos = npc.pos;
                    ws.rng = wseed * 2654435761ULL + 17;
                    wseed++;
                    //  Warm up initial position from seed so each seed tests a distinct starting tile.
                    for (int k = 0; k < 5; k++){
                        uint32_t ch = lcg_u32(ws.rng) % 5;
                        if (ch < 4){
                            SimTile nxt = ws.pos.next(static_cast<SimDir>(ch));
                            if (nxt.x >= ws.box.x0 && nxt.x <= ws.box.x1 && nxt.y >= ws.box.y0 && nxt.y <= ws.box.y1){
                                ws.pos = nxt;
                            }
                        }
                    }
                    m_wanderers.push_back(std::move(ws));
                }
            }
        }
    }
}

double Sim1717Engine::rand_unit(){
    return (double)lcg_u32(m_rng) / 4294967296.0;
}

double Sim1717Engine::rand_normal(){
    //  Box-Muller transform.
    const double u1 = std::max(1e-9, rand_unit());
    const double u2 = rand_unit();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * u2);
}

void Sim1717Engine::record(const std::string& msg){
    if (m_knobs.trace){
        m_state.trace_log.push_back(std::format("[{:6d}ms {} ({:.2f},{:.2f}) {}] {}",
            m_state.sim_time_ms, m_state.map, m_state.x, m_state.y, sim_dir_name(m_state.facing), msg));
    }
}

std::chrono::steady_clock::time_point Sim1717Engine::steady_now(){
    return m_steady_epoch + std::chrono::milliseconds(m_state.sim_time_ms);
}

WallClock Sim1717Engine::wall_now(){
    return m_wall_epoch + std::chrono::milliseconds(m_state.sim_time_ms);
}

VideoSnapshot Sim1717Engine::current_snapshot(){
    //  Ensure each snapshot has a strictly advancing timestamp if polled in a loop.
    m_snapshot_seq++;
    SimScreenKind kind = m_state.screen;
    if (kind == SimScreenKind::OVERWORLD){
        if (m_state.banner_timer_ms > 0){
            kind = SimScreenKind::OVERWORLD_WITH_BANNER;
        }else if (m_state.map == "RavagedPath"){
            kind = SimScreenKind::OVERWORLD_CAVE;
        }
    }
    VideoSnapshot snap;
    snap.frame = sim1717_frame(kind);
    snap.timestamp = wall_now() + std::chrono::microseconds(m_snapshot_seq);
    return snap;
}

void Sim1717Engine::advance_wanderers(int64_t ms){
    if (!m_knobs.enable_wanderers){
        return;
    }
    const SimTile player_tile{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};
    for (auto& w : m_wanderers){
        if (w.map != m_state.map){
            continue;
        }
        w.elapsed_ms += ms;
        while (w.elapsed_ms >= 2000){
            w.elapsed_ms -= 2000;
            const uint32_t ch = lcg_u32(w.rng) % 5;
            if (ch >= 4){
                continue;
            }
            const SimTile nxt = w.pos.next(static_cast<SimDir>(ch));
            if (nxt.x < w.box.x0 || nxt.x > w.box.x1 || nxt.y < w.box.y0 || nxt.y > w.box.y1){
                continue;
            }
            if (nxt == player_tile){
                continue;
            }
            if (!is_standable_tile(m_db.map(w.map).at(nxt))){
                continue;
            }
            w.pos = nxt;
        }
    }
}

bool Sim1717Engine::wanderer_at(const std::string& map_name, const SimTile& tile) const{
    if (!m_knobs.enable_wanderers){
        return false;
    }
    for (const auto& w : m_wanderers){
        if (w.map == map_name && w.pos == tile){
            return true;
        }
    }
    return false;
}

bool Sim1717Engine::can_stand_on(const std::string& map_name, const SimTile& tile) const{
    const SimMapData& m = m_db.map(map_name);
    if (!m.contains(tile)){
        return false;
    }
    //  Smashed rock in RavagedPath (either (22,42) or (22,43) along the rock barrier).
    if (map_name == "RavagedPath" && m_state.ravaged_rock_smashed &&
        (tile == SimTile{22, 42} || tile == SimTile{22, 43})){
        return true;
    }
    const SimTileKind k = m.at(tile);
    if (!is_standable_tile(k)){
        return false;
    }
    //  Check static NPCs (non-wandering).
    for (const auto& npc : m.npcs){
        if (npc.wander){
            continue;
        }
        //  Special: Mom in PlayerHouse1F is at (7, 7) from 00.04 on (r43), otherwise (7, 8).
        if (map_name == "PlayerHouse1F" && npc.name == "mom"){
            continue;
        }
        //  Special: Poketch president leaves (46, 35) after awarding the Poketch in 00.06.
        if (map_name == "JubilifeCity" && npc.name == "poketch_president" &&
            m_state.fired_events.contains("poketch_received")){
            continue;
        }
        //  Special: cyrus_3 at FloaromaTown (11, 26) is not present on first arrival in BDSP.
        if (map_name == "FloaromaTown" && npc.name == "cyrus_3"){
            continue;
        }
        SimTile npc_tile = npc.pos;
        if (auto it = m_state.trainer_moved_pos.find(map_name + ":" + npc.name); it != m_state.trainer_moved_pos.end()){
            npc_tile = it->second;
        }
        if (npc_tile == tile){
            return false;
        }
    }
    //  Items on ground block walking onto them until collected.
    for (const auto& it : m.items){
        if (it.pos == tile && !m_state.collected_items.contains(map_name + ":" + it.item)){
            return false;
        }
    }
    if (wanderer_at(map_name, tile)){
        return false;
    }
    return true;
}

void Sim1717Engine::poll_active_watches(){
    if (m_watch_stack.empty()){
        return;
    }
    const VideoSnapshot snap = current_snapshot();
    for (auto it = m_watch_stack.rbegin(); it != m_watch_stack.rend(); ++it){
        ActiveWatch* w = *it;
        if (w->triggered_index >= 0 || !w->callbacks){
            continue;
        }
        for (size_t c = 0; c < w->callbacks->size(); c++){
            const PeriodicInferenceCallback& cb = (*w->callbacks)[c];
            if (!cb.callback || cb.callback->type() != InferenceType::VISUAL){
                continue;
            }
            auto& vcb = static_cast<VisualInferenceCallback&>(*cb.callback);
            if (vcb.process_frame(*snap.frame, snap.timestamp)){
                w->triggered_index = (int)c;
                if (w->scope){
                    w->scope->cancel(nullptr);
                }
                return;
            }
        }
    }
}

void Sim1717Engine::advance_time_ms(int64_t ms, Cancellable* cancellable){
    if (ms <= 0){
        poll_active_watches();
        if (cancellable){
            cancellable->throw_if_cancelled();
        }
        return;
    }
    int64_t rem = ms;
    while (rem > 0){
        if (cancellable){
            cancellable->throw_if_cancelled();
        }
        const int64_t dt = std::min<int64_t>(rem, 50);
        m_state.sim_time_ms += dt;
        rem -= dt;
        if (m_knobs.max_sim_time_ms > 0 && m_state.sim_time_ms > m_knobs.max_sim_time_ms){
            throw std::runtime_error(std::format(
                "Sim1717: Infinite loop / hang detected (>{}s virtual time) at {} ({:.2f},{:.2f})",
                m_knobs.max_sim_time_ms / 1000, m_state.map, m_state.x, m_state.y));
        }

        advance_wanderers(dt);

        if (m_state.pending_banner_ms > 0){
            m_state.pending_banner_ms -= dt;
            if (m_state.pending_banner_ms <= 0){
                m_state.pending_banner_ms = 0;
                m_state.banner_timer_ms = 2200;
            }
        }else if (m_state.banner_timer_ms > 0){
            m_state.banner_timer_ms = std::max<int64_t>(0, m_state.banner_timer_ms - dt);
        }
        if (m_state.ledge_lock_ms > 0){
            m_state.ledge_lock_ms = std::max<int64_t>(0, m_state.ledge_lock_ms - dt);
        }

        if (m_state.screen_timer_ms > 0){
            m_state.screen_timer_ms -= dt;
            if (m_state.screen_timer_ms <= 0){
                m_state.screen_timer_ms = 0;
                const SimScreenKind next_screen = m_state.screen_after_timer;
                m_state.screen = next_screen;
                if (m_state.pending_post_warp_dialog && next_screen == SimScreenKind::OVERWORLD){
                    m_state.pending_post_warp_dialog = false;
                    start_dialog(m_state.pending_post_warp_event, m_state.pending_post_warp_lines);
                }
            }
        }

        poll_active_watches();
        if (cancellable){
            cancellable->throw_if_cancelled();
        }
    }
}

void Sim1717Engine::start_fade(bool white, int64_t duration_ms, SimScreenKind after){
    const int idx = m_state.fades_emitted++;
    if (m_knobs.missed_fade_index == idx){
        record(std::format("FAULT MISSED_FADE #{} suppressed!", idx));
        m_state.screen = after;
        m_state.screen_timer_ms = 0;
        if (m_state.pending_post_warp_dialog && after == SimScreenKind::OVERWORLD){
            m_state.pending_post_warp_dialog = false;
            start_dialog(m_state.pending_post_warp_event, m_state.pending_post_warp_lines);
        }
        return;
    }
    m_state.screen = white ? SimScreenKind::WHITE_SCREEN : SimScreenKind::BLACK_SCREEN;
    m_state.screen_timer_ms = duration_ms + m_knobs.slow_fade_in_extra_ms;
    m_state.screen_after_timer = after;
    poll_active_watches();
}

void Sim1717Engine::start_dialog(const std::string& event, int lines, bool has_prompt, bool default_yes){
    m_state.active_event = event;
    m_state.dialog_lines_left = std::max(1, lines);
    m_state.dialog_has_prompt = has_prompt;
    m_state.prompt_yes_selected = default_yes;
    m_state.screen = (has_prompt && m_state.dialog_lines_left == 1)
        ? (default_yes ? SimScreenKind::SHORT_DIALOG_PROMPT_YES : SimScreenKind::SHORT_DIALOG_PROMPT_NO)
        : SimScreenKind::SHORT_DIALOG;
    m_state.screen_timer_ms = 0;
    record("DIALOG START: " + event);
    poll_active_watches();
}

void Sim1717Engine::start_wild_battle(const std::string& reason){
    m_state.wild_battles++;
    m_state.in_battle = true;
    m_state.in_wild_battle = true;
    m_state.in_trainer_battle = false;
    m_state.battle_cursor_row = 0;
    m_state.active_event = "wild:" + reason;
    record("WILD BATTLE: " + reason);
    //  Wild encounter flashes white -> black -> battle menu.
    const int64_t intro = 400 + m_knobs.battle_opening_delay_ms;
    m_state.screen = SimScreenKind::WHITE_SCREEN;
    m_state.screen_timer_ms = intro;
    m_state.screen_after_timer = SimScreenKind::BATTLE_MENU_ROW0;
    poll_active_watches();
}

void Sim1717Engine::start_trainer_battle(const std::string& name){
    m_state.in_battle = true;
    m_state.in_trainer_battle = true;
    m_state.in_wild_battle = false;
    m_state.battle_cursor_row = 0;
    m_state.active_event = "trainer:" + name;
    m_state.active_trainer = name;
    record("TRAINER TRIGGERED: " + name);
    start_dialog("trainer:" + name, 2);
}

void Sim1717Engine::apply_scene_end_shift(const std::string& event_name){
    if (m_knobs.scene_end_shift_x == 0 && m_knobs.scene_end_shift_y == 0){
        return;
    }
    if (!m_knobs.scene_end_target_event.empty() && m_knobs.scene_end_target_event != event_name){
        return;
    }
    const SimTile shifted{
        (int)std::lround(m_state.x) + m_knobs.scene_end_shift_x,
        (int)std::lround(m_state.y) + m_knobs.scene_end_shift_y
    };
    if (can_stand_on(m_state.map, shifted)){
        m_state.x = (double)shifted.x;
        m_state.y = (double)shifted.y;
        record(std::format("FAULT SCENE_END_POSE ({}) shifted to ({},{})", event_name, shifted.x, shifted.y));
    }
}

void Sim1717Engine::trigger_warp(const SimWarpRule& warp){
    m_state.warps_taken++;
    record(std::format("WARP {} ({},{}) -> {} ({},{}) {}",
        warp.from_map, warp.from_tile.x, warp.from_tile.y,
        warp.to_map, warp.to_tile.x, warp.to_tile.y,
        warp.white_fade ? "[WHITE]" : "[BLACK]"));
    m_state.map = warp.to_map;
    m_state.x = (double)warp.to_tile.x;
    m_state.y = (double)warp.to_tile.y;
    m_state.facing = warp.to_facing;

    //  Special story scene triggered immediately on exiting RowansLab -> Dawn Sandgem tour!
    if (warp.from_map == "RowansLab" && warp.to_map == "SandgemTown" &&
        !m_state.fired_events.contains("dawn_sandgem_tour")){
        m_state.fired_events.insert("dawn_sandgem_tour");
        m_state.pending_post_warp_dialog = true;
        m_state.pending_post_warp_event = "dawn_sandgem_tour";
        m_state.pending_post_warp_lines = 3;
    }

    //  Special story scene triggered immediately on going downstairs PlayerHouse2F -> PlayerHouse1F!
    if (warp.from_map == "PlayerHouse2F" && warp.to_map == "PlayerHouse1F" &&
        !m_state.fired_events.contains("mom_calls_downstairs")){
        m_state.fired_events.insert("mom_calls_downstairs");
        m_state.pending_post_warp_dialog = true;
        m_state.pending_post_warp_event = "mom_calls_downstairs";
        m_state.pending_post_warp_lines = 3;
    }

    //  Special story scene triggered immediately on going upstairs RivalHouse1F -> RivalHouse2F!
    if (warp.from_map == "RivalHouse1F" && warp.to_map == "RivalHouse2F" &&
        !m_state.fired_events.contains("barry_room_2f")){
        m_state.fired_events.insert("barry_room_2f");
        m_state.pending_post_warp_dialog = true;
        m_state.pending_post_warp_event = "barry_room_2f";
        m_state.pending_post_warp_lines = 3;
    }

    start_fade(warp.white_fade, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
}

void Sim1717Engine::on_tile_entered(const SimTile& tile, SimDir dir){
    //  1. Repel step countdown (outdoors and caves where encounters can occur).
    if (m_state.repel_steps > 0){
        m_state.repel_steps--;
        if (m_state.repel_steps == 0){
            m_state.repel_expiring = true;
            start_dialog("repel_expired", 2, true, true);
            return;
        }
    }

    const SimMapData& m = m_db.map(m_state.map);

    //  2. Story coord events!
    const int expand = m_knobs.scene_early_expand;
    auto near_tiles = [&](const std::vector<SimTile>& tiles) -> bool{
        for (const auto& t : tiles){
            if (std::abs(t.x - tile.x) <= expand && std::abs(t.y - tile.y) <= expand){
                return true;
            }
        }
        return false;
    };

    //  PlayerHouse1F: Mom's first departure warning on (6, 10) or (6, 9).
    if (m_state.map == "PlayerHouse1F"){
        if (!m_state.fired_events.contains("moms_warning") &&
            m_state.fired_events.contains("mom_calls_downstairs") &&
            std::abs(tile.x - 6) <= 1 && tile.y >= 9){
            m_state.fired_events.insert("moms_warning");
            m_state.x = 6.0;
            m_state.y = 9.0;
            m_state.facing = SimDir::SOUTH;
            start_dialog("moms_warning", 3);
            return;
        }
        if (m_state.fired_events.contains("mom_parcel") &&
            !m_state.fired_events.contains("moms_call_departure") &&
            std::abs(tile.x - 6) <= 1 && tile.y >= 9 && dir == SimDir::SOUTH){
            m_state.fired_events.insert("moms_call_departure");
            m_state.pending_post_dialog_fade = true;
            //  Console-verified (bdsp-00.04.plan.json): Mom's hat scene leaves
            //  the player outside on TwinleafTown (19, 22) facing east.
            m_state.pending_warp_map = "TwinleafTown";
            m_state.pending_warp_tile = {19, 22};
            m_state.pending_warp_facing = SimDir::EAST;
            m_state.pending_post_warp_dialog = true;
            m_state.pending_post_warp_event = "mom_outdoor_goodbye";
            m_state.pending_post_warp_lines = 2;
            start_dialog("moms_call_departure", 2);
            return;
        }
    }

    //  TwinleafTown: Barry doorstep (9, 12)
    if (m_state.map == "TwinleafTown" && !m_state.fired_events.contains("barry_doorstep")){
        if (near_tiles({{9, 12}})){
            m_state.fired_events.insert("barry_doorstep");
            m_state.x = 9.0;
            m_state.y = 13.0;
            m_state.facing = SimDir::NORTH;
            start_dialog("barry_doorstep", 4);
            return;
        }
    }

    //  RivalHouse2F: Barry runs to lake on entering room / moving east from stairs.
    if (m_state.map == "RivalHouse2F" && !m_state.fired_events.contains("barry_room_2f")){
        if (tile.x >= 4 && tile.y <= 5){
            m_state.fired_events.insert("barry_room_2f");
            start_dialog("barry_room_2f", 3);
            return;
        }
    }

    //  Route201: Barry warning (row 18) and Rowan stop (col 5, rows 13..15)
    if (m_state.map == "Route201"){
        if (!m_state.fired_events.contains("barry_route201_warning") &&
            tile.y <= 18 + expand && tile.x >= 12 && tile.x <= 20){
            m_state.fired_events.insert("barry_route201_warning");
            //  Console-verified (bdsp-00.01.plan.json): Barry's warning scene
            //  leaves the player on Route201 (15, 18).
            m_state.x = 15.0;
            m_state.y = 18.0;
            m_state.facing = SimDir::NORTH;
            start_dialog("barry_route201_warning", 3);
            return;
        }
        if (m_state.fired_events.contains("starly_done") &&
            !m_state.fired_events.contains("rowan_stop_route201") &&
            tile.x >= 4 && tile.x <= 8 && tile.y >= 12 && tile.y <= 16){
            m_state.fired_events.insert("rowan_stop_route201");
            m_state.pending_post_dialog_fade = true;
            m_state.pending_warp_map = "PlayerHouse1F";
            m_state.pending_warp_tile = {6, 8};
            m_state.pending_warp_facing = SimDir::EAST;
            m_state.pending_post_warp_dialog = true;
            m_state.pending_post_warp_event = "mom_running_shoes";
            m_state.pending_post_warp_lines = 3;
            start_dialog("rowan_stop_route201", 2);
            return;
        }
    }

    //  VerityLakefront: Lake entrance (16..17, 12)
    if (m_state.map == "VerityLakefront" && !m_state.fired_events.contains("lake_verity_intro")){
        if (tile.y <= 13 + expand && tile.x >= 15 && tile.x <= 18){
            m_state.fired_events.insert("lake_verity_intro");
            start_dialog("lake_verity_intro", 2);
            return;
        }
    }

    //  SandgemTown: Dawn walks us to Rowan's Lab (x == 4, y: 10..15)
    if (m_state.map == "SandgemTown" && !m_state.fired_events.contains("dawn_to_lab")){
        if (tile.x >= 3 && tile.x <= 6 && tile.y >= 9 && tile.y <= 15){
            m_state.fired_events.insert("dawn_to_lab");
            start_dialog("dawn_to_lab_nickname", 2, true, true);
            return;
        }
    }

    //  Route202: Dawn catch tutorial (x: 20..22, y: 24..29)
    if (m_state.map == "Route202" && !m_state.fired_events.contains("catch_tutorial")){
        if (tile.x >= 19 && tile.x <= 22 && tile.y >= 24 && tile.y <= 29){
            m_state.fired_events.insert("catch_tutorial");
            start_dialog("catch_tutorial", 2);
            return;
        }
    }

    //  JubilifeCity: Dawn at steps (45..48, 60), Poketch president (44..48, 40), Galactic grunts (45..47, 7)
    if (m_state.map == "JubilifeCity"){
        if (!m_state.fired_events.contains("dawn_jubilife") &&
            tile.y <= 60 + expand && tile.y >= 56 && tile.x >= 44 && tile.x <= 49){
            m_state.fired_events.insert("dawn_jubilife");
            m_state.x = 46.0;
            m_state.y = 60.0;
            start_dialog("dawn_jubilife", 3);
            return;
        }
        if (m_state.fired_events.contains("barry_town_map") &&
            !m_state.fired_events.contains("poketch_intro") &&
            tile.y <= 40 + expand && tile.y >= 38 && tile.x >= 43 && tile.x <= 49){
            m_state.fired_events.insert("poketch_intro");
            m_state.x = 46.0;
            m_state.y = 40.0;
            start_dialog("poketch_intro", 3);
            return;
        }
        if (m_state.fired_events.contains("roark_badge") &&
            !m_state.fired_events.contains("jubilife_galactic") &&
            tile.y <= 8 + expand && tile.y >= 5 && tile.x >= 44 && tile.x <= 48){
            m_state.fired_events.insert("jubilife_galactic");
            start_trainer_battle("Jubilife Galactic grunts");
            return;
        }
    }

    //  Route203: Barry 1 (x == 5, y: 17..20)
    if (m_state.map == "Route203" && !m_state.fired_events.contains("barry_route203")){
        if (tile.x >= 5 - expand && tile.x <= 7 && tile.y >= 16 && tile.y <= 21){
            m_state.fired_events.insert("barry_route203");
            m_state.x = 5.0;
            start_trainer_battle("Barry 1");
            return;
        }
    }

    //  OreburghGate: Hiker Rock Smash (7, 22)
    if (m_state.map == "OreburghGate" && !m_state.fired_events.contains("hiker_rock_smash")){
        if (tile.x >= 6 && tile.x <= 8 && tile.y >= 21 && tile.y <= 23){
            m_state.fired_events.insert("hiker_rock_smash");
            start_dialog("hiker_rock_smash", 3);
            return;
        }
    }

    //  OreburghCity: Welcome tour (x == 10, y: 12..15) and Barry post-Roark bump (x == 6, y: 12..15)
    if (m_state.map == "OreburghCity"){
        if (!m_state.fired_events.contains("oreburgh_tour") &&
            !m_state.fired_events.contains("roark_badge") &&
            tile.x >= 9 && tile.x <= 12 && tile.y >= 11 && tile.y <= 16){
            m_state.fired_events.insert("oreburgh_tour");
            //  Tour boy escorts player to outside Oreburgh Gym (23, 23).
            m_state.x = 23.0;
            m_state.y = 23.0;
            m_state.facing = SimDir::EAST;
            start_dialog("oreburgh_tour", 3);
            return;
        }
        if (m_state.fired_events.contains("roark_badge") &&
            !m_state.fired_events.contains("barry_oreburgh_bump") &&
            tile.x <= 7 + expand && tile.x >= 4 && tile.y >= 11 && tile.y <= 16){
            m_state.fired_events.insert("barry_oreburgh_bump");
            m_state.x = 6.0;
            m_state.y = 13.0;
            m_state.facing = SimDir::WEST;
            start_dialog("barry_oreburgh_bump", 3);
            return;
        }
    }

    //  3. Trainer sightlines!
    for (const auto& tr : m_db.trainers_on(m_state.map)){
        if (m_state.defeated_trainers.contains(tr.name)){
            continue;
        }
        //  In BDSP, undefeated Oreburgh Gym trainers stand down once Roark is beaten.
        if (m_state.map == "OreburghGym" && m_state.fired_events.contains("roark_badge")){
            continue;
        }
        SimTile cur = tr.pos;
        for (int step = 1; step <= tr.range; step++){
            cur = cur.next(tr.facing);
            if (!m.contains(cur) || (!is_standable_tile(m.at(cur)) && m.at(cur) != SimTileKind::LEDGE_SOUTH)){
                break;
            }
            if (cur == tile){
                //  In BDSP, when a trainer spots the player, the player finishes
                //  stepping onto the integer tile center and faces the trainer,
                //  while the trainer walks up to the adjacent tile.
                SimDir opp = SimDir::SOUTH;
                if (tr.facing == SimDir::NORTH){
                    opp = SimDir::SOUTH;
                }else if (tr.facing == SimDir::SOUTH){
                    opp = SimDir::NORTH;
                }else if (tr.facing == SimDir::EAST){
                    opp = SimDir::WEST;
                }else{
                    opp = SimDir::EAST;
                }
                m_state.x = (double)tile.x;
                m_state.y = (double)tile.y;
                m_state.facing = opp;
                const SimTile new_tr_tile = tile.next(opp);
                //  Note: Bug Catcher Brandon paces in BDSP and meets the player
                //  from the north along row 12 (leaving the westward lane below
                //  the flower beds open; StoryMaps.cpp:256).
                if (tr.name != "Bug Catcher Brandon"){
                    for (const auto& npc : m.npcs){
                        if (npc.pos == tr.pos){
                            m_state.trainer_moved_pos[m_state.map + ":" + npc.name] = new_tr_tile;
                            break;
                        }
                    }
                }else{
                    m_state.trainer_moved_pos["Route204North:bug_catcher_brandon"] = {tile.x, tile.y - 1};
                }
                start_trainer_battle(tr.name);
                return;
            }
        }
    }

    //  4. Wild encounters (tall grass & caves when Repel is inactive).
    if (m_state.repel_steps == 0){
        if (m.at(tile) == SimTileKind::TALL_GRASS){
            m_state.grass_steps++;
            bool encounter = false;
            if (m_knobs.grass_every > 0 && (m_state.grass_steps % m_knobs.grass_every == 0)){
                encounter = true;
            }else if (m_knobs.grass_prob > 0.0 && rand_unit() < m_knobs.grass_prob){
                encounter = true;
            }
            if (encounter){
                start_wild_battle(m_state.map + " grass");
                return;
            }
        }else if (m.cave){
            m_state.cave_steps++;
            bool encounter = false;
            if (m_knobs.cave_every > 0 && (m_state.cave_steps % m_knobs.cave_every == 0)){
                encounter = true;
            }else if (m_knobs.cave_prob > 0.0 && rand_unit() < m_knobs.cave_prob){
                encounter = true;
            }
            if (encounter){
                start_wild_battle(m_state.map + " cave");
                return;
            }
        }
    }
}

void Sim1717Engine::move_continuous(double vx, double vy, int64_t dt_ms, bool running){
    if (m_state.ledge_lock_ms > 0){
        return;
    }
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle || m_state.menu_open){
        if (m_state.screen == SimScreenKind::BLACK_SCREEN || m_state.screen == SimScreenKind::WHITE_SCREEN){
            m_state.fade_in_moves++;
        }
        return;
    }

    const SimMapData& m = m_db.map(m_state.map);
    const bool use_run = running && m_state.has_running_shoes && m.allow_run;
    const double ms_x = (use_run ? m_knobs.run_ms_across : m_knobs.walk_ms_across) / m_knobs.speed_scale;
    const double ms_y = (use_run ? m_knobs.run_ms_up_down : m_knobs.walk_ms_up_down) / m_knobs.speed_scale;

    //  Note: joystick jy > 0 is NORTH (-y in tile grid), jy < 0 is SOUTH (+y).
    const double norm = std::hypot(vx, vy);
    if (norm < 1e-6){
        return;
    }
    const double nx = vx / norm;
    const double ny = -vy / norm; //  Convert joystick Y (up+) to tile Y (down+).
    m_state.facing = vec_to_dir(vx, vy);

    const double dx = nx * ((double)dt_ms / ms_x);
    const double dy = ny * ((double)dt_ms / ms_y);

    const SimTile old_tile{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};

    auto try_axis = [&](bool is_x, double delta){
        if (std::abs(delta) < 1e-9){
            return;
        }
        const double cand_x = is_x ? (m_state.x + delta) : m_state.x;
        const double cand_y = is_x ? m_state.y : (m_state.y + delta);
        const SimDir move_dir = is_x
            ? (delta > 0.0 ? SimDir::EAST : SimDir::WEST)
            : (delta > 0.0 ? SimDir::SOUTH : SimDir::NORTH);

        //  Check all tiles overlapped by player's collision box.
        const int min_tx = (int)std::floor(cand_x + 0.5 - PLAYER_HALF_BOX);
        const int max_tx = (int)std::floor(cand_x + 0.5 + PLAYER_HALF_BOX);
        const int min_ty = (int)std::floor(cand_y + 0.5 - PLAYER_HALF_BOX);
        const int max_ty = (int)std::floor(cand_y + 0.5 + PLAYER_HALF_BOX);

        const SimTile center{(int)std::lround(cand_x), (int)std::lround(cand_y)};

        //  Check warp on center OR leading edge tile (since PLAYER_HALF_BOX stops
        //  us ~0.28 tiles before the tile boundary of a DOOR/stairs warp).
        const double lead_x = is_x ? (cand_x + (delta > 0.0 ? PLAYER_HALF_BOX : -PLAYER_HALF_BOX)) : cand_x;
        const double lead_y = is_x ? cand_y : (cand_y + (delta > 0.0 ? PLAYER_HALF_BOX : -PLAYER_HALF_BOX));
        const SimTile lead{(int)std::lround(lead_x), (int)std::lround(lead_y)};
        for (const SimTile& probe : {center, lead}){
            //  Mom's departure warning / call fires on PlayerHouse1F (6, 10) BEFORE warping out!
            if (m_state.map == "PlayerHouse1F" && probe == SimTile{6, 10}){
                if ((!m_state.fired_events.contains("moms_warning") && m_state.fired_events.contains("mom_calls_downstairs")) ||
                    (m_state.fired_events.contains("mom_parcel") && !m_state.fired_events.contains("moms_call_departure"))){
                    on_tile_entered(probe, move_dir);
                    return;
                }
            }
            if (const SimWarpRule* warp = m_db.find_warp(m_state.map, probe, move_dir)){
                const double perp_signed = is_x
                    ? (m_state.y - (double)probe.y)
                    : (m_state.x - (double)probe.x);
                const double perp_err = std::abs(perp_signed);
                const int adj_sign = perp_signed >= 0.0 ? 1 : -1;
                const SimTile adj = is_x
                    ? SimTile{probe.x, probe.y + adj_sign}
                    : SimTile{probe.x + adj_sign, probe.y};
                const bool wide_opening = (m_db.find_warp(m_state.map, adj, move_dir) != nullptr);
                if (wide_opening || perp_err <= 0.42){
                    trigger_warp(*warp);
                    return;
                }else{
                    m_state.off_grid_door_snags++;
                    m_state.wall_collisions++;
                    record(std::format("OFF-GRID DOOR SNAG at ({:.2f},{:.2f}) trying warp ({},{})",
                        m_state.x, m_state.y, probe.x, probe.y));
                    return;
                }
            }
        }

        //  Check seamless map border crossing on overworld!
        if (!m_db.map(m_state.map).contains(center)){
            if (auto crossed = m_db.cross_border(m_state.map, center)){
                const std::string prev_map = m_state.map;
                m_state.map = crossed->first;
                m_state.x = (double)crossed->second.x + (cand_x - std::lround(cand_x));
                m_state.y = (double)crossed->second.y + (cand_y - std::lround(cand_y));
                record(std::format("BORDER {} -> {} ({:.2f},{:.2f})", prev_map, m_state.map, m_state.x, m_state.y));
                //  Emit Town Banner on named area change (350ms realistic slide-in delay).
                const int bidx = m_state.banners_emitted++;
                if (m_knobs.missed_banner_index != bidx){
                    m_state.pending_banner_ms = 350;
                }else{
                    record(std::format("FAULT MISSED_BANNER #{} suppressed!", bidx));
                }
                on_tile_entered(crossed->second, move_dir);
                return;
            }
            m_state.wall_collisions++;
            return;
        }

        //  Check ledge hop on center OR leading edge tile!
        for (const SimTile& probe : {center, lead}){
            if (m_db.map(m_state.map).contains(probe) &&
                ledge_matches(m_db.map(m_state.map).at(probe), move_dir)){
                const SimTile land = probe.next(move_dir);
                if (can_stand_on(m_state.map, land)){
                    m_state.x = is_x ? (double)land.x : m_state.x;
                    m_state.y = is_x ? m_state.y : (double)land.y;
                    m_state.ledge_lock_ms = 600;
                    record(std::format("LEDGE HOP to ({},{})", land.x, land.y));
                    on_tile_entered(land, move_dir);
                    return;
                }
            }
        }

        //  Verify every newly-overlapped tile is standable (or seamlessly across border).
        //  Allow moving away from an already-overlapped obstacle tile (depenetration).
        const int cur_min_tx = (int)std::floor(m_state.x + 0.5 - PLAYER_HALF_BOX);
        const int cur_max_tx = (int)std::floor(m_state.x + 0.5 + PLAYER_HALF_BOX);
        const int cur_min_ty = (int)std::floor(m_state.y + 0.5 - PLAYER_HALF_BOX);
        const int cur_max_ty = (int)std::floor(m_state.y + 0.5 + PLAYER_HALF_BOX);
        for (int ty = min_ty; ty <= max_ty; ty++){
            for (int tx = min_tx; tx <= max_tx; tx++){
                SimTile t{tx, ty};
                const bool already_overlapped =
                    tx >= cur_min_tx && tx <= cur_max_tx &&
                    ty >= cur_min_ty && ty <= cur_max_ty;
                const bool moving_away = already_overlapped && (
                    is_x
                        ? (delta > 0.0 ? (double)tx < m_state.x : (double)tx > m_state.x)
                        : (delta > 0.0 ? (double)ty < m_state.y : (double)ty > m_state.y)
                );
                if (moving_away){
                    continue;
                }
                if (!m_db.map(m_state.map).contains(t)){
                    if (auto cr = m_db.cross_border(m_state.map, t)){
                        if (can_stand_on(cr->first, cr->second)){
                            continue;
                        }
                    }
                    m_state.wall_collisions++;
                    return;
                }
                if (!can_stand_on(m_state.map, t)){
                    m_state.wall_collisions++;
                    return;
                }
            }
        }

        m_state.x = cand_x;
        m_state.y = cand_y;
    };

    try_axis(true, dx);
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle){
        return;
    }
    try_axis(false, dy);
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle){
        return;
    }

    const SimTile new_tile{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};
    if (new_tile != old_tile){
        on_tile_entered(new_tile, m_state.facing);
    }
}

void Sim1717Engine::step_one_tile_dpad(SimDir dir){
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle || m_state.menu_open){
        if (m_state.screen == SimScreenKind::BLACK_SCREEN || m_state.screen == SimScreenKind::WHITE_SCREEN){
            m_state.fade_in_moves++;
        }
        return;
    }
    m_state.facing = dir;
    const SimTile cur{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};
    const SimTile nxt = cur.next(dir);

    if (const SimWarpRule* warp = m_db.find_warp(m_state.map, nxt, dir)){
        const double perp_err = (dir == SimDir::EAST || dir == SimDir::WEST)
            ? std::abs(m_state.y - (double)cur.y)
            : std::abs(m_state.x - (double)cur.x);
        if (perp_err <= 0.35){
            trigger_warp(*warp);
        }else{
            m_state.off_grid_door_snags++;
            m_state.wall_collisions++;
        }
        return;
    }

    if (!m_db.map(m_state.map).contains(nxt)){
        if (auto cr = m_db.cross_border(m_state.map, nxt); cr && can_stand_on(cr->first, cr->second)){
            m_state.map = cr->first;
            m_state.x = (double)cr->second.x;
            m_state.y = (double)cr->second.y;
            const int bidx = m_state.banners_emitted++;
            if (m_knobs.missed_banner_index != bidx){
                m_state.banner_timer_ms = 2200;
                poll_active_watches();
            }
            on_tile_entered(cr->second, dir);
            return;
        }
        m_state.wall_collisions++;
        return;
    }

    if (ledge_matches(m_db.map(m_state.map).at(nxt), dir)){
        const SimTile land = nxt.next(dir);
        if (can_stand_on(m_state.map, land)){
            m_state.x = (double)land.x;
            m_state.y = (double)land.y;
            on_tile_entered(land, dir);
            return;
        }
    }

    if (can_stand_on(m_state.map, nxt)){
        //  D-pad movement in BDSP snaps cleanly onto the tile grid center.
        m_state.x = (double)nxt.x;
        m_state.y = (double)nxt.y;
        on_tile_entered(nxt, dir);
    }else{
        m_state.wall_collisions++;
    }
}

void Sim1717Engine::handle_press_a(){
    //  1. Battle menu: pressing A on RUN escapes wild battle!
    if (m_state.in_wild_battle &&
        m_state.screen >= SimScreenKind::BATTLE_MENU_ROW0 &&
        m_state.screen <= SimScreenKind::BATTLE_MENU_ROW3){
        if (m_state.battle_cursor_row == 3){
            m_state.in_battle = false;
            m_state.in_wild_battle = false;
            record("ESCAPED WILD BATTLE");
            start_fade(false, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
            apply_scene_end_shift("wild_escape");
        }
        return;
    }

    //  2. Dialog / Yes-No prompt advancement.
    if (m_state.screen == SimScreenKind::SHORT_DIALOG ||
        m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_YES ||
        m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_NO){
        if (m_state.dialog_has_prompt && m_state.dialog_lines_left <= 1){
            //  Confirm prompt choice!
            const std::string ev = m_state.active_event;
            const bool chose_yes = m_state.prompt_yes_selected;
            m_state.dialog_has_prompt = false;
            m_state.dialog_lines_left = 0;

            if (ev == "dawn_to_lab_nickname"){
                //  After nickname prompt, show Pokedex Yes/No prompt!
                start_dialog("dawn_to_lab_pokedex", 2, true, true);
                return;
            }
            if (ev == "dawn_to_lab_pokedex"){
                //  Finished Rowan's Lab scene: player ends at RowansLab (7, 6) facing north.
                m_state.map = "RowansLab";
                m_state.x = 7.0;
                m_state.y = 6.0;
                m_state.facing = SimDir::NORTH;
                m_state.fired_events.insert("pokedex_received");
                start_dialog("pokedex_outro", 1);
                return;
            }
            if (ev == "repel_expired"){
                m_state.repel_expiring = false;
                if (chose_yes){
                    m_state.repel_steps = 100;
                    record("REPEL RENEWED (100 steps)");
                }else{
                    record("REPEL DECLINED");
                }
                m_state.screen = SimScreenKind::OVERWORLD;
                poll_active_watches();
                return;
            }
            if (ev.starts_with("clown:")){
                if (chose_yes){
                    m_state.coupons.insert(ev);
                    record("COUPON EARNED: " + ev);
                }
                start_dialog(ev + "_done", 1);
                return;
            }
            m_state.screen = SimScreenKind::OVERWORLD;
            poll_active_watches();
            return;
        }

        m_state.dialog_lines_left--;
        if (m_state.dialog_lines_left <= 1 && m_state.dialog_has_prompt){
            m_state.screen = m_state.prompt_yes_selected
                ? SimScreenKind::SHORT_DIALOG_PROMPT_YES
                : SimScreenKind::SHORT_DIALOG_PROMPT_NO;
            poll_active_watches();
            return;
        }
        if (m_state.dialog_lines_left <= 0){
            handle_press_b(); //  Run end-of-dialog transitions cleanly.
        }
        return;
    }

    //  3. Overworld interaction (talk to NPC / pick up item / talk to Roark / Mom / Barry / Clowns / Poketch President).
    if (m_state.screen == SimScreenKind::OVERWORLD && !m_state.in_battle && !m_state.menu_open){
        const SimTile cur{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};
        const SimTile faced = cur.next(m_state.facing);

        //  Route201 Potion clerk at (43, 3) (allow facing or adjacent).
        if (m_state.map == "Route201" &&
            std::abs(cur.x - 43) <= 1 && std::abs(cur.y - 3) <= 1){
            m_state.fired_events.insert("route201_potion");
            start_dialog("route201_potion", 2);
            return;
        }

        //  PlayerHouse1F Mom (parcel).
        if (m_state.map == "PlayerHouse1F" &&
            std::abs(cur.x - 7) <= 1 && std::abs(cur.y - 7) <= 2){
            m_state.fired_events.insert("mom_parcel");
            start_dialog("mom_parcel", 3);
            return;
        }

        //  TrainersSchool Barry.
        if (m_state.map == "TrainersSchool" && cur.y <= 5){
            m_state.fired_events.insert("barry_town_map");
            start_dialog("barry_town_map", 3);
            return;
        }

        //  JubilifeCity 3 Clowns & Poketch President!
        if (m_state.map == "JubilifeCity"){
            auto check_clown = [&](const std::string& cname, SimTile default_pos) -> bool{
                SimTile pos = default_pos;
                if (m_knobs.enable_wanderers){
                    for (const auto& w : m_wanderers){
                        if (w.map == "JubilifeCity" && w.name == cname){
                            pos = w.pos;
                            break;
                        }
                    }
                }
                if (faced == pos || (std::abs(cur.x - pos.x) + std::abs(cur.y - pos.y) == 1 && faced == pos)){
                    start_dialog("clown:" + cname, 2, true, true);
                    return true;
                }
                return false;
            };
            if (check_clown("clown_mart", {55, 33}) ||
                check_clown("clown_tv_station", {36, 16}) ||
                check_clown("clown_poketch_company", {15, 18})){
                return;
            }
            if (faced == SimTile{46, 35} || (cur == SimTile{46, 34} && m_state.facing == SimDir::SOUTH)){
                m_state.fired_events.insert("poketch_received");
                start_dialog("poketch_received", 3);
                return;
            }
        }

        //  OreburghMine Escape Rope item (4, 28) & Roark (18, 28).
        if (m_state.map == "OreburghMine"){
            if (faced == SimTile{4, 28} || (std::abs(cur.x - 4) <= 1 && std::abs(cur.y - 28) <= 1 && !m_state.has_escape_rope)){
                m_state.has_escape_rope = true;
                m_state.collected_items.insert("OreburghMine:escape_rope");
                start_dialog("mine_escape_rope", 2);
                return;
            }
            if (faced == SimTile{18, 28} || (std::abs(cur.x - 18) <= 1 && std::abs(cur.y - 28) <= 1)){
                m_state.fired_events.insert("mine_roark");
                start_dialog("mine_roark", 3);
                return;
            }
        }

        //  OreburghGym Roark (5, 3).
        if (m_state.map == "OreburghGym"){
            if (faced == SimTile{5, 3} || (cur == SimTile{5, 4} && m_state.facing == SimDir::NORTH)){
                start_trainer_battle("Roark");
                return;
            }
        }
    }
}

void Sim1717Engine::handle_press_b(){
    if (m_state.menu_open){
        m_state.menu_open = false;
        m_state.screen = SimScreenKind::OVERWORLD;
        poll_active_watches();
        return;
    }

    if (m_state.screen == SimScreenKind::SHORT_DIALOG ||
        m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_YES ||
        m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_NO){
        //  B declines Repel renewal prompt directly (matches force_repel_expiry(NO)),
        //  but cannot dismiss Rowan/Clown prompts so clear_dialog(STOP_PROMPT) stops on them.
        if (m_state.dialog_has_prompt && m_state.dialog_lines_left <= 1){
            if (m_state.active_event == "repel_expired"){
                m_state.dialog_has_prompt = false;
                m_state.dialog_lines_left = 0;
                m_state.repel_expiring = false;
                m_state.active_event.clear();
                m_state.screen = SimScreenKind::OVERWORLD;
                record("REPEL DECLINED (via B)");
                poll_active_watches();
                return;
            }
            m_state.screen = m_state.prompt_yes_selected
                ? SimScreenKind::SHORT_DIALOG_PROMPT_YES
                : SimScreenKind::SHORT_DIALOG_PROMPT_NO;
            poll_active_watches();
            return;
        }
        m_state.dialog_lines_left--;
        if (m_state.dialog_lines_left <= 1 && m_state.dialog_has_prompt){
            m_state.screen = m_state.prompt_yes_selected
                ? SimScreenKind::SHORT_DIALOG_PROMPT_YES
                : SimScreenKind::SHORT_DIALOG_PROMPT_NO;
            poll_active_watches();
            return;
        }
        if (m_state.dialog_lines_left > 0){
            return;
        }

        const std::string ev = m_state.active_event;
        m_state.active_event.clear();

        //  Special cutscene progressions on dialog completion:
        if (ev == "lake_verity_intro"){
            m_state.screen = SimScreenKind::BRIEFCASE;
            record("BRIEFCASE APPEARED");
            poll_active_watches();
            return;
        }
        if (ev == "catch_tutorial"){
            //  Dawn's catching demo enters STARTER battle menu, then fades out!
            m_state.screen = SimScreenKind::BATTLE_MENU_ROW0;
            m_state.active_event = "catch_tutorial_demo";
            m_state.catch_demo_presses = 4; //  Stay on battle menu ~600ms so watcher confirms it.
            record("CATCH TUTORIAL DEMO BATTLE MENU");
            poll_active_watches();
            return;
        }
        if (ev == "dawn_sandgem_tour"){
            //  Dawn's tour leaves the player outside Sandgem Poke Mart (27, 12).
            m_state.map = "SandgemTown";
            m_state.x = 27.0;
            m_state.y = 12.0;
            m_state.facing = SimDir::NORTH;
            m_state.screen = SimScreenKind::OVERWORLD;
            apply_scene_end_shift(ev);
            poll_active_watches();
            return;
        }
        if (ev == "mom_running_shoes"){
            m_state.has_running_shoes = true;
            m_state.screen = SimScreenKind::OVERWORLD;
            apply_scene_end_shift(ev);
            poll_active_watches();
            return;
        }
        if (m_state.in_trainer_battle){
            //  Pre-battle trainer dialog finished -> transition to BattleMenu!
            if (m_knobs.battle_opening_delay_ms > 0){
                m_state.screen = SimScreenKind::BLACK_SCREEN;
                m_state.screen_timer_ms = m_knobs.battle_opening_delay_ms;
                m_state.screen_after_timer = SimScreenKind::BATTLE_MENU_ROW0;
            }else{
                m_state.screen = SimScreenKind::BATTLE_MENU_ROW0;
            }
            poll_active_watches();
            return;
        }
        if (m_state.pending_post_dialog_fade){
            m_state.pending_post_dialog_fade = false;
            m_state.map = m_state.pending_warp_map;
            m_state.x = (double)m_state.pending_warp_tile.x;
            m_state.y = (double)m_state.pending_warp_tile.y;
            m_state.facing = m_state.pending_warp_facing;
            start_fade(false, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
            return;
        }

        m_state.screen = SimScreenKind::OVERWORLD;
        apply_scene_end_shift(ev);
        poll_active_watches();
        return;
    }

    //  Mashing B during Dawn's catch tutorial demonstration exits via black fade!
    if (m_state.active_event == "catch_tutorial_demo" && m_state.screen == SimScreenKind::BATTLE_MENU_ROW0){
        if (m_state.catch_demo_presses > 1){
            m_state.catch_demo_presses--;
            return;
        }
        m_state.catch_demo_presses = 0;
        m_state.active_event.clear();
        m_state.map = "Route202";
        m_state.x = 20.0;
        m_state.y = 26.0;
        m_state.facing = SimDir::WEST;
        m_state.pending_post_warp_dialog = true;
        m_state.pending_post_warp_event = "dawn_after_catch_demo";
        m_state.pending_post_warp_lines = 2;
        start_fade(false, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
        return;
    }

    //  Mashing B after Starly battle triggers leaving Lake Verity black fade!
    if (m_state.pending_post_dialog_fade && m_state.screen == SimScreenKind::OVERWORLD){
        m_state.pending_post_dialog_fade = false;
        m_state.map = m_state.pending_warp_map;
        m_state.x = (double)m_state.pending_warp_tile.x;
        m_state.y = (double)m_state.pending_warp_tile.y;
        m_state.facing = m_state.pending_warp_facing;
        start_fade(false, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
    }
}

void Sim1717Engine::handle_press_x(){
    if (m_state.screen == SimScreenKind::OVERWORLD && !m_state.in_battle){
        m_state.menu_open = true;
        m_state.screen = SimScreenKind::MAIN_MENU;
        poll_active_watches();
    }
}

void Sim1717Engine::step_wait(Cancellable* cancellable, Milliseconds duration){
    advance_time_ms(duration.count(), cancellable);
}

void Sim1717Engine::step_button(Cancellable* cancellable, Button button, Milliseconds hold, Milliseconds cooldown){
    //  Note: enter_cave_with_down_right_search uses BUTTON_DOWN / BUTTON_RIGHT!
    if (button == BUTTON_DOWN || button == BUTTON_UP || button == BUTTON_LEFT || button == BUTTON_RIGHT){
        DpadPosition dp = (button == BUTTON_DOWN) ? DPAD_DOWN :
                          (button == BUTTON_UP)   ? DPAD_UP :
                          (button == BUTTON_LEFT) ? DPAD_LEFT : DPAD_RIGHT;
        step_dpad(cancellable, dp, hold, cooldown);
        return;
    }
    advance_time_ms(hold.count(), cancellable);
    if (button & (BUTTON_A | BUTTON_ZL)){
        handle_press_a();
    }else if (button & BUTTON_B){
        handle_press_b();
    }else if (button & BUTTON_X){
        handle_press_x();
    }
    advance_time_ms(cooldown.count(), cancellable);
}

void Sim1717Engine::step_dpad(Cancellable* cancellable, DpadPosition pos, Milliseconds hold, Milliseconds cooldown){
    m_state.dpad_calls++;
    const auto dir = dpad_to_dir(pos);
    if (!dir){
        advance_time_ms(hold.count() + cooldown.count(), cancellable);
        return;
    }

    //  Dialog Yes/No prompt cursor navigation.
    if (m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_YES ||
        m_state.screen == SimScreenKind::SHORT_DIALOG_PROMPT_NO){
        if (*dir == SimDir::NORTH){
            m_state.prompt_yes_selected = true;
            m_state.screen = SimScreenKind::SHORT_DIALOG_PROMPT_YES;
        }else if (*dir == SimDir::SOUTH){
            m_state.prompt_yes_selected = false;
            m_state.screen = SimScreenKind::SHORT_DIALOG_PROMPT_NO;
        }
        advance_time_ms(hold.count() + cooldown.count(), cancellable);
        return;
    }

    //  Wild battle menu cursor navigation (0..3 wrapping).
    if (m_state.in_wild_battle &&
        m_state.screen >= SimScreenKind::BATTLE_MENU_ROW0 &&
        m_state.screen <= SimScreenKind::BATTLE_MENU_ROW3){
        if (*dir == SimDir::NORTH){
            m_state.battle_cursor_row = (m_state.battle_cursor_row + 3) % 4;
        }else if (*dir == SimDir::SOUTH){
            m_state.battle_cursor_row = (m_state.battle_cursor_row + 1) % 4;
        }
        m_state.screen = static_cast<SimScreenKind>(static_cast<int>(SimScreenKind::BATTLE_MENU_ROW0) + m_state.battle_cursor_row);
        advance_time_ms(hold.count() + cooldown.count(), cancellable);
        return;
    }

    //  Lost D-pad tap fault.
    if (m_knobs.dpad_drop_prob > 0.0 && rand_unit() < m_knobs.dpad_drop_prob){
        record("FAULT LOST_TAP dropped D-pad press");
        advance_time_ms(hold.count() + cooldown.count(), cancellable);
        return;
    }

    const int64_t h = hold.count();
    if (m_state.facing != *dir && h < 120){
        //  Short tap in a new direction only turns in BDSP!
        m_state.facing = *dir;
    }else{
        const double tile_ms = (*dir == SimDir::EAST || *dir == SimDir::WEST)
            ? m_knobs.walk_ms_across : m_knobs.walk_ms_up_down;
        const int steps = h <= 240 ? 1 : std::max(1, (int)std::lround((double)h / tile_ms));
        for (int i = 0; i < steps; i++){
            step_one_tile_dpad(*dir);
            if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle){
                break;
            }
        }
    }
    advance_time_ms(hold.count() + cooldown.count(), cancellable);
}

void Sim1717Engine::step_joystick(
    Cancellable* cancellable, double jx, double jy, Milliseconds hold, Milliseconds cooldown
){
    m_state.joystick_calls++;
    double eff_hold = (double)hold.count();
    if (m_knobs.hold_jitter_ms > 0.0 && eff_hold > 0.0){
        eff_hold = std::max(0.0, eff_hold + rand_normal() * m_knobs.hold_jitter_ms);
    }
    const double start_delay = std::min(eff_hold, m_knobs.stick_start_delay_ms);
    if (start_delay > 0.0){
        advance_time_ms((int64_t)std::lround(start_delay), cancellable);
    }
    int64_t move_ms = std::max<int64_t>(0, (int64_t)std::lround(eff_hold - start_delay));
    const bool running = std::hypot(jx, jy) >= 0.75;

    while (move_ms > 0){
        if (cancellable){
            cancellable->throw_if_cancelled();
        }
        const int64_t dt = std::min<int64_t>(move_ms, 25);
        move_ms -= dt;
        move_continuous(jx, jy, dt, running);
        advance_time_ms(dt, cancellable);
    }
    advance_time_ms(cooldown.count(), cancellable);
}

void Sim1717Engine::step_mash(Cancellable* cancellable, Button button, Milliseconds duration){
    int64_t rem = duration.count();
    while (rem > 0){
        if (cancellable){
            cancellable->throw_if_cancelled();
        }
        const int64_t dt = std::min<int64_t>(rem, 150);
        rem -= dt;
        if (button & (BUTTON_A | BUTTON_ZL)){
            handle_press_a();
        }else if (button & BUTTON_B){
            handle_press_b();
        }
        advance_time_ms(dt, cancellable);
    }
}

// ============================================================================
//  Synchronous Virtual-Time wait_until / run_until
// ============================================================================

int Sim1717Engine::wait_until(
    VideoStream&,
    CancellableScope& scope,
    Milliseconds timeout,
    const std::vector<PeriodicInferenceCallback>& callbacks,
    Milliseconds default_video_period
){
    const int64_t step_ms = std::max<int64_t>(25, default_video_period.count());
    int64_t elapsed = 0;
    const int64_t limit = std::max<int64_t>(0, timeout.count());

    while (true){
        scope.throw_if_cancelled();
        const VideoSnapshot snap = current_snapshot();
        for (size_t c = 0; c < callbacks.size(); c++){
            const PeriodicInferenceCallback& cb = callbacks[c];
            if (!cb.callback || cb.callback->type() != InferenceType::VISUAL){
                continue;
            }
            auto& vcb = static_cast<VisualInferenceCallback&>(*cb.callback);
            if (vcb.process_frame(*snap.frame, snap.timestamp)){
                return (int)c;
            }
        }
        if (elapsed >= limit){
            return -1;
        }
        const int64_t dt = std::min<int64_t>(step_ms, limit - elapsed);
        elapsed += dt;
        //  Advance virtual clock + timers without cancelling outer scope.
        m_state.sim_time_ms += dt;
        if (m_knobs.max_sim_time_ms > 0 && m_state.sim_time_ms > m_knobs.max_sim_time_ms){
            throw std::runtime_error(std::format(
                "Sim1717: Infinite loop / hang detected (>{}s virtual time) at {} ({:.2f},{:.2f})",
                m_knobs.max_sim_time_ms / 1000, m_state.map, m_state.x, m_state.y));
        }
        advance_wanderers(dt);
        if (m_state.pending_banner_ms > 0){
            m_state.pending_banner_ms -= dt;
            if (m_state.pending_banner_ms <= 0){
                m_state.pending_banner_ms = 0;
                m_state.banner_timer_ms = 2200;
            }
        }else if (m_state.banner_timer_ms > 0){
            m_state.banner_timer_ms = std::max<int64_t>(0, m_state.banner_timer_ms - dt);
        }
        if (m_state.ledge_lock_ms > 0){
            m_state.ledge_lock_ms = std::max<int64_t>(0, m_state.ledge_lock_ms - dt);
        }
        if (m_state.screen_timer_ms > 0){
            m_state.screen_timer_ms -= dt;
            if (m_state.screen_timer_ms <= 0){
                m_state.screen_timer_ms = 0;
                const SimScreenKind next_screen = m_state.screen_after_timer;
                m_state.screen = next_screen;
                if (m_state.pending_post_warp_dialog && next_screen == SimScreenKind::OVERWORLD){
                    m_state.pending_post_warp_dialog = false;
                    start_dialog(m_state.pending_post_warp_event, m_state.pending_post_warp_lines);
                }
            }
        }
    }
}

int Sim1717Engine::run_until(
    VideoStream&,
    CancellableScope& scope,
    std::function<void(CancellableScope&)>&& command,
    const std::vector<PeriodicInferenceCallback>& callbacks,
    Milliseconds
){
    CancellableHolder<CancellableScope> subscope(scope);
    ActiveWatch watch{&subscope, &callbacks, -1};
    m_watch_stack.push_back(&watch);

    try{
        poll_active_watches();
        if (watch.triggered_index < 0){
            command(subscope);
            poll_active_watches();
        }
    }catch (OperationCancelledException&){
        //  Expected when a watcher triggers and cancels subscope.
    }catch (...){
        m_watch_stack.pop_back();
        throw;
    }

    m_watch_stack.pop_back();
    subscope.throw_if_cancelled_with_exception();
    scope.throw_if_cancelled();
    return watch.triggered_index;
}

// ============================================================================
//  Geometric Overworld & Mine Matchers (with Fault Injection)
// ============================================================================

std::optional<OverworldMatchResult> Sim1717Engine::match_overworld(const char* matcher_name){
    OverworldMatchResult res;
    if (m_state.screen != SimScreenKind::OVERWORLD && m_state.screen != SimScreenKind::OVERWORLD_CAVE){
        return res;
    }
    if (m_knobs.landmark_dropout_prob > 0.0 && rand_unit() < m_knobs.landmark_dropout_prob){
        return res;
    }
    const SimLandmarkDef* def = m_db.landmark(matcher_name);
    if (!def || def->map != m_state.map){
        return res;
    }

    double err_tiles_x = def->target_x - m_state.x; //  >0 means target is EAST of player -> move RIGHT.
    double err_tiles_y = def->target_y - m_state.y; //  >0 means target is SOUTH of player -> move DOWN (dy > 0).
    if (m_knobs.landmark_noise_tiles > 0.0){
        err_tiles_x += rand_normal() * m_knobs.landmark_noise_tiles;
        err_tiles_y += rand_normal() * m_knobs.landmark_noise_tiles;
    }

    const bool in_fine = std::abs(err_tiles_x) <= def->max_range_x && std::abs(err_tiles_y) <= def->max_range_y;
    const bool in_coarse = def->has_coarse &&
        std::abs(err_tiles_x) <= def->coarse_range_x && std::abs(err_tiles_y) <= def->coarse_range_y;
    if (!in_fine && !in_coarse){
        return res;
    }

    res.located = true;
    res.coarse = !in_fine;
    //  Sign conventions checked directly from Butters' callers:
    //  Segment_00 & Segment_02:
    //    horizontal ? (dx > 0 ? 1.0 : -1.0) -> dx > 0 means move RIGHT (+x)!
    //    vertical   ? (dy < 0 ? 1.0 : -1.0) -> dy < 0 means move NORTH (-y)!
    //  Wait: check Segment_00 TrainersSchool (line 843):
    //    error = vertical ? (dy + 0.04) * 9/16 : dx;
    //    direction = vertical ? (error < 0 ? UP : DOWN) : (error < 0 ? LEFT : RIGHT);
    //    Notice TrainersSchool wants dy ~ -0.04 and dx ~ 0! Wait: line 841 says
    //    `vertical = result.dy < -0.08 || result.dy > 1.0/54`, so at dy == 0 it is NOT vertical,
    //    and dx < 0 -> LEFT, dx > 0 -> RIGHT!
    //  Check Segment_02 RavagedPath (line 558):
    //    if (abs(dy) > 1/36) y = dy > 0 ? -1.0 : 1.0 (dy > 0 -> move SOUTH, dy < 0 -> move NORTH!)
    //    if (abs(dx) > 1/64) x = dx > 0 ? 1.0 : -1.0 (dx > 0 -> move RIGHT, dx < 0 -> move LEFT!)
    //  So ALL matchers share the SAME convention:
    //    dx = (target_x - player_x) * screen_per_tile_x
    //    dy = (target_y - player_y) * screen_per_tile_y
    res.dx = err_tiles_x * def->screen_per_tile_x;
    res.dy = err_tiles_y * def->screen_per_tile_y;
    if (std::string_view(matcher_name) == "match_jubilife_trainers_school_sign"){
        res.at_target = in_fine && std::abs(res.dx) <= 7.0 / 960.0 &&
            res.dy >= -0.08 && res.dy <= 1.0 / 54.0;
    }else{
        res.at_target = in_fine && std::abs(res.dx) <= def->tol_x && std::abs(res.dy) <= def->tol_y;
    }

    for (int i = 0; i < def->landmark_count; i++){
        OverworldLandmarkResult lm;
        lm.name = std::string(matcher_name) + "_" + std::to_string(i);
        lm.accepted = true;
        lm.score = 5.0;
        lm.margin = 25.0;
        lm.dx = res.dx;
        lm.dy = res.dy;
        lm.search = {0.2, 0.2, 0.6, 0.6};
        lm.found = {0.45 + res.dx, 0.45 + res.dy, 0.08, 0.08};
        res.landmarks.push_back(std::move(lm));
    }
    return res;
}

std::optional<Sim1717MineCenterOffset> Sim1717Engine::mine_center_offset(){
    Sim1717MineCenterOffset out;
    if (m_state.map != "OreburghMine" || m_state.screen != SimScreenKind::OVERWORLD){
        return out;
    }
    if (m_knobs.landmark_dropout_prob > 0.0 && rand_unit() < m_knobs.landmark_dropout_prob){
        return out;
    }
    //  Entrance center is tile x = 15.0, y ~ 2.0.
    //  Positive offset = player left of center -> move RIGHT (+x).
    const double err_x = 15.0 - m_state.x;
    const double err_y = std::abs(2.0 - m_state.y);
    if (std::abs(err_x) <= 2.5 && err_y <= 2.0){
        out.valid = true;
        out.offset = err_x * 0.075;
        out.score = 5.0;
    }
    return out;
}

std::optional<Sim1717MinePictureScore> Sim1717Engine::mine_picture_score(const char* filename){
    Sim1717MinePictureScore out;
    if (m_state.map != "OreburghMine" || m_state.screen != SimScreenKind::OVERWORLD){
        return out;
    }
    if (m_knobs.landmark_dropout_prob > 0.0 && rand_unit() < m_knobs.landmark_dropout_prob){
        return out;
    }
    for (const auto& t : mine_stage_targets()){
        if (std::string_view(filename) == t.filename){
            const double dx = std::abs(m_state.x - t.tx);
            const double dy = std::abs(m_state.y - t.ty);
            if (dx <= t.radius_x && dy <= t.radius_y){
                const double dist = std::hypot(dx / t.radius_x, dy / t.radius_y);
                out.average = 5.0 + 12.0 * dist;
                out.worst = 8.0 + 18.0 * dist;
                out.horizontal = (t.tx - m_state.x) * 0.05;
                out.vertical = (t.ty - m_state.y) * 0.05;
            }else{
                out.average = 85.0;
                out.worst = 95.0;
            }
            return out;
        }
    }
    return out;
}

std::optional<Sim1717MineRoarkScore> Sim1717Engine::mine_roark_score(){
    Sim1717MineRoarkScore out;
    if (m_state.map != "OreburghMine" || m_state.screen != SimScreenKind::OVERWORLD){
        return out;
    }
    //  Roark is at (18, 28); target standing tile 18 is (18, 29).
    const double dx = std::abs(m_state.x - 18.0);
    const double dy = std::abs(m_state.y - 29.0);
    if (dx <= 0.45 && dy <= 0.55){
        out.left = out.right = out.helmet = 10.0;
    }else if (dx <= 1.2 && dy <= 0.8){
        out.left = out.right = out.helmet = 70.0;
    }else if (dx <= 2.5 && dy <= 1.0){
        out.left = out.right = out.helmet = 90.0;
    }else{
        out.left = out.right = out.helmet = 120.0;
    }
    return out;
}

// ============================================================================
//  Non-Navigation Routine Hooks (with strict precondition checks!)
// ============================================================================

bool Sim1717Engine::on_set_time(){
    record("HOOK set_time_fast");
    return true;
}

bool Sim1717Engine::on_verify_settings(){
    record("HOOK verify_tutorial_settings");
    return true;
}

bool Sim1717Engine::on_select_starter(size_t){
    if (m_state.screen != SimScreenKind::BRIEFCASE){
        throw std::runtime_error("Sim1717: select_starter called without BRIEFCASE active!");
    }
    m_state.in_battle = true;
    m_state.in_starly_battle = true;
    m_state.screen = SimScreenKind::BATTLE_MENU_ROW0;
    record("HOOK select_starter -> Starly battle");
    return true;
}

bool Sim1717Engine::on_fight_starly(){
    if (!m_state.in_starly_battle){
        throw std::runtime_error("Sim1717: fight_starly called without Starly battle active!");
    }
    m_state.in_battle = false;
    m_state.in_starly_battle = false;
    m_state.fired_events.insert("starly_done");
    //  After Starly battle, Dawn retrieves briefcase and game walks us out via black fade to VerityLakefront!
    m_state.screen = SimScreenKind::OVERWORLD;
    m_state.pending_post_dialog_fade = true;
    m_state.pending_warp_map = "VerityLakefront";
    m_state.pending_warp_tile = {17, 14};
    m_state.pending_warp_facing = SimDir::NORTH;
    m_state.pending_post_warp_dialog = true;
    m_state.pending_post_warp_event = "barry_after_starly";
    m_state.pending_post_warp_lines = 2;
    record("HOOK fight_starly complete");
    return true;
}

bool Sim1717Engine::on_fight_trainer(const std::string& label){
    if (m_state.in_wild_battle && label == "Route training"){
        m_state.in_battle = false;
        m_state.in_wild_battle = false;
        m_state.active_event.clear();
        start_fade(false, m_knobs.fade_duration_ms, SimScreenKind::OVERWORLD);
        apply_scene_end_shift("wild_training");
        record("HOOK fight_using_move_slot(Route training) won wild battle");
        return true;
    }
    if (!m_state.in_trainer_battle){
        throw std::runtime_error("Sim1717: fight_trainer(" + label + ") called when no trainer battle is active! (pos=" +
            m_state.map + " (" + std::to_string(m_state.x) + "," + std::to_string(m_state.y) + "))");
    }
    m_state.trainer_battles++;
    std::string tr_name = m_state.active_trainer;
    if (tr_name.empty()){
        tr_name = m_state.active_event;
        if (tr_name.starts_with("trainer:")){
            tr_name.erase(0, 8);
        }
    }
    m_state.defeated_trainers.insert(tr_name);
    m_state.in_battle = false;
    m_state.in_trainer_battle = false;
    m_state.active_event.clear();
    m_state.active_trainer.clear();

    if (tr_name == "Roark" || label == "Roark"){
        m_state.fired_events.insert("roark_badge");
        //  Roark gives Coal Badge + TM dialog after battle.
        start_dialog("roark_reward", 3);
    }else if (tr_name == "Jubilife Galactic grunts" || label == "Jubilife Galactic grunts"){
        //  Rowan & Poketch president leave player at (46, 5) / (47, 5) with post-battle dialog.
        m_state.map = "JubilifeCity";
        m_state.x = 46.5;
        m_state.y = 6.0;
        start_dialog("galactic_after", 2);
    }else{
        m_state.screen = SimScreenKind::OVERWORLD;
    }
    apply_scene_end_shift("trainer:" + tr_name);
    record("HOOK fight_trainer(" + label + ") won vs " + tr_name);
    poll_active_watches();
    return true;
}

bool Sim1717Engine::on_catch_1_pokemon(){
    //  Must be on Route202 in or adjacent to the catch grass patch (x: 15..22, y: 24..29).
    if (m_state.map != "Route202" || m_state.x < 14.0 || m_state.x > 23.0 || m_state.y < 23.0 || m_state.y > 30.0){
        throw std::runtime_error(std::format("Sim1717: catch_1_pokemon outside Route202 grass! At {} ({:.2f},{:.2f})",
            m_state.map, m_state.x, m_state.y));
    }
    apply_scene_end_shift("catch_1_pokemon");
    record("HOOK catch_1_pokemon succeeded");
    return true;
}

bool Sim1717Engine::on_battle_1_pokemon(){
    if (m_state.map != "Route202" || m_state.x < 14.0 || m_state.x > 23.0 || m_state.y < 23.0 || m_state.y > 30.0){
        throw std::runtime_error(std::format("Sim1717: battle_1_pokemon outside Route202 grass! At {} ({:.2f},{:.2f})",
            m_state.map, m_state.x, m_state.y));
    }
    apply_scene_end_shift("battle_1_pokemon");
    record("HOOK battle_1_pokemon succeeded (level 7)");
    return true;
}

bool Sim1717Engine::on_use_potion(uint8_t* level){
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle){
        throw std::runtime_error("Sim1717: use_potion_first_pokemon called while not in overworld!");
    }
    if (level){
        *level = 7;
    }
    m_state.sim_time_ms += 500;
    record("HOOK use_potion_first_pokemon");
    return true;
}

bool Sim1717Engine::on_buy_items(){
    if (m_state.map != "OreburghMart"){
        throw std::runtime_error("Sim1717: buy_items called outside OreburghMart! Currently in " + m_state.map);
    }
    m_state.has_repels = true;
    record("HOOK buy_items (Repels bought)");
    return true;
}

bool Sim1717Engine::on_activate_repel(){
    if (!m_state.has_repels){
        throw std::runtime_error("Sim1717: activate_repel called without Repels!");
    }
    if (m_state.screen != SimScreenKind::OVERWORLD || m_state.in_battle){
        throw std::runtime_error("Sim1717: activate_repel called while not in overworld!");
    }
    m_state.repel_steps = 100;
    record("HOOK activate_repel (100 steps)");
    return true;
}

bool Sim1717Engine::on_use_escape_rope(){
    if (m_state.map != "OreburghMine" && m_state.map != "OreburghMineB1F"){
        throw std::runtime_error("Sim1717: use_escape_rope called outside OreburghMine! In " + m_state.map);
    }
    if (!m_state.has_escape_rope){
        throw std::runtime_error("Sim1717: use_escape_rope called without having picked up Escape Rope!");
    }
    //  Warps to OreburghCity outside mine entrance (46, 58) facing south.
    m_state.map = "OreburghCity";
    m_state.x = 46.0;
    m_state.y = 58.0;
    m_state.facing = SimDir::SOUTH;
    m_state.screen = SimScreenKind::OVERWORLD;
    apply_scene_end_shift("use_escape_rope");
    record("HOOK use_escape_rope -> OreburghCity (46, 58)");
    return true;
}

bool Sim1717Engine::on_heal_pokemon(const std::string& label, bool exit_center){
    const bool in_jub = (m_state.map == "JubilifeCenter");
    const bool in_flo = (m_state.map == "FloaromaCenter");
    if (!in_jub && !in_flo){
        throw std::runtime_error("Sim1717: heal_pokemon(" + label + ") called outside Pokemon Center! In " + m_state.map);
    }
    if (exit_center){
        if (in_jub){
            m_state.map = "JubilifeCity";
            m_state.x = 52.0;
            m_state.y = 41.0;
        }else{
            m_state.map = "FloaromaTown";
            m_state.x = 16.0;
            m_state.y = 27.0;
        }
        m_state.facing = SimDir::SOUTH;
        m_state.screen = SimScreenKind::OVERWORLD;
        apply_scene_end_shift("heal:" + label);
    }
    record("HOOK heal_pokemon(" + label + ") complete");
    return true;
}

bool Sim1717Engine::on_use_hm(const std::string& label){
    if (m_state.map != "RavagedPath"){
        throw std::runtime_error("Sim1717: use_HM(" + label + ") called outside RavagedPath! In " + m_state.map);
    }
    const SimTile cur{(int)std::lround(m_state.x), (int)std::lround(m_state.y)};
    const SimTile faced = cur.next(m_state.facing);
    //  Verify player is adjacent to rock (22, 43) or (22, 42) and facing east!
    if (std::abs(cur.x - 21) <= 1 && std::abs(cur.y - 43) <= 1 &&
        (faced == SimTile{22, 43} || faced == SimTile{22, 42} || m_state.facing == SimDir::EAST)){
        m_state.ravaged_rock_smashed = true;
        record("HOOK use_HM smashed rock (22, 43)!");
        return true;
    }
    throw std::runtime_error(std::format(
        "Sim1717: use_HM({}) not aligned to RavagedPath rock (22,43)! Player at ({:.2f},{:.2f}) facing {}",
        label, m_state.x, m_state.y, sim_dir_name(m_state.facing)));
}

bool Sim1717Engine::on_checkpoint_save(){
    record("HOOK checkpoint_save");
    return true;
}

bool Sim1717Engine::on_reload_backup_save(){
    record("HOOK reload_backup_save");
    return false;
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

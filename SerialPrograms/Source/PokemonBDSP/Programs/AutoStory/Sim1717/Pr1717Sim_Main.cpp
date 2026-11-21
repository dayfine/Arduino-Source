/*  BDSP AutoStory PR #1717 Simulation Runner
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs Developer-Butters' PR #1717 BDSP AutoStory (Beginning -> Floaroma:
 *  00.01 through 02.01) against our verified 22-map BDSP GridSim world and
 *  fault sweep suite.
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/Recording/StreamHistorySession.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Sim1717/Sim1717_Frames.h"
#include "PokemonBDSP/Programs/AutoStory/Sim1717/Sim1717_Hooks.h"
#include "PokemonBDSP/Programs/AutoStory/Sim1717/Sim1717_Map.h"
#include "PokemonBDSP/Programs/AutoStory/Sim1717/Sim1717_World.h"
#include "PokemonBDSP/Programs/AutoStory/StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
#include "PokemonBDSP/Programs/AutoStory/StorySegments/PokemonBDSP_AutoStory_Segment_01.h"
#include "PokemonBDSP/Programs/AutoStory/StorySegments/PokemonBDSP_AutoStory_Segment_02.h"
#include "Tests/TestUtils.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

//  Declared in Segment_00.cpp.
void sim1717_seg00_0001(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context);
void sim1717_seg00_0002(VideoStream& stream, ProControllerContext& context);
void sim1717_seg00_0003(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context);
void sim1717_seg00_0004(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, const AutoStoryOptions& options);
void sim1717_seg00_0005(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, const AutoStoryOptions& options);
void sim1717_seg00_0006(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context);

//  Declared in Segment_01.cpp.
void jubilife_to_oreburgh_gate(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, AutoStoryStats& stats);
void oreburgh_gate_nav(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, AutoStoryStats& stats);
void oreburgh_gate_to_mine(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context);
void oreburgh_mine_nav(VideoStream& stream, ProControllerContext& context, Language language);
void oreburgh_mine_to_gym(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context);
void gym_Puzzle_01(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, AutoStoryStats& stats);

//  Declared in Segment_02.cpp.
void roark_to_floaroma(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, AutoStoryStats& stats, Language language);

namespace{

class QuietLogger : public Logger{
public:
    explicit QuietLogger(bool verbose) : m_verbose(verbose){}
    virtual void log(const char* msg, Color = Color()) override{
        if (m_verbose){
            std::cout << "  [log] " << msg << "\n";
        }
    }
    virtual void log(const std::string& msg, Color = Color()) override{
        if (m_verbose){
            std::cout << "  [log] " << msg << "\n";
        }
    }
private:
    bool m_verbose = false;
};

struct CheckpointSpec{
    std::string id;
    std::string label;
    std::string start_map;
    double start_x;
    double start_y;
    SimDir start_facing;
    bool start_shoes;
    std::vector<std::string> prior_events;
    std::vector<std::string> prior_defeated_trainers;
    //  Expected arrival area check.
    std::string end_map;
    double end_x_min, end_x_max;
    double end_y_min, end_y_max;
    std::function<void(SingleSwitchProgramEnvironment&, VideoStream&, ProControllerContext&, const AutoStoryOptions&, AutoStoryStats&)> run;
};

std::vector<CheckpointSpec> build_checkpoints(){
    return {
        {
            "00.01", "Bedroom -> Verity Lakefront (before starter)",
            "PlayerHouse2F", 4.0, 6.0, SimDir::NORTH, false, {}, {},
            "VerityLakefront", 15.0, 19.0, 12.0, 17.0,
            [](auto& env, auto& stream, auto& ctx, const auto&, auto&){
                sim1717_seg00_0001(env, stream, ctx);
            }
        },
        {
            "00.02", "Verity Lakefront -> Starter & Starly -> Lakefront",
            "VerityLakefront", 17.0, 14.0, SimDir::NORTH, false,
            {"mom_calls_downstairs", "moms_warning", "barry_doorstep", "barry_room_2f", "barry_route201_warning"}, {},
            "VerityLakefront", 16.0, 18.0, 13.0, 16.0,
            [](auto&, auto& stream, auto& ctx, const auto&, auto&){
                sim1717_seg00_0002(stream, ctx);
            }
        },
        {
            "00.03", "Lakefront -> Running Shoes -> Route 201 Potions -> Rowan's Lab Pokedex",
            "VerityLakefront", 17.0, 14.0, SimDir::NORTH, false,
            {"mom_calls_downstairs", "moms_warning", "barry_doorstep", "barry_room_2f", "barry_route201_warning", "lake_verity_intro", "starly_done"}, {},
            "RowansLab", 6.0, 8.0, 5.0, 7.0,
            [](auto& env, auto& stream, auto& ctx, const auto&, auto&){
                sim1717_seg00_0003(env, stream, ctx);
            }
        },
        {
            "00.04", "Rowan's Lab -> Dawn Tour -> Mom Parcel -> Route 202 Catch",
            "RowansLab", 7.0, 6.0, SimDir::NORTH, true,
            {"mom_calls_downstairs", "moms_warning", "barry_doorstep", "barry_room_2f", "barry_route201_warning", "lake_verity_intro", "starly_done", "rowan_stop_route201", "route201_potion", "dawn_to_lab", "pokedex_received"}, {},
            "Route202", 14.0, 23.0, 23.0, 30.0,
            [](auto& env, auto& stream, auto& ctx, const auto& opt, auto&){
                sim1717_seg00_0004(env, stream, ctx, opt);
            }
        },
        {
            "00.05", "Route 202 Catch -> Trainers -> Jubilife Dawn",
            "Route202", 20.0, 26.0, SimDir::WEST, true,
            {"mom_calls_downstairs", "moms_warning", "barry_doorstep", "barry_room_2f", "barry_route201_warning", "lake_verity_intro", "starly_done", "rowan_stop_route201", "route201_potion", "dawn_to_lab", "pokedex_received", "dawn_sandgem_tour", "mom_parcel", "moms_call_departure", "catch_tutorial"}, {},
            "JubilifeCity", 44.0, 50.0, 56.0, 63.0,
            [](auto& env, auto& stream, auto& ctx, const auto& opt, auto&){
                sim1717_seg00_0005(env, stream, ctx, opt);
            }
        },
        {
            "00.06", "Jubilife Dawn -> Trainers School -> 3 Clowns -> Poketch -> Center PC",
            "JubilifeCity", 46.0, 60.0, SimDir::NORTH, true,
            {"dawn_jubilife"}, {"Tristan", "Natalie", "Logan"},
            "JubilifeCenter", 8.0, 14.0, 3.0, 7.0,
            [](auto& env, auto& stream, auto& ctx, const auto&, auto&){
                sim1717_seg00_0006(env, stream, ctx);
            }
        },
        {
            "01.01", "Jubilife Center PC -> Route 203 (Barry 1) -> Oreburgh Gate -> Oreburgh City",
            "JubilifeCenter", 11.0, 4.0, SimDir::NORTH, true,
            {"dawn_jubilife", "barry_town_map", "poketch_intro", "poketch_received"},
            {"Tristan", "Natalie", "Logan"},
            "OreburghCity", 1.0, 5.0, 11.0, 15.0,
            [](auto& env, auto& stream, auto& ctx, const auto&, auto& stats){
                jubilife_to_oreburgh_gate(env, stream, ctx, stats);
                oreburgh_gate_nav(env, stream, ctx, stats);
            }
        },
        {
            "01.02", "Oreburgh City -> Tour -> Mine (Escape Rope + Roark) -> Oreburgh Gym Entry",
            "OreburghCity", 2.0, 13.0, SimDir::EAST, true,
            {"dawn_jubilife", "barry_town_map", "poketch_intro", "poketch_received", "barry_route203", "hiker_rock_smash"},
            {"Tristan", "Natalie", "Logan", "Barry 1", "Route203 Dallas/Youngster", "Route203 Sebastian/Lass", "Camper Curtis", "Picnicker Diana"},
            "OreburghGym", 4.0, 6.0, 22.0, 24.0,
            [](auto& env, auto& stream, auto& ctx, const auto& opt, auto&){
                oreburgh_gate_to_mine(env, stream, ctx);
                oreburgh_mine_nav(stream, ctx, opt.language);
                oreburgh_mine_to_gym(env, stream, ctx);
            }
        },
        {
            "01.03", "Oreburgh Gym Puzzle -> Roark -> Coal Badge -> Exit Gym",
            "OreburghGym", 5.0, 24.0, SimDir::NORTH, true,
            {"oreburgh_tour", "mine_roark"},
            {"Tristan", "Natalie", "Logan", "Barry 1", "Route203 Dallas/Youngster", "Route203 Sebastian/Lass", "Camper Curtis", "Picnicker Diana"},
            "OreburghCity", 25.0, 27.0, 20.0, 23.0,
            [](auto& env, auto& stream, auto& ctx, const auto&, auto& stats){
                gym_Puzzle_01(env, stream, ctx, stats);
            }
        },
        {
            "02.01", "Oreburgh Gym Exit -> Mart -> Gate -> Route 203 -> Jubilife Galactic -> Route 204 -> Ravaged Path -> Floaroma Center",
            "OreburghCity", 26.0, 21.0, SimDir::SOUTH, true,
            {"dawn_jubilife", "barry_town_map", "poketch_intro", "poketch_received", "barry_route203", "hiker_rock_smash", "oreburgh_tour", "mine_roark", "roark_badge"},
            {"Tristan", "Natalie", "Logan", "Barry 1", "Route203 Dallas/Youngster", "Route203 Sebastian/Lass", "Camper Curtis", "Picnicker Diana", "Roark"},
            "FloaromaTown", 14.0, 18.0, 25.0, 29.0,
            [](auto& env, auto& stream, auto& ctx, const auto& opt, auto& stats){
                roark_to_floaroma(env, stream, ctx, stats, opt.language);
            }
        },
    };
}

struct RunOutcome{
    bool passed = false;
    std::string error;
    std::string end_map;
    double end_x = 0.0;
    double end_y = 0.0;
    SimDir end_facing = SimDir::SOUTH;
    int64_t sim_time_ms = 0;
    int wild_battles = 0;
    int trainer_battles = 0;
    int warps = 0;
    int fades = 0;
    int banners = 0;
    int off_grid_snags = 0;
    int fade_in_moves = 0;
    std::vector<std::string> trace;
};

RunOutcome run_checkpoint(
    const Sim1717MapDB& db,
    const CheckpointSpec& cp,
    const Sim1717FaultKnobs& knobs,
    bool verbose_log = false
){
    RunOutcome out;
    QuietLogger logger(verbose_log);
    Sim1717Engine engine(db, knobs);
    engine.reset(cp.start_map, cp.start_x, cp.start_y, cp.start_facing, cp.start_shoes);
    for (const auto& ev : cp.prior_events){
        engine.state().fired_events.insert(ev);
    }
    for (const auto& tr : cp.prior_defeated_trainers){
        engine.state().defeated_trainers.insert(tr);
    }

    Sim1717ProController controller(logger, engine);
    Sim1717VideoFeed video(engine);
    DummyAudioFeed audio;
    DummyVideoOverlay overlay;
    StreamHistorySession history(logger);
    VideoStream stream(logger, audio, video, history, overlay);
    CancellableHolder<CancellableScope> scope;
    ProControllerContext context(scope, controller);

    //  Env is never dereferenced inside the hooked checkpoint paths (only passed to set_time_fast).
    alignas(64) unsigned char dummy_env_storage[256] = {};
    auto& dummy_env = *reinterpret_cast<SingleSwitchProgramEnvironment*>(dummy_env_storage);

    EventNotificationOption notif("Status", true, false);
    AutoStoryOptions options{
        Language::English,
        StarterChoice::CHIMCHAR,
        notif,
        HelpMode::OUTSIDE_HELP,
        OutsideHelpConfig{}
    };
    AutoStoryStats stats;
    stats.starter_choice = StarterChoice::CHIMCHAR;
    stats.outside_help = true;

    set_sim1717_hooks(&engine);
    try{
        cp.run(dummy_env, stream, context, options, stats);
        const auto& st = engine.state();
        const bool in_target =
            st.map == cp.end_map &&
            st.x >= cp.end_x_min && st.x <= cp.end_x_max &&
            st.y >= cp.end_y_min && st.y <= cp.end_y_max;
        if (!in_target){
            out.passed = false;
            out.error = std::format("Ended off-target: at {} ({:.2f},{:.2f}) expected {} ([{:.0f}..{:.0f}], [{:.0f}..{:.0f}])",
                st.map, st.x, st.y, cp.end_map, cp.end_x_min, cp.end_x_max, cp.end_y_min, cp.end_y_max);
        }else{
            out.passed = true;
        }
    }catch (const Exception& e){
        out.passed = false;
        out.error = e.message();
    }catch (const std::exception& e){
        out.passed = false;
        out.error = e.what();
    }
    set_sim1717_hooks(nullptr);

    const auto& st = engine.state();
    out.end_map = st.map;
    out.end_x = st.x;
    out.end_y = st.y;
    out.end_facing = st.facing;
    out.sim_time_ms = st.sim_time_ms;
    out.wild_battles = st.wild_battles;
    out.trainer_battles = st.trainer_battles;
    out.warps = st.warps_taken;
    out.fades = st.fades_emitted;
    out.banners = st.banners_emitted;
    out.off_grid_snags = st.off_grid_door_snags;
    out.fade_in_moves = st.fade_in_moves;
    out.trace = st.trace_log;
    return out;
}

} // namespace

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

int main(int argc, char** argv){
    using namespace PokemonAutomation;
    using namespace PokemonAutomation::NintendoSwitch;
    using namespace PokemonAutomation::NintendoSwitch::PokemonBDSP;

    std::string maps_dir;
    if (const char* env_maps = std::getenv("SIM1717_MAPS_DIR"); env_maps && *env_maps){
        maps_dir = env_maps;
    }
    std::string only_cp;
    bool run_chained = false;
    bool run_sweeps = false;
    bool run_mc = false;
    bool trace = false;
    bool verbose = false;
    double speed_scale = 1.0;
    double walk_across = 282.0;
    double walk_updown = 282.0;
    double run_across = 140.0;
    double run_updown = 140.0;
    int mc_trials = 50;

    for (int i = 1; i < argc; i++){
        std::string arg = argv[i];
        if (arg == "--maps" && i + 1 < argc){
            maps_dir = argv[++i];
        }else if (arg == "--cp" && i + 1 < argc){
            only_cp = argv[++i];
        }else if (arg == "--chained"){
            run_chained = true;
        }else if (arg == "--sweeps"){
            run_sweeps = true;
        }else if (arg == "--mc"){
            run_mc = true;
        }else if (arg == "--mc-trials" && i + 1 < argc){
            mc_trials = std::stoi(argv[++i]);
        }else if (arg == "--trace"){
            trace = true;
        }else if (arg == "--verbose"){
            verbose = true;
        }else if (arg == "--speed" && i + 1 < argc){
            speed_scale = std::stod(argv[++i]);
        }else if (arg == "--walk-across" && i + 1 < argc){
            walk_across = std::stod(argv[++i]);
        }else if (arg == "--walk-updown" && i + 1 < argc){
            walk_updown = std::stod(argv[++i]);
        }else if (arg == "--run-across" && i + 1 < argc){
            run_across = std::stod(argv[++i]);
        }else if (arg == "--run-updown" && i + 1 < argc){
            run_updown = std::stod(argv[++i]);
        }
    }

    if (maps_dir.empty()){
        std::cerr << "Error: pass --maps <dir> (or set SIM1717_MAPS_DIR) pointing to data/games/bdsp/maps/runtime\n";
        return 2;
    }

    //  1. Verify synthetic frames against real BDSP detectors first!
    std::cout << "=== Verifying synthetic frames against real BDSP detectors ===\n";
    const std::string frame_err = sim1717_verify_synthetic_frames();
    if (!frame_err.empty()){
        std::cerr << "FRAME VERIFICATION FAILED: " << frame_err << "\n";
        return 1;
    }
    std::cout << "PASS: All 15 synthetic frames matched real BDSP detectors.\n\n";

    Sim1717MapDB db(maps_dir);
    auto checkpoints = build_checkpoints();

    Sim1717FaultKnobs base_knobs;
    base_knobs.walk_ms_across = walk_across;
    base_knobs.walk_ms_up_down = walk_updown;
    base_knobs.run_ms_across = run_across;
    base_knobs.run_ms_up_down = run_updown;
    base_knobs.speed_scale = speed_scale;
    base_knobs.trace = trace;

    std::cout << "=== Clean Nominal Runs (Beginning -> Floaroma) ===\n";
    for (const auto& cp : checkpoints){
        if (!only_cp.empty() && cp.id != only_cp){
            continue;
        }
        auto r = run_checkpoint(db, cp, base_knobs, verbose);
        std::cout << std::format("[{}] {:5s} | {:4s} | end={} ({:.2f},{:.2f}) | t={:.1f}s | warps={} fades={} banners={} trn={} wild={} snags={} fadeMoves={}",
            cp.id, r.passed ? "PASS" : "FAIL", "",
            r.end_map, r.end_x, r.end_y, r.sim_time_ms / 1000.0,
            r.warps, r.fades, r.banners, r.trainer_battles, r.wild_battles,
            r.off_grid_snags, r.fade_in_moves);
        if (!r.passed){
            std::cout << "\n    ERROR: " << r.error;
        }
        std::cout << "\n";
        if (trace){
            for (const auto& line : r.trace){
                std::cout << "    " << line << "\n";
            }
        }
    }

    if (run_chained){
        std::cout << "\n=== Chained End-to-End Run (00.01 -> 02.01, carrying position & state) ===\n";
        QuietLogger logger(verbose);
        auto chained_knobs = base_knobs;
        chained_knobs.max_sim_time_ms = 3'000'000;
        Sim1717Engine engine(db, chained_knobs);
        engine.reset("PlayerHouse2F", 4.0, 6.0, SimDir::NORTH, false);
        Sim1717ProController controller(logger, engine);
        Sim1717VideoFeed video(engine);
        DummyAudioFeed audio;
        DummyVideoOverlay overlay;
        StreamHistorySession history(logger);
        VideoStream stream(logger, audio, video, history, overlay);
        CancellableHolder<CancellableScope> scope;
        ProControllerContext context(scope, controller);
        alignas(64) unsigned char dummy_env_storage[256] = {};
        auto& dummy_env = *reinterpret_cast<SingleSwitchProgramEnvironment*>(dummy_env_storage);
        EventNotificationOption notif("Status", true, false);
        AutoStoryOptions options{Language::English, StarterChoice::CHIMCHAR, notif, HelpMode::OUTSIDE_HELP, OutsideHelpConfig{}};
        AutoStoryStats stats;
        stats.starter_choice = StarterChoice::CHIMCHAR;
        stats.outside_help = true;

        set_sim1717_hooks(&engine);
        bool all_ok = true;
        for (const auto& cp : checkpoints){
            try{
                cp.run(dummy_env, stream, context, options, stats);
                const auto& st = engine.state();
                const bool in_target =
                    st.map == cp.end_map &&
                    st.x >= cp.end_x_min && st.x <= cp.end_x_max &&
                    st.y >= cp.end_y_min && st.y <= cp.end_y_max;
                std::cout << std::format("  [{}] {:5s} | end={} ({:.2f},{:.2f}) | cum_t={:.1f}s\n",
                    cp.id, in_target ? "PASS" : "OFF", st.map, st.x, st.y, st.sim_time_ms / 1000.0);
                if (!in_target){
                    all_ok = false;
                    break;
                }
            }catch (const Exception& e){
                const auto& st = engine.state();
                std::cout << std::format("  [{}] FAIL  | end={} ({:.2f},{:.2f}) | ERROR: {}\n",
                    cp.id, st.map, st.x, st.y, e.message());
                all_ok = false;
                break;
            }catch (const std::exception& e){
                const auto& st = engine.state();
                std::cout << std::format("  [{}] FAIL  | end={} ({:.2f},{:.2f}) | ERROR: {}\n",
                    cp.id, st.map, st.x, st.y, e.what());
                all_ok = false;
                break;
            }
        }
        set_sim1717_hooks(nullptr);
        std::cout << "Chained Beginning -> Floaroma: " << (all_ok ? "PASS (10/10)" : "FAIL") << "\n";
    }

    if (run_sweeps){
        std::cout << "\n=== Fault Sweep Matrix (dev-bdsp parity + stick timing sweeps) ===\n";
        for (const auto& cp : checkpoints){
            if (!only_cp.empty() && cp.id != only_cp){
                continue;
            }
            auto clean = run_checkpoint(db, cp, base_knobs, false);
            std::cout << "\n--- Checkpoint " << cp.id << " (" << cp.label << ") | clean=" << (clean.passed ? "PASS" : "FAIL") << " ---\n";
            if (!clean.passed){
                std::cout << "  CLEAN FAILED: " << clean.error << "\n";
                continue;
            }

            auto report_single = [&](const char* name, Sim1717FaultKnobs k){
                k.trace = false;
                auto res = run_checkpoint(db, cp, k, false);
                std::cout << std::format("  {:22s}: {:4s}{}\n",
                    name, res.passed ? "PASS" : "FAIL",
                    res.passed ? "" : (" (" + res.error + ")"));
            };

            //  1. GRASS_BATTLES & CAVE_BATTLES (every 8 tiles)
            {
                auto k = base_knobs;
                k.grass_every = 8;
                k.cave_every = 8;
                report_single("GRASS+CAVE (every 8)", k);
            }
            //  2. SLOW_FADE_IN (+2500ms)
            {
                auto k = base_knobs;
                k.slow_fade_in_extra_ms = 2500;
                auto res = run_checkpoint(db, cp, k, false);
                std::cout << std::format("  {:22s}: {:4s} (fade_in_moves={}){}\n",
                    "SLOW_FADE_IN (+2.5s)", (res.passed && res.fade_in_moves == 0) ? "PASS" : "FAIL",
                    res.fade_in_moves, res.passed ? "" : (" (" + res.error + ")"));
            }
            //  3. BATTLE_OPENING (+2500ms delay before menu)
            {
                auto k = base_knobs;
                k.battle_opening_delay_ms = 2500;
                report_single("BATTLE_OPENING (+2.5s)", k);
            }
            //  4. SCENE_EARLY (+1 tile trigger radius)
            {
                auto k = base_knobs;
                k.scene_early_expand = 1;
                report_single("SCENE_EARLY (+1 tile)", k);
            }
            //  5. MISSED_FADE (each fade suppressed one by one)
            if (clean.fades > 0){
                int pass_cnt = 0;
                std::string first_err;
                for (int f = 0; f < clean.fades; f++){
                    auto k = base_knobs;
                    k.missed_fade_index = f;
                    auto res = run_checkpoint(db, cp, k, false);
                    if (res.passed){
                        pass_cnt++;
                    }else if (first_err.empty()){
                        first_err = std::format("fade #{}: {}", f, res.error);
                    }
                }
                std::cout << std::format("  {:22s}: {}/{} pass{}\n",
                    "MISSED_FADE (1 each)", pass_cnt, clean.fades,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
            //  6. MISSED_BANNER (each banner suppressed one by one)
            if (clean.banners > 0){
                int pass_cnt = 0;
                std::string first_err;
                for (int b = 0; b < clean.banners; b++){
                    auto k = base_knobs;
                    k.missed_banner_index = b;
                    auto res = run_checkpoint(db, cp, k, false);
                    if (res.passed){
                        pass_cnt++;
                    }else if (first_err.empty()){
                        first_err = std::format("banner #{}: {}", b, res.error);
                    }
                }
                std::cout << std::format("  {:22s}: {}/{} pass{}\n",
                    "MISSED_BANNER (1 each)", pass_cnt, clean.banners,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
            //  7. SCENE_END_POSE (8 neighbor tile shifts)
            {
                int pass_cnt = 0;
                int total = 0;
                std::string first_err;
                for (int dy = -1; dy <= 1; dy++){
                    for (int dx = -1; dx <= 1; dx++){
                        if (dx == 0 && dy == 0){
                            continue;
                        }
                        total++;
                        auto k = base_knobs;
                        k.scene_end_shift_x = dx;
                        k.scene_end_shift_y = dy;
                        auto res = run_checkpoint(db, cp, k, false);
                        if (res.passed){
                            pass_cnt++;
                        }else if (first_err.empty()){
                            first_err = std::format("shift({:+d},{:+d}): {}", dx, dy, res.error);
                        }
                    }
                }
                std::cout << std::format("  {:22s}: {}/{} pass{}\n",
                    "SCENE_END_POSE (8dir)", pass_cnt, total,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
            //  8. WANDERERS (10 seeds)
            {
                int pass_cnt = 0;
                std::string first_err;
                for (uint64_t s = 1; s <= 10; s++){
                    auto k = base_knobs;
                    k.enable_wanderers = true;
                    k.wanderer_seed = s;
                    auto res = run_checkpoint(db, cp, k, false);
                    if (res.passed){
                        pass_cnt++;
                    }else if (first_err.empty()){
                        first_err = std::format("seed {}: {}", s, res.error);
                    }
                }
                std::cout << std::format("  {:22s}: {}/10 pass{}\n",
                    "WANDERERS (10 seeds)", pass_cnt,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
            //  9. SPEED_DRIFT (-10%, -5%, -3%, +3%, +5%, +10%)
            {
                std::string line;
                for (double s : {0.90, 0.95, 0.97, 1.03, 1.05, 1.10}){
                    auto k = base_knobs;
                    k.speed_scale = s;
                    auto res = run_checkpoint(db, cp, k, false);
                    line += std::format(" {:+.0f}%:{}", (s - 1.0) * 100.0, res.passed ? "OK" : "FAIL");
                }
                std::cout << std::format("  {:22s}:{}\n", "SPEED_DRIFT", line);
            }
            //  10. LOST_DPAD_TAP (5% drop rate, 20 trials)
            {
                int pass_cnt = 0;
                std::string first_err;
                for (int t = 1; t <= 20; t++){
                    auto k = base_knobs;
                    k.dpad_drop_prob = 0.05;
                    k.rng_seed = 1000 + t;
                    auto res = run_checkpoint(db, cp, k, false);
                    if (res.passed){
                        pass_cnt++;
                    }else if (first_err.empty()){
                        first_err = res.error;
                    }
                }
                std::cout << std::format("  {:22s}: {}/20 pass{}\n",
                    "LOST_DPAD_TAP (5%)", pass_cnt,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
            //  11. LANDMARK_DROPOUT (25% frame dropout, 20 trials)
            {
                int pass_cnt = 0;
                std::string first_err;
                for (int t = 1; t <= 20; t++){
                    auto k = base_knobs;
                    k.landmark_dropout_prob = 0.25;
                    k.rng_seed = 2000 + t;
                    auto res = run_checkpoint(db, cp, k, false);
                    if (res.passed){
                        pass_cnt++;
                    }else if (first_err.empty()){
                        first_err = res.error;
                    }
                }
                std::cout << std::format("  {:22s}: {}/20 pass{}\n",
                    "LANDMARK_DROP (25%)", pass_cnt,
                    first_err.empty() ? "" : (" | e.g. " + first_err));
            }
        }
    }

    if (run_mc){
        std::cout << std::format("\n=== Realistic Combined Monte Carlo ({} trials/checkpoint) ===\n", mc_trials);
        double cumulative = 1.0;
        for (const auto& cp : checkpoints){
            if (!only_cp.empty() && cp.id != only_cp){
                continue;
            }
            int passed = 0;
            std::map<std::string, int> fail_reasons;
            for (int t = 1; t <= mc_trials; t++){
                auto k = base_knobs;
                k.trace = false;
                k.rng_seed = 50000ULL + t * 97ULL;
                //  Realistic console variance: +/-2% speed drift, 20ms stick hold jitter,
                //  10% grass / 6% cave wild encounter rate, wandering NPCs, 5% landmark dropout.
                uint64_t sr = k.rng_seed ^ 0x9E3779B97F4A7C15ULL;
                auto u = [&]() -> double{
                    sr = sr * 6364136223846793005ULL + 1442695040888963407ULL;
                    return (double)(sr >> 32) / 4294967296.0;
                };
                k.speed_scale = 0.98 + 0.04 * u();
                k.hold_jitter_ms = 20.0;
                k.grass_prob = 0.10;
                k.cave_prob = 0.06;
                k.enable_wanderers = true;
                k.wanderer_seed = 100 + t;
                k.landmark_dropout_prob = 0.05;
                auto res = run_checkpoint(db, cp, k, false);
                if (res.passed){
                    passed++;
                }else{
                    std::string key = res.error.substr(0, std::min<size_t>(res.error.size(), 90));
                    fail_reasons[key]++;
                }
            }
            const double rate = (double)passed / mc_trials;
            cumulative *= rate;
            std::cout << std::format("[{}] {:3d}/{} ({:5.1f}%) | {}", cp.id, passed, mc_trials, rate * 100.0, cp.label);
            if (!fail_reasons.empty()){
                //  Print top failure reason.
                auto top = std::max_element(fail_reasons.begin(), fail_reasons.end(),
                    [](const auto& a, const auto& b){ return a.second < b.second; });
                std::cout << std::format("\n    top fail ({}x): {}", top->second, top->first);
            }
            std::cout << "\n";
        }
        if (only_cp.empty()){
            std::cout << std::format("\nCumulative Beginning -> Floaroma 1-attempt pass probability: {:.2f}%\n", cumulative * 100.0);
        }
    }

    return 0;
}

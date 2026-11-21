/*  BDSP AutoStory PR #1717 Simulation Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Thread-local test seam allowing PR #1717 AutoStory checkpoints to execute
 *  against the offline GridSim + SimProController + SimVideoFeed harness
 *  without altering production behavior when inactive (g_sim1717_hooks == nullptr).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Hooks_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Hooks_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/InferenceCallback.h"

namespace PokemonAutomation{
class CancellableScope;
class VideoStream;

namespace NintendoSwitch{
namespace PokemonBDSP{

struct OverworldMatchResult;

struct Sim1717MineCenterOffset{
    bool valid = false;
    double offset = 0.0;
    double score = 1000.0;
};

struct Sim1717MinePictureScore{
    double average = 1000.0;
    double worst = 1000.0;
    double horizontal = 0.0;
    double vertical = 0.0;
};

struct Sim1717MineRoarkScore{
    double left = 1000.0;
    double right = 1000.0;
    double helmet = 1000.0;
    double horizontal = 0.0;
    double vertical = 0.0;
    double helmet_horizontal = 0.0;
    double helmet_vertical = 0.0;
};

struct Sim1717Hooks{
    virtual ~Sim1717Hooks() = default;

    //  Virtual steady clock & wall clock.
    virtual std::chrono::steady_clock::time_point steady_now() = 0;
    virtual WallClock wall_now() = 0;

    //  Synchronous virtual-time InferenceRoutines implementations.
    virtual int wait_until(
        VideoStream& stream,
        CancellableScope& scope,
        Milliseconds timeout,
        const std::vector<PeriodicInferenceCallback>& callbacks,
        Milliseconds default_video_period
    ) = 0;

    virtual int run_until(
        VideoStream& stream,
        CancellableScope& scope,
        std::function<void(CancellableScope&)>&& command,
        const std::vector<PeriodicInferenceCallback>& callbacks,
        Milliseconds default_video_period
    ) = 0;

    //  Geometric landmark matchers (backed by GridSim pose + fault knobs).
    virtual std::optional<OverworldMatchResult> match_overworld(const char* matcher_name) = 0;
    virtual std::optional<Sim1717MineCenterOffset> mine_center_offset() = 0;
    virtual std::optional<Sim1717MinePictureScore> mine_picture_score(const char* filename) = 0;
    virtual std::optional<Sim1717MineRoarkScore> mine_roark_score() = 0;

    //  Non-navigation routine hooks (each verifies preconditions on GridSim!).
    virtual bool on_set_time() = 0;
    virtual bool on_verify_settings() = 0;
    virtual bool on_fight_starly() = 0;
    virtual bool on_fight_trainer(const std::string& label) = 0;
    virtual bool on_catch_1_pokemon() = 0;
    virtual bool on_battle_1_pokemon() = 0;
    virtual bool on_use_potion(uint8_t* level) = 0;
    virtual bool on_buy_items() = 0;
    virtual bool on_activate_repel() = 0;
    virtual bool on_use_escape_rope() = 0;
    virtual bool on_heal_pokemon(const std::string& label, bool exit_center) = 0;
    virtual bool on_use_hm(const std::string& label) = 0;
    virtual bool on_checkpoint_save() = 0;
    virtual bool on_reload_backup_save() = 0;
    virtual bool on_select_starter(size_t right_presses) = 0;
};

//  Active hooks for the current thread, or nullptr in normal production runs.
Sim1717Hooks* get_sim1717_hooks();
void set_sim1717_hooks(Sim1717Hooks* hooks);

//  Steady clock helper: returns virtual time in sim mode, steady_clock::now() otherwise.
std::chrono::steady_clock::time_point sim1717_steady_now();

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

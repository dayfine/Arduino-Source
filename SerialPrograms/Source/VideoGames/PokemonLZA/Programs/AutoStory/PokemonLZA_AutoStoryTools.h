/*  PokemonLZA AutoStory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTools_H
#define PokemonAutomation_PokemonLZA_AutoStoryTools_H

#include <functional>
#include <string>
#include <memory>
#include <vector>
#include "CommonFramework/Language.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

//  Statistics tracker for AutoStory progress.
struct AutoStoryStats : public StatsTracker{
    AutoStoryStats()
        : m_checkpoint(m_stats["Checkpoint"])
        , m_segment(m_stats["Segment"])
        , m_reset(m_stats["Reset"])
    {
        m_display_order.emplace_back("Checkpoint");
        m_display_order.emplace_back("Segment");
        m_display_order.emplace_back("Reset");
    }
    std::atomic<uint64_t>& m_checkpoint;
    std::atomic<uint64_t>& m_segment;
    std::atomic<uint64_t>& m_reset;
};

//  Available starter Pokemon choices for LZA.
enum class StarterChoice{
    CHIKORITA,
    CYNDAQUIL,
    TOTODILE,
};

//  Runtime options bundle passed to segments and checkpoints.
struct AutoStoryOptions{
    Language language;
    StarterChoice starter_choice;
    BattleAIOption& battle_ai;
    EventNotificationOption& notif_status_update;
    EventNotificationOption& notif_error_recoverable;
};

//  Interface representing a major story mission or tutorial segment.
class AutoStory_Segment{
public:
    virtual ~AutoStory_Segment() = default;
    virtual std::string name() const = 0;
    virtual std::string start_text() const = 0;
    virtual std::string end_text() const = 0;
    virtual void run_segment(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats
    ) const = 0;
};

//  Interface representing an individual checkpoint with retry support.
class AutoStory_Checkpoint{
public:
    virtual ~AutoStory_Checkpoint() = default;
    virtual std::string name() const = 0;
    virtual std::string start_text() const = 0;
    virtual std::string end_text() const = 0;
    virtual void run_checkpoint(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats
    ) const = 0;

    std::string checkpoint_text() const{
        return "Checkpoint: " + name() + ". Start point: " + start_text();
    }
};

//  Save game from the overworld pause menu and update stats.
void checkpoint_save(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats
);

//  Execute checkpoint action, resetting from Home on failure.
void checkpoint_reattempt_loop(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    EventNotificationOption& notif_error_recoverable,
    AutoStoryStats& stats,
    const std::string& checkpoint_text,
    std::function<void(size_t attempt_number)>&& action
);

//  Mash B through dialogue sequences until overworld is reached.
void clear_dialog(
    ConsoleHandle& console,
    ProControllerContext& context,
    std::chrono::milliseconds timeout = std::chrono::seconds(60)
);

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

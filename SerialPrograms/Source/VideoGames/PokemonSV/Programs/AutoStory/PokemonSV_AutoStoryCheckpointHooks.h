/*  AutoStory Checkpoint Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_AutoStoryCheckpointHooks_H
#define PokemonAutomation_PokemonSV_AutoStoryCheckpointHooks_H

#include <string>
#include "CommonFramework/Notifications/EventNotificationOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryRetry.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class SVCheckpointMode{
    //  Saves without the overworld menu, never day skips, and does not check for
    //  the overworld after the action.
    TUTORIAL,
    MAIN_STORY,
    //  Also day skips and saves before each retry.
    MAIN_STORY_DAY_SKIP,
};

//  The SV game steps around an AutoStory checkpoint attempt.
class SVCheckpointHooks : public Pokemon::CheckpointRetryHooks{
public:
    SVCheckpointHooks(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        EventNotificationOption& notif_status_update,
        EventNotificationOption& notif_error_recoverable,
        Pokemon::AutoStoryStats& stats,
        std::string fail_message,
        SVCheckpointMode mode
    );

    virtual void log(const std::string& message) override;
    virtual void wait_for_all_requests() override;
    virtual void save_checkpoint() override;
    virtual void before_attempt(size_t attempt) override;
    virtual void after_success() override;
    virtual void report_recoverable(const std::string& message) override;
    [[noreturn]] virtual void fail(const OperationFailedException& e) override;
    virtual void reset_for_retry() override;

private:
    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    EventNotificationOption& m_notif_status_update;
    EventNotificationOption& m_notif_error_recoverable;
    Pokemon::AutoStoryStats& m_stats;
    const std::string m_fail_message;
    const SVCheckpointMode m_mode;
};


}
}
}
#endif

/*  Nintendo Switch Pro Controller (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "Controllers/MgbaLink/MgbaLink_SharedLink.h"
#include "NintendoSwitch_MgbaLink_KeyMap.h"
#include "NintendoSwitch_MgbaLink_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


ProController_MgbaLink::ProController_MgbaLink(Logger& logger, std::string address)
    : ProController(logger)
    , ControllerWithScheduler(logger, logging_throttler())
    , m_sender(logger, GlobalThreadPools::unlimited_realtime(), MgbaLink::link_registry(), std::move(address))
{}
ProController_MgbaLink::~ProController_MgbaLink() = default;


void ProController_MgbaLink::cancel_all_commands(){
    std::lock_guard<Mutex> lg(m_state_lock);
    m_scheduler.clear_on_next();
    m_sender.cancel();
    m_logger.log("cancel_all_commands(): keys released.", COLOR_DARKGREEN);
}
void ProController_MgbaLink::replace_on_next_command(){
    std::lock_guard<Mutex> lg(m_state_lock);
    m_scheduler.clear_on_next();
    m_sender.replace_on_next();
    m_logger.log("replace_on_next_command()", COLOR_DARKGREEN);
}
void ProController_MgbaLink::wait_for_all(Cancellable* cancellable){
    SuperscalarScheduler::Schedule schedule;
    std::lock_guard<Mutex> lg0(m_issue_lock);
    {
        std::lock_guard<Mutex> lg1(m_state_lock);
        m_scheduler.issue_wait_for_all(schedule);
    }
    execute_schedule(cancellable, schedule);
    m_sender.wait_for_all(cancellable);
}


void ProController_MgbaLink::execute_state(
    Cancellable* cancellable,
    const SuperscalarScheduler::ScheduleEntry& entry
){
    if (cancellable){
        cancellable->throw_if_cancelled();
    }

    SwitchControllerState controller_state;
    for (auto& item : entry.state){
        static_cast<const SwitchCommand&>(*item).apply(controller_state);
    }
    GbaKeysFromSwitch keys = gba_keys_from_switch(controller_state);
    log_unmapped_once(keys.unmapped, keys.right_stick_moved);

    m_sender.push(cancellable, keys.keys, entry.duration);
}

void ProController_MgbaLink::log_unmapped_once(Button unmapped, bool right_stick_moved){
    Button new_buttons = unmapped & (Button)~(ButtonFlagType)m_logged_unmapped;
    if (new_buttons != BUTTON_NONE){
        m_logged_unmapped |= new_buttons;
        m_logger.log(
            "MgbaLink: no GBA key for " + button_to_string(new_buttons) + "; ignored (logged once).",
            COLOR_ORANGE
        );
    }
    if (right_stick_moved && !m_logged_right_stick){
        m_logged_right_stick = true;
        m_logger.log("MgbaLink: no GBA key for the right stick; ignored (logged once).", COLOR_ORANGE);
    }
}



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation

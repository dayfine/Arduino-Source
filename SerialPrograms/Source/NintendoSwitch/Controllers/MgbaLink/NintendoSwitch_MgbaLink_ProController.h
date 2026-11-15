/*  Nintendo Switch Pro Controller (mGBA over MgbaLink)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Drives an mGBA instance as if it were a Switch: programs keep
 *  ProControllerContext and milliseconds; each scheduled controller state
 *  becomes GBA keys (NintendoSwitch_MgbaLink_KeyMap.h) held for whole
 *  frames, sent as "KEYS" on the connection the mGBA video source for the
 *  same address opened (MgbaLink::KeySender). While the controller is in
 *  use, mGBA runs paced by the app's clock. Buttons with no GBA key are
 *  logged once and do nothing. Developer mode only (fork lab tooling).
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_MgbaLink_ProController_H
#define PokemonAutomation_NintendoSwitch_MgbaLink_ProController_H

#include <string>
#include "Controllers/ControllerTypes.h"
#include "Controllers/MgbaLink/MgbaLink_KeySender.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerWithScheduler.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  A Pro Controller that plays its states as GBA keys on an mGBA instance.
class ProController_MgbaLink final :
    public ProController,
    public ControllerWithScheduler
{
public:
    using ContextType = ProControllerContext;

    //  "address" is the "host:port" the mGBA video source uses.
    ProController_MgbaLink(Logger& logger, std::string address);
    ~ProController_MgbaLink();


    //  The ProController interface. Callers reach it through ProController
    //  (the program's context), so the overrides stay out of this class's
    //  own public surface.
private:
    virtual ControllerPerformanceClass performance_class() const override{
        return ControllerPerformanceClass::SerialPABotBase_Wired;
    }
    //  One GBA frame: a state shorter than that still lasts a frame.
    virtual Milliseconds ticksize() const override{
        return Milliseconds(17);
    }
    virtual Milliseconds cooldown() const override{
        return Milliseconds(17);
    }
    //  Paced: every state lands on the frame it was scheduled for.
    virtual Milliseconds timing_variation() const override{
        return Milliseconds::zero();
    }
    virtual bool atomic_multibutton() const override{
        return true;
    }


private:
    virtual void cancel_all_commands() override;
    virtual void replace_on_next_command() override;
    virtual bool cancel_all_and_wait(Milliseconds timeout) override{
        //  The release is sent before cancel_all_commands() returns.
        cancel_all_commands();
        return true;
    }

    virtual void wait_for_all(Cancellable* cancellable) override;


private:
    //  Superscalar Commands (the "ssf" framework)

    virtual void issue_barrier(Cancellable* cancellable) override{
        ControllerWithScheduler::issue_barrier(cancellable);
    }
    virtual void issue_nop(Cancellable* cancellable, Milliseconds duration) override{
        ControllerWithScheduler::issue_nop(cancellable, duration);
    }

    virtual void issue_buttons(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        Button button
    ) override{
        ControllerWithScheduler::issue_buttons(cancellable, delay, hold, cooldown, button);
    }
    virtual void issue_dpad(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        DpadPosition position
    ) override{
        ControllerWithScheduler::issue_dpad(cancellable, delay, hold, cooldown, position);
    }
    virtual void issue_left_joystick(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        const JoystickPosition& position
    ) override{
        ControllerWithScheduler::issue_left_joystick(cancellable, delay, hold, cooldown, position);
    }
    virtual void issue_right_joystick(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        const JoystickPosition& position
    ) override{
        ControllerWithScheduler::issue_right_joystick(cancellable, delay, hold, cooldown, position);
    }

    virtual void issue_gyro_accel_x(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_accel_x(cancellable, delay, hold, cooldown, value);
    }
    virtual void issue_gyro_accel_y(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_accel_y(cancellable, delay, hold, cooldown, value);
    }
    virtual void issue_gyro_accel_z(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_accel_z(cancellable, delay, hold, cooldown, value);
    }
    virtual void issue_gyro_rotate_x(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_rotate_x(cancellable, delay, hold, cooldown, value);
    }
    virtual void issue_gyro_rotate_y(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_rotate_y(cancellable, delay, hold, cooldown, value);
    }
    virtual void issue_gyro_rotate_z(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        int16_t value
    ) override{
        ControllerWithScheduler::issue_gyro_rotate_z(cancellable, delay, hold, cooldown, value);
    }

    virtual void issue_full_controller_state(
        Cancellable* cancellable,
        bool enable_logging,
        Milliseconds duration,
        Button button,
        DpadPosition dpad,
        const JoystickPosition& left_joystick,
        const JoystickPosition& right_joystick
    ) override{
        ControllerWithScheduler::issue_full_controller_state(
            cancellable,
            enable_logging,
            duration,
            button,
            dpad,
            left_joystick,
            right_joystick
        );
    }


private:
    //  High speed RPCs.

    virtual void issue_mash_button(
        Cancellable* cancellable,
        Milliseconds duration,
        Button button,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown
    ) override{
        ControllerWithScheduler::issue_mash_button(cancellable, duration, button, delay, hold, cooldown);
    }
    virtual void issue_mash_button(
        Cancellable* cancellable,
        Milliseconds duration,
        Button button0, Button button1
    ) override{
        ControllerWithScheduler::issue_mash_button(cancellable, duration, button0, button1);
    }
    virtual void issue_mash_AZs(
        Cancellable* cancellable,
        Milliseconds duration
    ) override{
        ControllerWithScheduler::issue_mash_AZs(cancellable, duration);
    }
    virtual void issue_system_scroll(
        Cancellable* cancellable,
        Milliseconds delay, Milliseconds hold, Milliseconds cooldown,
        DpadPosition direction  //  Diagonals not allowed.
    ) override{
        ControllerWithScheduler::issue_system_scroll(cancellable, delay, hold, cooldown, direction);
    }


private:
    virtual Logger& logger() override{
        return m_logger;
    }
    virtual bool is_ready() const override{
        return m_sender.is_ready();
    }


private:
    virtual void execute_state(
        Cancellable* cancellable,
        const SuperscalarScheduler::ScheduleEntry& entry
    ) override;

    void log_unmapped_once(Button unmapped, bool right_stick_moved);


private:
    MgbaLink::KeySender m_sender;

    //  What was already reported as having no GBA key. Only the issuing
    //  thread (execute_state) touches these.
    Button m_logged_unmapped = BUTTON_NONE;
    bool m_logged_right_stick = false;
};



}  // namespace NintendoSwitch
}  // namespace PokemonAutomation
#endif

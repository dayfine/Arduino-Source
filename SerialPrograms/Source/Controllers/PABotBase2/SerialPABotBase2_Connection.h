/*  Serial Port (PABotBase2) Connection
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Controllers_SerialPABotBase2_Connection_H
#define PokemonAutomation_Controllers_SerialPABotBase2_Connection_H

#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/SerialConnection/SerialConnection.h"
#include "Common/PABotBase2/ReliableConnectionLayer/PABotBase2CC_ReliableStreamConnection.h"
#ifdef QT_CORE_LIB
#include "CommonFramework/Devices/LabDeviceRuntime.h"
#endif
#include "Controllers/SerialPort/SerialLogger.h"
#include "Controllers/PABotBase2/PABotBase2_Connection.h"

namespace PokemonAutomation{
namespace SerialPABotBase{



class SerialPABotBase2_Connection final : public PABotBase2::Connection{
public:
    SerialPABotBase2_Connection(
        Logger& logger,
        std::string name
    );
    ~SerialPABotBase2_Connection();

    virtual bool cancel(std::exception_ptr exception = nullptr) noexcept override;


public:
    ControllerType refresh_controller_type();


private:
    void naked_wait(WallDuration duration);

    void open_guarded_serial_port();
    bool open_serial_port();
    bool try_connect_to_device(WallDuration timeout);
    bool connect_to_device();
    bool open_serial_connection();
    bool open_device_connection();
    void connect_thread_body();


private:
    SerialLogger m_logger;
    std::string m_device_name;

//    Mutex m_lock;
#ifdef QT_CORE_LIB
    // Declared before the port: release only after the physical connection closes.
    std::unique_ptr<LabDeviceLease> m_device_lease;
#endif
    AsyncTask m_connect_thread;
    std::unique_ptr<SerialConnection> m_unreliable_connection;
    std::unique_ptr<PABotBase2::ReliableStreamConnection> m_stream_connection;
};




}
}
#endif

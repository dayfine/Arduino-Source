/*  MgbaLink Pacer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Paced lockstep (design: docs/design/Lab-mGBA-Console.md, section 2): the
 *  app owns the clock and mGBA follows it. About every 8 ms the pacer sends
 *  "GRANT F0 + floor((now - t0) * 59.7275 / 1000) + 2", so the emulator runs
 *  at most two frames ahead of app time and stalls when the app does. The
 *  bridge ignores GRANT in free mode.
 *
 *  Runs on a thread pool task; Qt-free.
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Pacer_H
#define PokemonAutomation_Controllers_MgbaLink_Pacer_H

#include <stdint.h>
#include <chrono>
#include <memory>
#include <optional>
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ThreadPool.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "MgbaLink_Connection.h"
#include "MgbaLink_Protocol.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  Grants frames to the bridge from app time until destroyed.
class Pacer{
public:
    static constexpr std::chrono::milliseconds DEFAULT_PERIOD{8};

    //  Starts granting at once. "clock" is the anchor the controller
    //  schedules its KEYS against. The pacer does not keep "connection"
    //  open: it stops once its owner (the video source) closes it.
    Pacer(
        Logger& logger,
        ThreadPool& thread_pool,
        std::weak_ptr<Connection> connection,
        FrameClock clock,
        std::chrono::milliseconds period = DEFAULT_PERIOD
    );

    //  Stops granting. The last grant stays with the bridge: the emulator
    //  stalls there until someone grants again (or closes the connection).
    ~Pacer();

    //  False once a GRANT failed or the connection was closed; the pacer
    //  then stops.
    bool running() const;

private:
    void run();

private:
    Logger& m_logger;
    const std::weak_ptr<Connection> m_connection;
    const FrameClock m_clock;
    const std::chrono::milliseconds m_period;

    mutable Mutex m_lock;
    ConditionVariable m_cv;
    bool m_stopping = false;
    bool m_failed = false;

    AsyncTask m_task;
};


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif

/*  MgbaLink Pacer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "MgbaLink_Keys.h"
#include "MgbaLink_Pacer.h"

namespace PokemonAutomation{
namespace MgbaLink{


Pacer::Pacer(
    Logger& logger,
    ThreadPool& thread_pool,
    std::weak_ptr<Connection> connection,
    FrameClock clock,
    std::chrono::milliseconds period
)
    : m_logger(logger)
    , m_connection(std::move(connection))
    , m_clock(clock)
    , m_period(period)
{
    m_task = thread_pool.dispatch_now_blocking([this]{ run(); });
}
Pacer::~Pacer(){
    {
        std::lock_guard<Mutex> lg(m_lock);
        m_stopping = true;
    }
    m_cv.notify_all();
    m_task.wait_and_ignore_exceptions();
}
bool Pacer::running() const{
    std::lock_guard<Mutex> lg(m_lock);
    return !m_stopping && !m_failed;
}

void Pacer::run(){
    std::optional<uint64_t> last_grant;
    while (true){
        uint64_t grant = grant_frame(m_clock, current_time(), KeySchedule::LEAD_FRAMES);
        if (grant != last_grant){
            std::optional<std::string> error;
            if (std::shared_ptr<Connection> connection = m_connection.lock()){
                try{
                    connection->command(std::format("GRANT {}", grant));
                }catch (ConnectionException& e){
                    error = e.message();
                }
            }else{
                error = "the connection was closed.";
            }
            if (error){
                m_logger.log("MgbaLink: pacer stopped: " + *error, COLOR_RED);
                std::lock_guard<Mutex> lg(m_lock);
                m_failed = true;
                return;
            }
            last_grant = grant;
        }
        std::unique_lock<Mutex> lg(m_lock);
        if (m_cv.wait_for(lg, m_period, [this]{ return m_stopping; })){
            return;
        }
    }
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation

/*  MgbaLink Connection
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One TCP connection to one "bridge.lua" running inside mGBA (MgbaLink v1,
 *  lab/mgba/PROTOCOL.md on the fork's agents branch). Every command is one
 *  line and gets exactly one reply line; commands from several threads are
 *  serialized. The bridge holds one client at a time, so the destructor
 *  always says BYE, which also releases every key and grant it holds.
 *
 *  Qt-free. The socket's receive thread comes from the thread pool passed in.
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Connection_H
#define PokemonAutomation_Controllers_MgbaLink_Connection_H

#include <stdint.h>
#include <chrono>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ThreadPool.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Sockets/ClientSocket.h"
#include "MgbaLink_Protocol.h"

namespace PokemonAutomation{
namespace MgbaLink{


class MgbaLink_Connection final : private AbstractClientSocket::Listener{
public:
    MgbaLink_Connection(
        Logger& logger,
        ThreadPool& thread_pool,
        std::string host,
        uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    ~MgbaLink_Connection();

    //  Connects and says "HELLO 1". Throws ConnectionException if the bridge
    //  cannot be reached, refuses the client ("ERR busy") or replies with
    //  anything but a v1 HELLO.
    HelloReply open();

    //  Sends "BYE" (best effort) and closes. Safe to call more than once.
    void close() noexcept;

    //  Sends one command line and returns its reply line. Throws
    //  ConnectionException on a timeout, a lost connection or an "ERR" reply.
    std::string command(std::string_view line);

    //  "FRAME": the bridge writes the last finished frame as a PNG.
    FrameReply frame();

    //  "KEYS <frame> <mask>": hold "mask" from "frame" on. Returns how many
    //  frames late the command arrived (0 = on time).
    uint64_t keys(uint64_t frame, uint16_t mask);

private:
    virtual void on_connect_finished(const std::string& error_message) override;
    virtual void on_receive_data(const void* data, size_t bytes) override;
    virtual void on_disconnect(const std::string& error_message) override;

    void connect_socket();
    std::string wait_for_reply(std::string_view command_name);
    [[noreturn]] void fail(std::string message);

private:
    Logger& m_logger;
    const std::string m_host;
    const uint16_t m_port;
    const std::chrono::milliseconds m_timeout;

    //  Held for a whole command: one request in flight at a time.
    Mutex m_command_lock;

    //  Guards everything below; the socket's receive thread writes it.
    Mutex m_lock;
    ConditionVariable m_cv;
    std::optional<std::string> m_connect_error;
    std::optional<std::string> m_disconnect_error;
    LineSplitter m_splitter;
    std::deque<std::string> m_replies;

    //  Last member: destroyed first, which joins its receive thread before
    //  the state above goes away.
    ClientSocket m_socket;
};



}
}
#endif

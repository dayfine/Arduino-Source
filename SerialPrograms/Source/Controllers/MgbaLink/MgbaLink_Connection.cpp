/*  MgbaLink Connection
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "MgbaLink_Connection.h"

namespace PokemonAutomation{
namespace MgbaLink{



Connection::Connection(
    Logger& logger,
    ThreadPool& thread_pool,
    std::string host,
    uint16_t port,
    std::chrono::milliseconds timeout
)
    : m_logger(logger)
    , m_host(std::move(host))
    , m_port(port)
    , m_timeout(timeout)
    , m_socket(thread_pool)
{
    m_socket.add_listener(*this);
}
Connection::~Connection(){
    close();
    m_socket.remove_listener(*this);
}


HelloReply Connection::open(){
    bool opened_before;
    {
        std::lock_guard<Mutex> lg(m_lock);
        opened_before = m_opened;
        m_opened = true;
    }
    if (opened_before){
        throw InternalProgramError(
            &m_logger, PA_CURRENT_FUNCTION,
            "MgbaLink: open() called twice on one connection; make a new one to reconnect."
        );
    }
    connect_socket();
    std::string reply = command("HELLO 1");
    std::optional<HelloReply> hello = parse_hello_reply(reply);
    if (!hello){
        fail(std::format("Unexpected HELLO reply: {}", reply));
    }
    m_logger.log(std::format(
        "MgbaLink: connected to {}:{}, game {}, {}, mode {}.",
        m_host, m_port, hello->game_code, hello->bridge_version, hello->mode
    ));
    return *hello;
}

void Connection::connect_socket(){
    m_socket.connect(m_host, m_port);
    std::optional<std::string> error;
    {
        std::unique_lock<Mutex> lg(m_lock);
        bool finished = m_cv.wait_for(lg, m_timeout, [this]{
            return m_aborted || m_connect_error.has_value();
        });
        if (m_aborted){
            error = "aborted";
        }else if (!finished){
            error = "timed out";
        }else if (!m_connect_error->empty()){
            error = *m_connect_error;
        }
    }
    if (error){
        fail(std::format("Unable to connect to {}:{}: {}", m_host, m_port, *error));
    }
}

void Connection::close() noexcept{
    bool aborted = true;
    try{
        std::lock_guard<Mutex> lg(m_lock);
        aborted = m_aborted;
    }catch (...){}
    if (!aborted && m_socket.state() == AbstractClientSocket::State::CONNECTED){
        try{
            command("BYE");
        }catch (...){}
    }
    m_socket.close();
}

void Connection::abort() noexcept{
    try{
        std::lock_guard<Mutex> lg(m_lock);
        m_aborted = true;
        m_cv.notify_all();
    }catch (...){}
    m_socket.close();
}


std::string Connection::command(std::string_view line){
    std::lock_guard<Mutex> command_lg(m_command_lock);
    std::string_view name = line.substr(0, line.find(' '));
    //  A line the bridge sent unasked ("ERR busy" right after connecting)
    //  is still read as this command's reply, even after the bridge closed.
    std::string data(line);
    data += '\n';
    bool sent = m_socket.send(data.data(), data.size(), m_timeout) == data.size();
    bool nothing_to_read;
    {
        std::lock_guard<Mutex> lg(m_lock);
        nothing_to_read = !sent && m_replies.empty() && !m_aborted;
    }
    if (nothing_to_read){
        fail(std::format("Unable to send {}.", name));
    }
    return wait_for_reply(name);
}

std::string Connection::wait_for_reply(std::string_view command_name){
    std::unique_lock<Mutex> lg(m_lock);
    bool ready = m_cv.wait_for(lg, m_timeout, [this]{
        return m_aborted || !m_replies.empty() || m_disconnect_error.has_value();
    });
    if (m_aborted || m_replies.empty()){
        std::string reason;
        if (m_aborted){
            reason = "aborted";
        }else if (ready){
            reason = std::format("connection lost: {}", *m_disconnect_error);
        }else{
            reason = "timed out";
            //  A late reply would pair with the next command: drop the link.
            m_disconnect_error = std::format("no reply to {}", command_name);
        }
        lg.unlock();
        m_socket.close();
        fail(std::format("No reply to {} ({}).", command_name, reason));
    }
    std::string reply = std::move(m_replies.front());
    m_replies.pop_front();
    lg.unlock();
    if (reply.starts_with("ERR")){
        fail(std::format("{} -> {}", command_name, reply));
    }
    return reply;
}

FrameReply Connection::frame(){
    std::string reply = command("FRAME");
    std::optional<FrameReply> frame = parse_frame_reply(reply);
    if (!frame){
        fail(std::format("Unexpected FRAME reply: {}", reply));
    }
    return std::move(*frame);
}

uint64_t Connection::keys(uint64_t frame, uint16_t mask){
    std::string reply = command(std::format("KEYS {} {}", frame, mask));
    std::optional<uint64_t> late = parse_keys_reply(reply);
    if (!late){
        fail(std::format("Unexpected KEYS reply: {}", reply));
    }
    if (*late > 0){
        m_logger.log(
            std::format("MgbaLink: KEYS for frame {} was {} frame(s) late.", frame, *late),
            COLOR_ORANGE
        );
    }
    return *late;
}

void Connection::fail(std::string message){
    throw ConnectionException(&m_logger, std::format("MgbaLink: {}", message));
}



void Connection::on_connect_finished(const std::string& error_message){
    std::lock_guard<Mutex> lg(m_lock);
    m_connect_error = error_message;
    m_disconnect_error.reset();
    m_splitter = LineSplitter();
    m_cv.notify_all();
}

void Connection::on_receive_data(const void* data, size_t bytes){
    bool overflowed;
    {
        std::lock_guard<Mutex> lg(m_lock);
        for (std::string& line : m_splitter.push(std::string_view(static_cast<const char*>(data), bytes))){
            m_replies.emplace_back(std::move(line));
        }
        overflowed = m_splitter.overflowed();
        if (overflowed){
            m_disconnect_error = std::format("reply line longer than {} bytes", LineSplitter::MAX_LINE_BYTES);
        }
        m_cv.notify_all();
    }
    if (overflowed){
        m_socket.close();
    }
}

void Connection::on_disconnect(const std::string& error_message){
    std::lock_guard<Mutex> lg(m_lock);
    if (!m_disconnect_error){
        m_disconnect_error = error_message.empty() ? "closed by the bridge" : error_message;
    }
    m_cv.notify_all();
}



}  // namespace MgbaLink
}  // namespace PokemonAutomation

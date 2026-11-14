/*  MgbaLink Connection
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "MgbaLink_Connection.h"

namespace PokemonAutomation{
namespace MgbaLink{



MgbaLink_Connection::MgbaLink_Connection(
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
MgbaLink_Connection::~MgbaLink_Connection(){
    close();
    m_socket.remove_listener(*this);
}


HelloReply MgbaLink_Connection::open(){
    connect_socket();
    std::string reply = command("HELLO 1");
    std::optional<HelloReply> hello = parse_hello_reply(reply);
    if (!hello){
        fail("Unexpected HELLO reply: " + reply);
    }
    m_logger.log(
        "MgbaLink: connected to " + m_host + ":" + std::to_string(m_port)
        + ", game " + hello->game_code + ", " + hello->bridge_version + ", mode " + hello->mode + "."
    );
    return *hello;
}

void MgbaLink_Connection::connect_socket(){
    m_socket.connect(m_host, m_port);
    std::unique_lock<Mutex> lg(m_lock);
    if (!m_cv.wait_for(lg, m_timeout, [this]{ return m_connect_error.has_value(); })){
        lg.unlock();
        fail("Timed out connecting to " + m_host + ":" + std::to_string(m_port) + ".");
    }
    if (!m_connect_error->empty()){
        std::string error = *m_connect_error;
        lg.unlock();
        fail("Unable to connect to " + m_host + ":" + std::to_string(m_port) + ": " + error);
    }
}

void MgbaLink_Connection::close() noexcept{
    if (m_socket.state() == AbstractClientSocket::State::CONNECTED){
        try{
            command("BYE");
        }catch (...){}
    }
    m_socket.close();
}


std::string MgbaLink_Connection::command(std::string_view line){
    std::lock_guard<Mutex> command_lg(m_command_lock);
    std::string_view name = line.substr(0, line.find(' '));
    //  A line the bridge sent unasked ("ERR busy" right after connecting)
    //  is still read as this command's reply, even after the bridge closed.
    std::string data(line);
    data += '\n';
    bool sent = m_socket.send(data.data(), data.size(), m_timeout) == data.size();
    {
        std::lock_guard<Mutex> lg(m_lock);
        if (!sent && m_replies.empty()){
            fail("Unable to send " + std::string(name) + ".");
        }
    }
    return wait_for_reply(name);
}

std::string MgbaLink_Connection::wait_for_reply(std::string_view command_name){
    std::unique_lock<Mutex> lg(m_lock);
    bool ready = m_cv.wait_for(lg, m_timeout, [this]{
        return !m_replies.empty() || m_disconnect_error.has_value();
    });
    if (m_replies.empty()){
        std::string reason = ready ? "connection lost: " + *m_disconnect_error : std::string("timed out");
        if (!ready){
            //  A late reply would pair with the next command: drop the link.
            m_disconnect_error = "no reply to " + std::string(command_name);
        }
        lg.unlock();
        m_socket.close();
        fail("No reply to " + std::string(command_name) + " (" + reason + ").");
    }
    std::string reply = std::move(m_replies.front());
    m_replies.pop_front();
    lg.unlock();
    if (reply.starts_with("ERR")){
        fail(std::string(command_name) + " -> " + reply);
    }
    return reply;
}

FrameReply MgbaLink_Connection::frame(){
    std::string reply = command("FRAME");
    std::optional<FrameReply> frame = parse_frame_reply(reply);
    if (!frame){
        fail("Unexpected FRAME reply: " + reply);
    }
    return std::move(*frame);
}

uint64_t MgbaLink_Connection::keys(uint64_t frame, uint16_t mask){
    std::string reply = command("KEYS " + std::to_string(frame) + " " + std::to_string(mask));
    std::optional<uint64_t> late = parse_keys_reply(reply);
    if (!late){
        fail("Unexpected KEYS reply: " + reply);
    }
    if (*late > 0){
        m_logger.log(
            "MgbaLink: KEYS for frame " + std::to_string(frame) + " was "
            + std::to_string(*late) + " frame(s) late.",
            COLOR_ORANGE
        );
    }
    return *late;
}

void MgbaLink_Connection::fail(std::string message){
    throw ConnectionException(&m_logger, "MgbaLink: " + std::move(message));
}



void MgbaLink_Connection::on_connect_finished(const std::string& error_message){
    std::lock_guard<Mutex> lg(m_lock);
    m_connect_error = error_message;
    m_disconnect_error.reset();
    m_splitter = LineSplitter();
    m_cv.notify_all();
}

void MgbaLink_Connection::on_receive_data(const void* data, size_t bytes){
    bool overflowed;
    {
        std::lock_guard<Mutex> lg(m_lock);
        for (std::string& line : m_splitter.push(std::string_view((const char*)data, bytes))){
            m_replies.emplace_back(std::move(line));
        }
        overflowed = m_splitter.overflowed();
        if (overflowed){
            m_disconnect_error = "reply line longer than " + std::to_string(MAX_LINE_BYTES) + " bytes";
        }
        m_cv.notify_all();
    }
    if (overflowed){
        m_socket.close();
    }
}

void MgbaLink_Connection::on_disconnect(const std::string& error_message){
    std::lock_guard<Mutex> lg(m_lock);
    if (!m_disconnect_error){
        m_disconnect_error = error_message.empty() ? "closed by the bridge" : error_message;
    }
    m_cv.notify_all();
}



}
}

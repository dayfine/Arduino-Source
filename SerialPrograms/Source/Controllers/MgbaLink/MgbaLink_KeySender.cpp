/*  MgbaLink Key Sender
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "MgbaLink_KeySender.h"

namespace PokemonAutomation{
namespace MgbaLink{

namespace{

//  How long a wait sleeps before it looks at its Cancellable again.
constexpr std::chrono::milliseconds CANCEL_POLL_PERIOD(50);

//  A release lasts one frame: frames_for_duration() of anything under
//  1.5 frames.
constexpr std::chrono::milliseconds ONE_FRAME(17);

//  "GRANT 0" holds the emulator where it is and says which frame that is.
uint64_t hold_and_read_frame(Logger& logger, Connection& connection){
    std::string reply = connection.command("GRANT 0");
    std::optional<uint64_t> frame = parse_ok_frame_reply(reply);
    if (!frame){
        throw ConnectionException(&logger, std::format("MgbaLink: unexpected GRANT reply: {}", reply));
    }
    return *frame;
}

}  // namespace


JumpCommand JumpCommand::load_state(std::string_view file){
    return JumpCommand(std::format("LOAD {}", file));
}
JumpCommand JumpCommand::reset(){
    return JumpCommand("RESET");
}


KeySender::KeySender(
    Logger& logger,
    ThreadPool& thread_pool,
    LinkRegistry& registry,
    std::string address,
    std::chrono::milliseconds grant_period
)
    : m_logger(logger)
    , m_thread_pool(thread_pool)
    , m_registry(registry)
    , m_address(std::move(address))
    , m_grant_period(grant_period)
{}
KeySender::~KeySender(){
    std::lock_guard<Mutex> lg(m_lock);
    m_pacer.reset();
    //  With the connection closed, the bridge has released keys and grant.
    if (!m_attachment || m_attachment->connection.expired()){
        return;
    }
    try{
        if (m_attachment->held_mask.value_or(0) != 0){
            restart_schedule(*m_attachment);
            push_now(*m_attachment, 0, ONE_FRAME);
        }
        connection(*m_attachment)->command(std::format("GRANT {}", RUN_ON_GRANT));
    }catch (Exception& e){
        m_logger.log("MgbaLink: could not release mGBA: " + e.message(), COLOR_ORANGE);
    }
}

bool KeySender::is_ready() const{
    return m_registry.find(m_address).has_value();
}

KeySender::Attachment& KeySender::attach(){
    std::optional<LinkRegistry::Link> link = m_registry.find(m_address);
    if (!link){
        throw ConnectionException(&m_logger, std::format(
            "MgbaLink: no connection to {}. Select \"mGBA ({})\" as the video source.",
            m_address, m_address
        ));
    }
    if (m_attachment && m_attachment->serial == link->serial){
        //  No pacer: a LOAD or RESET lost the connection before granting again.
        if (!m_pacer || !m_pacer->running()){
            throw ConnectionException(&m_logger, "MgbaLink: the connection to mGBA was lost.");
        }
        return *m_attachment;
    }

    //  A new connection (first use, or the video source reconnected).
    m_pacer.reset();
    m_attachment.reset();
    m_input_log = InputLog();

    //  The frame the emulator is held on is the anchor. The pacer releases
    //  it from there.
    WallClock now = current_time();
    uint64_t frame = hold_and_read_frame(m_logger, *link->connection);
    FrameClock clock(now, frame);
    m_attachment.emplace(Attachment{
        .serial = link->serial,
        .connection = link->connection,
        .clock = clock,
        .schedule = KeySchedule(clock),
    });
    m_pacer.emplace(m_logger, m_thread_pool, link->connection, clock, m_grant_period);
    m_logger.log(std::format("MgbaLink: controller on {} anchored at frame {}.", m_address, frame));
    return *m_attachment;
}

std::shared_ptr<Connection> KeySender::connection(const Attachment& attachment){
    std::shared_ptr<Connection> locked = attachment.connection.lock();
    if (!locked){
        throw ConnectionException(&m_logger, "MgbaLink: the connection to mGBA was closed.");
    }
    return locked;
}

void KeySender::restart_schedule(Attachment& attachment){
    //  Commands already sent stay queued in the bridge, and the last one
    //  queued for a frame wins: start after the last one sent.
    uint64_t next = attachment.last_sent_frame
        ? *attachment.last_sent_frame + 1
        : attachment.clock.anchor_frame();
    attachment.schedule = KeySchedule(FrameClock(attachment.clock.timestamp(next), next));
    attachment.held_mask.reset();
}

void KeySender::push_now(Attachment& attachment, uint16_t mask, WallDuration duration){
    std::optional<KeysCommand> command = attachment.schedule.push(current_time(), mask, duration);
    if (duration > WallDuration::zero()){
        attachment.held_mask = mask;
    }
    if (command){
        send(attachment, *command);
    }
}

void KeySender::send(Attachment& attachment, const KeysCommand& command){
    uint64_t late = connection(attachment)->keys(command.frame, command.mask);
    m_input_log.record(command, late);
    attachment.last_sent_frame = std::max(attachment.last_sent_frame.value_or(0), command.frame + late);
    if (late != 0){
        m_logger.log(
            std::format("MgbaLink: KEYS for frame {} landed {} frame(s) late.", command.frame, late),
            COLOR_ORANGE
        );
    }
}

bool KeySender::wait_for_frame(
    std::unique_lock<Mutex>& lg, Cancellable* cancellable, uint64_t frame, uint64_t epoch
){
    while (true){
        if (cancellable != nullptr){
            cancellable->throw_if_cancelled();
        }
        if (m_epoch != epoch || !m_attachment){
            return false;
        }
        WallClock now = current_time();
        WallClock target = m_attachment->clock.timestamp(frame);
        if (now >= target){
            return true;
        }
        WallDuration remaining = target - now;
        m_cv.wait_for(lg, std::min<WallDuration>(remaining, CANCEL_POLL_PERIOD));
    }
}

void KeySender::push(Cancellable* cancellable, uint16_t mask, WallDuration duration){
    std::unique_lock<Mutex> lg(m_lock);
    uint64_t epoch = m_epoch;
    Attachment* attachment = &attach();

    uint64_t start = attachment->schedule.end_frame();
    if (start > SEND_AHEAD_FRAMES){
        if (!wait_for_frame(lg, cancellable, start - SEND_AHEAD_FRAMES, epoch)){
            return;
        }
        //  The connection may have changed while waiting.
        attachment = &attach();
    }
    if (m_replace_pending){
        m_replace_pending = false;
        restart_schedule(*attachment);
    }
    push_now(*attachment, mask, duration);
}

void KeySender::wait_for_all(Cancellable* cancellable){
    bool holding = false;
    {
        std::lock_guard<Mutex> lg(m_lock);
        holding = m_attachment && m_attachment->held_mask.value_or(0) != 0;
    }
    if (holding){
        push(cancellable, 0, ONE_FRAME);
    }
    std::unique_lock<Mutex> lg(m_lock);
    if (!m_attachment){
        return;
    }
    wait_for_frame(lg, cancellable, m_attachment->schedule.end_frame(), m_epoch);
}

void KeySender::cancel(){
    std::lock_guard<Mutex> lg(m_lock);
    m_epoch++;
    m_replace_pending = false;
    m_cv.notify_all();
    if (!m_attachment){
        return;
    }
    restart_schedule(*m_attachment);
    try{
        push_now(*m_attachment, 0, ONE_FRAME);
    }catch (ConnectionException&){
        //  Logged by the exception. With the connection gone, the bridge
        //  has released the keys itself.
    }
}

void KeySender::replace_on_next(){
    std::lock_guard<Mutex> lg(m_lock);
    m_epoch++;
    m_replace_pending = true;
    m_cv.notify_all();
}

uint64_t KeySender::jump(const JumpCommand& command){
    std::lock_guard<Mutex> lg(m_lock);
    //  What waits to be sent was meant for the frames before the jump.
    m_epoch++;
    m_replace_pending = false;
    m_cv.notify_all();
    Attachment& attachment = attach();
    std::shared_ptr<Connection> link = connection(attachment);
    std::string_view line = command.line();
    std::string_view name = line.substr(0, line.find(' '));

    //  Stop granting first: a grant on the old frames would stall mGBA
    //  after a jump ahead, or let it run free after a jump back.
    m_pacer.reset();
    WallClock now;
    uint64_t frame = 0;
    try{
        link->command(line);
        now = current_time();
        frame = hold_and_read_frame(m_logger, *link);
    }catch (ConnectionException&){
        //  A refused command leaves mGBA where it was: the old clock holds.
        m_pacer.emplace(m_logger, m_thread_pool, attachment.connection, attachment.clock, m_grant_period);
        throw;
    }

    //  The bridge dropped its queued KEYS but still holds the last keys it
    //  applied: release them on the first frame of the new timeline.
    bool keys_sent = attachment.last_sent_frame.has_value();
    attachment.clock = FrameClock(now, frame);
    attachment.last_sent_frame.reset();
    m_input_log = InputLog();
    if (keys_sent){
        send(attachment, KeysCommand{frame, 0});
    }
    restart_schedule(attachment);
    m_pacer.emplace(m_logger, m_thread_pool, attachment.connection, attachment.clock, m_grant_period);
    m_logger.log(std::format("MgbaLink: {} on {}: re-anchored at frame {}.", name, m_address, frame));
    return frame;
}

std::string KeySender::input_log_text() const{
    std::lock_guard<Mutex> lg(m_lock);
    return m_input_log.to_text();
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation

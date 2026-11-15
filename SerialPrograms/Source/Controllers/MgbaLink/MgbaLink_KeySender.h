/*  MgbaLink Key Sender
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The controller side of one mGBA instance: it takes the connection the
 *  video source published for the same "host:port" (the bridge holds one
 *  client), anchors a frame clock with "GRANT 0" (which also holds the
 *  emulator until the pacer grants), runs the pacer, and turns key states
 *  held for durations into "KEYS" commands on frames. Every command it sends
 *  goes into the input log at the frame it landed on.
 *
 *  The bridge cannot take back a queued KEYS, so a state is sent only when
 *  it starts at most SEND_AHEAD_FRAMES after app time: push() blocks until
 *  then. A cancel then drops what is not sent yet and releases the keys at
 *  the first frame no sent command can override.
 *
 *  LOAD and RESET move mGBA to another frame, under the clock's anchor. The
 *  sender stops granting, drops what is not sent yet, sends the command
 *  (the bridge clears its queued KEYS and holds at the new frame), reads
 *  the new frame with "GRANT 0", anchors a new clock and input log there,
 *  releases the keys on that frame, and grants again.
 *
 *  Qt-free. Thread-safe: push() and wait_for_all() come from the issuing
 *  thread; cancel(), replace_on_next() and jump() from any thread.
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_KeySender_H
#define PokemonAutomation_Controllers_MgbaLink_KeySender_H

#include <stdint.h>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Concurrency/ThreadPool.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "MgbaLink_Keys.h"
#include "MgbaLink_Pacer.h"
#include "MgbaLink_Protocol.h"
#include "MgbaLink_SharedLink.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  "LOAD <file>" or "RESET": a command that moves mGBA to another frame.
class JumpCommand{
public:
    //  Loads the mGBA save state "file" (a path on the Mac running mGBA).
    static JumpCommand load_state(std::string_view file);
    //  Resets the GBA.
    static JumpCommand reset();

    const std::string& line() const{
        return m_line;
    }

private:
    explicit JumpCommand(std::string line)
        : m_line(std::move(line))
    {}

    std::string m_line;
};


//  Sends one controller's key states to the mGBA published under an address.
class KeySender{
public:
    //  Three frames past the pacer's lead (84 ms): a KEYS sent this early
    //  reaches the bridge before the emulator may run its frame, even if
    //  the sending thread wakes a frame or two late. It is also how long a
    //  cancelled state can still play.
    static constexpr uint64_t SEND_AHEAD_FRAMES = KeySchedule::LEAD_FRAMES + 3;

    //  Granted when the sender stops, so mGBA runs on instead of stalling
    //  at the last grant. Never reached.
    static constexpr uint64_t RUN_ON_GRANT = (uint64_t)1 << 62;

    KeySender(
        Logger& logger,
        ThreadPool& thread_pool,
        LinkRegistry& registry,
        std::string address,
        std::chrono::milliseconds grant_period = Pacer::DEFAULT_PERIOD
    );

    //  Stops the pacer, releases held keys and lets mGBA run on (best effort).
    ~KeySender();

    //  A connection is published for the address.
    bool is_ready() const;

    //  Holds GBA keys "mask" for "duration" after what is already scheduled.
    //  Blocks until that start is at most SEND_AHEAD_FRAMES ahead of app
    //  time; returns without sending if cancel() or replace_on_next() comes
    //  first. Throws ConnectionException if no connection is published or
    //  it is lost, OperationCancelledException if "cancellable" is cancelled.
    void push(Cancellable* cancellable, uint16_t mask, WallDuration duration);

    //  Releases the keys after the last state, then returns once app time
    //  reaches the end of the schedule.
    void wait_for_all(Cancellable* cancellable);

    //  Drops what is not sent yet and releases the keys.
    void cancel();

    //  Drops what is not sent yet; the next push() starts as soon as it can
    //  and the keys it holds replace the current ones.
    void replace_on_next();

    //  Sends "command" and re-anchors the clock on the frame mGBA is on
    //  after it. Drops what is not sent yet: a waiting push() returns
    //  without sending. Returns the new anchor frame. Throws
    //  ConnectionException if no connection is published, it is lost, or
    //  the bridge refuses ("ERR load failed"); a refused command leaves the
    //  old clock running.
    uint64_t jump(const JumpCommand& command);

    //  "<frame> <mask>" per change since the clock was anchored (at connect,
    //  or at the last LOAD or RESET).
    std::string input_log_text() const;

private:
    //  The connection stays the video source's: the sender holds it only
    //  for the length of a command.
    struct Attachment{
        uint64_t serial = 0;
        std::weak_ptr<Connection> connection;
        FrameClock clock;
        KeySchedule schedule;
        std::optional<uint16_t> held_mask = std::nullopt;
        std::optional<uint64_t> last_sent_frame = std::nullopt;
    };

    Attachment& attach();
    std::shared_ptr<Connection> connection(const Attachment& attachment);
    void restart_schedule(Attachment& attachment);
    void push_now(Attachment& attachment, uint16_t mask, WallDuration duration);
    void send(Attachment& attachment, const KeysCommand& command);
    //  Waits until app time reaches "frame" (in "lg"). False if a cancel
    //  came first.
    bool wait_for_frame(std::unique_lock<Mutex>& lg, Cancellable* cancellable, uint64_t frame, uint64_t epoch);

private:
    Logger& m_logger;
    ThreadPool& m_thread_pool;
    LinkRegistry& m_registry;
    const std::string m_address;
    const std::chrono::milliseconds m_grant_period;

    mutable Mutex m_lock;
    ConditionVariable m_cv;
    //  Bumped by cancel() and replace_on_next(): a waiting push() drops its state.
    uint64_t m_epoch = 0;
    bool m_replace_pending = false;
    std::optional<Attachment> m_attachment;
    InputLog m_input_log;
    //  Declared after the attachment: destroyed first, so it stops granting
    //  before the connection is released.
    std::optional<Pacer> m_pacer;
};


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif

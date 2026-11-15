/*  MgbaLink Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <atomic>
#include <chrono>
#include <format>
#include <functional>
#include <random>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Concurrency/AsyncTask.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#ifdef _WIN32
#include "Common/Cpp/Sockets/ServerSocket_WinSocket.h"
#else
#include "Common/Cpp/Sockets/ServerSocket_POSIX.h"
#endif
#include "MgbaLink_Connection.h"
#include "MgbaLink_Protocol.h"
#include "MgbaLink_Tests.h"

namespace PokemonAutomation{
namespace MgbaLink{

namespace{

#ifdef _WIN32
using FakeServerSocket = ServerSocket_WinSocket;
#else
using FakeServerSocket = ServerSocket_POSIX;
#endif


//  Line framing over a byte stream.
struct LineFramingInput{
    std::vector<std::string> chunks;
    size_t max_line;
};
struct LineFramingExpectation{
    std::vector<std::string> lines;
    bool overflowed;
};
struct LineFramingCase{
    std::string name;
    LineFramingInput input;
    LineFramingExpectation expectation;
};

class Test_LineFraming : public UnitTest{
public:
    Test_LineFraming(LineFramingCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - line framing - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LineSplitter splitter(m_case.input.max_line);
        std::vector<std::string> lines;
        for (const std::string& chunk : m_case.input.chunks){
            for (std::string& line : splitter.push(chunk)){
                lines.emplace_back(std::move(line));
            }
        }
        if (lines != m_case.expectation.lines || splitter.overflowed() != m_case.expectation.overflowed){
            return std::format("got {} line(s), overflowed={}", lines.size(), splitter.overflowed());
        }
        return true;
    }

private:
    LineFramingCase m_case;
};



//  Reply parsing. Each parser's result is printed as one string, so one table
//  covers all reply kinds; nullopt means the reply was rejected.
enum class ReplyKind{
    HELLO,
    OK_FRAME,
    KEYS,
    FRAME,
};
struct ReplyInput{
    ReplyKind kind;
    std::string reply;
};
struct ReplyExpectation{
    std::optional<std::string> parsed;
};
struct ReplyCase{
    std::string name;
    ReplyInput input;
    ReplyExpectation expectation;
};

std::optional<std::string> describe_reply(const ReplyInput& input){
    switch (input.kind){
    case ReplyKind::HELLO:{
        std::optional<HelloReply> hello = parse_hello_reply(input.reply);
        if (!hello){
            return std::nullopt;
        }
        return std::format("{}|{}|{}", hello->game_code, hello->bridge_version, hello->mode);
    }
    case ReplyKind::OK_FRAME:{
        std::optional<uint64_t> frame = parse_ok_frame_reply(input.reply);
        return frame ? std::optional<std::string>(std::to_string(*frame)) : std::nullopt;
    }
    case ReplyKind::KEYS:{
        std::optional<uint64_t> late = parse_keys_reply(input.reply);
        return late ? std::optional<std::string>(std::to_string(*late)) : std::nullopt;
    }
    case ReplyKind::FRAME:{
        std::optional<FrameReply> frame = parse_frame_reply(input.reply);
        if (!frame){
            return std::nullopt;
        }
        return std::format("{}|{}|{}|{}", frame->frame, frame->width, frame->height, frame->path);
    }
    }
    return std::nullopt;
}

class Test_Reply : public UnitTest{
public:
    Test_Reply(ReplyCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - reply - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        std::optional<std::string> parsed = describe_reply(m_case.input);
        if (parsed != m_case.expectation.parsed){
            return std::format("parsed as <{}>", parsed.value_or("(rejected)"));
        }
        return true;
    }

private:
    ReplyCase m_case;
};



//  Frame numbers to timestamps, anchored at (t0, frame 1000).
struct FrameClockInput{
    uint64_t frame;
};
struct FrameClockExpectation{
    std::chrono::microseconds offset_from_anchor;
};
struct FrameClockCase{
    std::string name;
    FrameClockInput input;
    FrameClockExpectation expectation;
};

const WallClock CLOCK_T0 = WallClock() + std::chrono::hours(1000);

class Test_FrameClock : public UnitTest{
public:
    Test_FrameClock(FrameClockCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - frame clock - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FrameClock clock(CLOCK_T0, 1000);
        WallDuration offset = clock.timestamp(m_case.input.frame) - CLOCK_T0;
        if (offset != std::chrono::duration_cast<WallDuration>(m_case.expectation.offset_from_anchor)){
            return std::format("offset {} us", std::chrono::duration_cast<std::chrono::microseconds>(offset).count());
        }
        return true;
    }

private:
    FrameClockCase m_case;
};



//  Stamps for a series of reads, the clock anchored at (t0, frame 1000).
//  Frame 1060 maps to 1004.6 ms after the anchor.
struct ClockRead{
    uint64_t frame = 0;
    std::chrono::milliseconds read_after_anchor{};
};
ClockRead frame_read_at(uint64_t frame, std::chrono::milliseconds read_after_anchor){
    return ClockRead{frame, read_after_anchor};
}
struct StampInput{
    std::vector<ClockRead> reads;
};
struct StampExpectation{
    std::vector<std::chrono::milliseconds> stamps_after_anchor;
    size_t lag_reanchors = 0;
};
struct StampCase{
    std::string name;
    StampInput input;
    StampExpectation expectation;
};

std::string join_milliseconds(const std::vector<std::chrono::milliseconds>& values){
    std::string text;
    for (std::chrono::milliseconds value : values){
        text += text.empty() ? "" : ", ";
        text += std::to_string(value.count());
    }
    return text;
}

class Test_Stamp : public UnitTest{
public:
    Test_Stamp(StampCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - frame stamps - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FrameClock clock(CLOCK_T0, 1000);
        std::vector<std::chrono::milliseconds> stamps;
        size_t lag_reanchors = 0;
        for (const ClockRead& read : m_case.input.reads){
            FrameClock::Stamp stamp = clock.stamp(CLOCK_T0 + read.read_after_anchor, read.frame, std::chrono::milliseconds(500));
            stamps.emplace_back(std::chrono::duration_cast<std::chrono::milliseconds>(stamp.time - CLOCK_T0));
            lag_reanchors += stamp.reanchored_after_lag ? 1 : 0;
        }
        if (stamps != m_case.expectation.stamps_after_anchor || lag_reanchors != m_case.expectation.lag_reanchors){
            return std::format(
                "stamps [{}] ms with {} lag re-anchor(s), expected [{}] ms with {}",
                join_milliseconds(stamps), lag_reanchors,
                join_milliseconds(m_case.expectation.stamps_after_anchor), m_case.expectation.lag_reanchors
            );
        }
        return true;
    }

private:
    StampCase m_case;
};



//  How the fake bridge treats its client.
enum class BridgeMode{
    ANSWERS,    //  Answers each command line from a script.
    BUSY,       //  Refuses the client with "ERR busy", as a bridge that has one.
    HANGS,      //  Answers HELLO, then never answers again.
};

//  A bridge stand-in.
class FakeBridge : public AbstractServerSocket::Listener{
public:
    FakeBridge(ThreadPool& thread_pool, BridgeMode mode)
        : m_server(thread_pool)
        , m_mode(mode)
    {
        m_server.add_listener(*this);
    }
    ~FakeBridge(){
        m_server.remove_listener(*this);
    }

    //  Listens on a random port in 40000..59999; nullopt if 8 tries fail.
    std::optional<uint16_t> start();

    std::vector<std::string> commands(){
        std::lock_guard<Mutex> lg(m_lock);
        return m_commands;
    }

    //  Waits up to 5 s for the client to send "count" commands.
    bool wait_for_commands(size_t count){
        std::unique_lock<Mutex> lg(m_lock);
        return m_cv.wait_for(lg, std::chrono::seconds(5), [&]{ return m_commands.size() >= count; });
    }

private:
    virtual void on_listen_finished(const std::string& error_message) override{
        std::lock_guard<Mutex> lg(m_lock);
        m_listen_error = error_message;
        m_cv.notify_all();
    }
    virtual void on_connect() override{
        if (m_mode == BridgeMode::BUSY){
            //  Closed once the client's first line is read (no reset that
            //  could drop this line before the client reads it).
            send("ERR busy\n");
        }
    }
    virtual void on_receive_data(const void* data, size_t bytes) override;

    void send(std::string_view text){
        m_server.send(text.data(), text.size());
    }

    static std::string reply_for(std::string_view line);

private:
    FakeServerSocket m_server;
    const BridgeMode m_mode;
    LineSplitter m_splitter;
    Mutex m_lock;
    ConditionVariable m_cv;
    std::optional<std::string> m_listen_error;
    std::vector<std::string> m_commands;
};

std::optional<uint16_t> FakeBridge::start(){
    std::mt19937 rng{std::random_device{}()};
    for (int attempt = 0; attempt < 8; attempt++){
        uint16_t port = (uint16_t)(40000 + rng() % 20000);
        {
            std::lock_guard<Mutex> lg(m_lock);
            m_listen_error.reset();
        }
        m_server.start("127.0.0.1", port);
        std::unique_lock<Mutex> lg(m_lock);
        m_cv.wait_for(lg, std::chrono::seconds(10), [this]{ return m_listen_error.has_value(); });
        if (m_listen_error && m_listen_error->empty()){
            return port;
        }
    }
    return std::nullopt;
}

void FakeBridge::on_receive_data(const void* data, size_t bytes){
    if (m_mode == BridgeMode::BUSY){
        //  Record what the client sent, then refuse it.
        std::lock_guard<Mutex> lg(m_lock);
        m_commands.emplace_back("(refused)");
        m_cv.notify_all();
        m_server.close_connection();
        return;
    }
    std::vector<std::string> lines = m_splitter.push(std::string_view(static_cast<const char*>(data), bytes));
    for (const std::string& line : lines){
        {
            std::lock_guard<Mutex> lg(m_lock);
            m_commands.emplace_back(line);
            m_cv.notify_all();
        }
        if (m_mode == BridgeMode::HANGS && line != "HELLO 1"){
            continue;
        }
        std::string reply = reply_for(line);
        //  Two writes, so the client has to reassemble the line.
        send(std::string_view(reply).substr(0, 3));
        send(std::string_view(reply).substr(3));
        if (line == "BYE"){
            m_server.close_connection();
        }
    }
}

std::string FakeBridge::reply_for(std::string_view line){
    //  The emulator is about to run frame 120.
    if (line == "HELLO 1"){
        return "OK 1 BPRE bridge-0.1 paced\r\n";
    }
    if (line == "FRAME"){
        return "FRAME 120 240 160 /tmp/mgba bridge.png\n";
    }
    if (line == "KEYS 100 1"){
        return "LATE 20\n";
    }
    if (line == "KEYS 130 1"){
        return "OK\n";
    }
    if (line == "BYE"){
        return "OK BYE\n";
    }
    return std::format("ERR unknown {}\n", line.substr(0, line.find(' ')));
}



//  A whole session against the fake bridge: open (twice, if asked), FRAME,
//  KEYS late and on time, an ERR reply, then close (BYE).
struct SessionInput{
    BridgeMode bridge_mode = BridgeMode::ANSWERS;
    bool open_twice = false;
    //  Abort from another thread once the bridge has seen FRAME.
    bool abort_after_frame = false;
};
struct SessionExpectation{
    std::vector<std::string> results;
    std::vector<std::string> bridge_saw;
};
struct SessionCase{
    std::string name;
    SessionInput input;
    SessionExpectation expectation;
};

std::string join_lines(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += text.empty() ? "<" : ", <";
        text += line;
        text += ">";
    }
    return text;
}

void run_commands(Connection& connection, std::vector<std::string>& results){
    FrameReply frame = connection.frame();
    results.emplace_back(std::format("frame {} {}", frame.frame, frame.path));
    results.emplace_back(std::format("late {}", connection.keys(100, 1)));
    results.emplace_back(std::format("late {}", connection.keys(130, 1)));
    connection.command("FLY");
    results.emplace_back("FLY accepted");
}

std::vector<std::string> run_session(
    Logger& logger, ThreadPool& thread_pool, FakeBridge& bridge, uint16_t port, const SessionInput& input
){
    std::vector<std::string> results;
    Connection connection(logger, thread_pool, "127.0.0.1", port);
    AsyncTask aborter;
    try{
        HelloReply hello = connection.open();
        results.emplace_back(std::format("hello {} {}", hello.game_code, hello.mode));
        if (input.open_twice){
            try{
                connection.open();
                results.emplace_back("second open accepted");
            }catch (InternalProgramError&){
                results.emplace_back("second open refused");
            }
        }
        if (input.abort_after_frame){
            aborter = thread_pool.dispatch_now_blocking([&]{
                bridge.wait_for_commands(2);
                connection.abort();
            });
        }
        run_commands(connection, results);
    }catch (ConnectionException& e){
        bool busy = e.message().find("ERR busy") != std::string::npos;
        results.emplace_back(busy ? "failed: ERR busy" : std::format("failed: {}", e.message()));
    }
    aborter.wait_and_ignore_exceptions();
    return results;
}

class Test_Session : public UnitTest{
public:
    Test_Session(SessionCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - fake bridge - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        ThreadPool& thread_pool = GlobalThreadPools::unlimited_normal();
        FakeBridge bridge(thread_pool, m_case.input.bridge_mode);
        std::optional<uint16_t> port = bridge.start();
        if (!port){
            return UnitTestResult(UnitTestResult::SKIPPED, "No free port for the fake bridge.");
        }
        std::vector<std::string> results = run_session(logger, thread_pool, bridge, *port, m_case.input);
        if (results != m_case.expectation.results){
            return std::format("results {}, expected {}", join_lines(results), join_lines(m_case.expectation.results));
        }
        bridge.wait_for_commands(m_case.expectation.bridge_saw.size());
        std::vector<std::string> saw = bridge.commands();
        if (saw != m_case.expectation.bridge_saw){
            return std::format("the bridge saw {}, expected {}", join_lines(saw), join_lines(m_case.expectation.bridge_saw));
        }
        return true;
    }

private:
    SessionCase m_case;
};


void add_framing_and_reply_tests(UnitTestDatabase& database){
    const LineFramingCase LINE_FRAMING_CASES[] = {
        {.name = "one line", .input = {{"OK\n"}, 16}, .expectation = {{"OK"}, false}},
        {.name = "CR before LF dropped", .input = {{"OK 5\r\n"}, 16}, .expectation = {{"OK 5"}, false}},
        {.name = "line split across chunks", .input = {{"FRAME 12", "0 240 160 /a b.png\nOK"}, 64}, .expectation = {{"FRAME 120 240 160 /a b.png"}, false}},
        {.name = "two lines in one chunk", .input = {{"OK\nLATE 3\n"}, 16}, .expectation = {{"OK", "LATE 3"}, false}},
        {.name = "empty line", .input = {{"\n"}, 16}, .expectation = {{""}, false}},
        {.name = "exactly max", .input = {{"aaaaaaaa\r\n"}, 8}, .expectation = {{"aaaaaaaa"}, false}},
        {.name = "over max without LF", .input = {{"aaaaaaaaaa"}, 8}, .expectation = {{}, true}},
        {.name = "over max across chunks", .input = {{"aaaaa", "aaaaa\n"}, 8}, .expectation = {{}, true}},
    };
    for (const LineFramingCase& test_case : LINE_FRAMING_CASES){
        database.add<Test_LineFraming>(test_case);
    }

    const ReplyCase REPLY_CASES[] = {
        {.name = "HELLO v1", .input = {ReplyKind::HELLO, "OK 1 BPRE bridge-0.1 paced"}, .expectation = {"BPRE|bridge-0.1|paced"}},
        {.name = "HELLO wrong version", .input = {ReplyKind::HELLO, "OK 2 BPRE bridge-0.1 paced"}, .expectation = {std::nullopt}},
        {.name = "HELLO missing mode", .input = {ReplyKind::HELLO, "OK 1 BPRE bridge-0.1"}, .expectation = {std::nullopt}},
        {.name = "HELLO extra field", .input = {ReplyKind::HELLO, "OK 1 BPRE bridge-0.1 paced x"}, .expectation = {std::nullopt}},
        {.name = "HELLO busy", .input = {ReplyKind::HELLO, "ERR busy"}, .expectation = {std::nullopt}},
        {.name = "GRANT frame", .input = {ReplyKind::OK_FRAME, "OK 4521"}, .expectation = {"4521"}},
        {.name = "bare OK", .input = {ReplyKind::OK_FRAME, "OK"}, .expectation = {std::nullopt}},
        {.name = "BYE is not a frame", .input = {ReplyKind::OK_FRAME, "OK BYE"}, .expectation = {std::nullopt}},
        {.name = "KEYS on time", .input = {ReplyKind::KEYS, "OK"}, .expectation = {"0"}},
        {.name = "KEYS late", .input = {ReplyKind::KEYS, "LATE 3"}, .expectation = {"3"}},
        {.name = "KEYS late without count", .input = {ReplyKind::KEYS, "LATE"}, .expectation = {std::nullopt}},
        {.name = "KEYS usage error", .input = {ReplyKind::KEYS, "ERR usage KEYS <frame> <mask>"}, .expectation = {std::nullopt}},
        {.name = "FRAME on 0.10.5", .input = {ReplyKind::FRAME, "FRAME 1234 240 160 /tmp/mgba-bridge-47801.png"}, .expectation = {"1234|240|160|/tmp/mgba-bridge-47801.png"}},
        {.name = "FRAME path with spaces", .input = {ReplyKind::FRAME, "FRAME 7 240 160 /Users/x/My Folder/f.png"}, .expectation = {"7|240|160|/Users/x/My Folder/f.png"}},
        {.name = "FRAME without path", .input = {ReplyKind::FRAME, "FRAME 7 240 160"}, .expectation = {std::nullopt}},
        {.name = "FRAME negative number", .input = {ReplyKind::FRAME, "FRAME -7 240 160 /f.png"}, .expectation = {std::nullopt}},
        {.name = "FRAME no game", .input = {ReplyKind::FRAME, "ERR no-game"}, .expectation = {std::nullopt}},
    };
    for (const ReplyCase& test_case : REPLY_CASES){
        database.add<Test_Reply>(test_case);
    }
}

void add_clock_tests(UnitTestDatabase& database){
    //  One frame = 1000 / 59.7275 ms = 16742.7 us.
    const FrameClockCase FRAME_CLOCK_CASES[] = {
        {.name = "anchor frame", .input = {1000}, .expectation = {std::chrono::microseconds(0)}},
        {.name = "next frame", .input = {1001}, .expectation = {std::chrono::microseconds(16743)}},
        {.name = "60 frames", .input = {1060}, .expectation = {std::chrono::microseconds(1004562)}},
        {.name = "before the anchor (after a LOAD)", .input = {940}, .expectation = {std::chrono::microseconds(-1004562)}},
    };
    for (const FrameClockCase& test_case : FRAME_CLOCK_CASES){
        database.add<Test_FrameClock>(test_case);
    }

    using std::chrono::milliseconds;
    const StampCase STAMP_CASES[] = {
        {.name = "small lag keeps the anchor", .input = {{frame_read_at(1060, milliseconds(1100))}}, .expectation = {{milliseconds(1004)}, 0}},
        {.name = "mGBA paused re-anchors at the read", .input = {{frame_read_at(1060, milliseconds(10000))}}, .expectation = {{milliseconds(10000)}, 1}},
        {.name = "mGBA running ahead is stamped at the read", .input = {{frame_read_at(1060, milliseconds(900))}}, .expectation = {{milliseconds(900)}, 0}},
        {
            .name = "mGBA running ahead stays at or before each read",
            .input = {{frame_read_at(1060, milliseconds(900)), frame_read_at(1120, milliseconds(1900))}},
            .expectation = {{milliseconds(900), milliseconds(1900)}, 0},
        },
        {
            .name = "frame number going back keeps stamps monotonic",
            .input = {{frame_read_at(1060, milliseconds(1010)), frame_read_at(1059, milliseconds(1012))}},
            .expectation = {{milliseconds(1004), milliseconds(1004)}, 0},
        },
    };
    for (const StampCase& test_case : STAMP_CASES){
        database.add<Test_Stamp>(test_case);
    }
}

void add_session_tests(UnitTestDatabase& database){
    const SessionCase SESSION_CASES[] = {
        {
            .name = "session",
            .input = {},
            .expectation = {
                {"hello BPRE paced", "frame 120 /tmp/mgba bridge.png", "late 20", "late 0", "failed: MgbaLink: FLY -> ERR unknown FLY"},
                {"HELLO 1", "FRAME", "KEYS 100 1", "KEYS 130 1", "FLY", "BYE"},
            },
        },
        {
            .name = "busy",
            .input = {.bridge_mode = BridgeMode::BUSY},
            .expectation = {{"failed: ERR busy"}, {"(refused)"}},
        },
        {
            .name = "second open refused",
            .input = {.open_twice = true},
            .expectation = {
                {"hello BPRE paced", "second open refused", "frame 120 /tmp/mgba bridge.png", "late 20", "late 0", "failed: MgbaLink: FLY -> ERR unknown FLY"},
                {"HELLO 1", "FRAME", "KEYS 100 1", "KEYS 130 1", "FLY", "BYE"},
            },
        },
        {
            //  Without abort() FRAME would wait out the 5 s timeout, then
            //  report "timed out"; aborted, it fails at once and skips BYE.
            .name = "abort while waiting for a reply",
            .input = {.bridge_mode = BridgeMode::HANGS, .abort_after_frame = true},
            .expectation = {
                {"hello BPRE paced", "failed: MgbaLink: No reply to FRAME (aborted)."},
                {"HELLO 1", "FRAME"},
            },
        },
    };
    for (const SessionCase& test_case : SESSION_CASES){
        database.add<Test_Session>(test_case);
    }
}

}  // namespace


void add_tests(UnitTestDatabase& database){
    add_framing_and_reply_tests(database);
    add_clock_tests(database);
    add_session_tests(database);
}



}  // namespace MgbaLink
}  // namespace PokemonAutomation

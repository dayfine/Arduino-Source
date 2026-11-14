/*  MgbaLink Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <atomic>
#include <format>
#include <functional>
#include <random>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Concurrency/ConditionVariable.h"
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
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


UnitTestResult test_line_splitter(){
    struct Input{
        std::vector<std::string> chunks;
        size_t max_line;
    };
    struct Expectation{
        std::vector<std::string> lines;
        bool overflowed;
    };
    struct Case{
        std::string name;
        Input input;
        Expectation expectation;
    };
    const Case CASES[] = {
        {"one line", {{"OK\n"}, 16}, {{"OK"}, false}},
        {"CR before LF dropped", {{"OK 5\r\n"}, 16}, {{"OK 5"}, false}},
        {"line split across chunks", {{"FRAME 12", "0 240 160 /a b.png\nOK"}, 64}, {{"FRAME 120 240 160 /a b.png"}, false}},
        {"two lines in one chunk", {{"OK\nLATE 3\n"}, 16}, {{"OK", "LATE 3"}, false}},
        {"empty line", {{"\n"}, 16}, {{""}, false}},
        {"exactly max", {{std::string(8, 'a') + "\r\n"}, 8}, {{std::string(8, 'a')}, false}},
        {"over max without LF", {{std::string(10, 'a')}, 8}, {{}, true}},
        {"over max across chunks", {{"aaaaa", "aaaaa\n"}, 8}, {{}, true}},
    };
    for (const Case& c : CASES){
        LineSplitter splitter(c.input.max_line);
        std::vector<std::string> lines;
        for (const std::string& chunk : c.input.chunks){
            for (std::string& line : splitter.push(chunk)){
                lines.emplace_back(std::move(line));
            }
        }
        if (lines != c.expectation.lines || splitter.overflowed() != c.expectation.overflowed){
            return std::format("{}: got {} line(s), overflowed={}", c.name, lines.size(), splitter.overflowed());
        }
    }
    return true;
}


UnitTestResult test_parse_hello(){
    struct Case{
        std::string name;
        std::string input;
        std::optional<std::string> expected_code;
    };
    const Case CASES[] = {
        {"v1 reply", "OK 1 BPRE bridge-0.1 paced", "BPRE"},
        {"wrong version", "OK 2 BPRE bridge-0.1 paced", std::nullopt},
        {"missing mode", "OK 1 BPRE bridge-0.1", std::nullopt},
        {"extra field", "OK 1 BPRE bridge-0.1 paced x", std::nullopt},
        {"error", "ERR busy", std::nullopt},
    };
    for (const Case& c : CASES){
        std::optional<HelloReply> hello = parse_hello_reply(c.input);
        std::optional<std::string> code;
        if (hello){
            code = hello->game_code;
        }
        if (code != c.expected_code){
            return c.name + ": unexpected result.";
        }
    }
    return true;
}


UnitTestResult test_parse_frame(){
    struct Expectation{
        uint64_t frame;
        size_t width;
        size_t height;
        std::string path;
    };
    struct Case{
        std::string name;
        std::string input;
        std::optional<Expectation> expectation;
    };
    const Case CASES[] = {
        {"0.10.5 reply", "FRAME 1234 240 160 /tmp/mgba-bridge-47801.png", Expectation{1234, 240, 160, "/tmp/mgba-bridge-47801.png"}},
        {"path with spaces", "FRAME 7 240 160 /Users/x/My Folder/f.png", Expectation{7, 240, 160, "/Users/x/My Folder/f.png"}},
        {"no path", "FRAME 7 240 160", std::nullopt},
        {"negative frame", "FRAME -7 240 160 /f.png", std::nullopt},
        {"error", "ERR no-game", std::nullopt},
    };
    for (const Case& c : CASES){
        std::optional<FrameReply> reply = parse_frame_reply(c.input);
        if (reply.has_value() != c.expectation.has_value()){
            return c.name + ": parsed=" + (reply ? "true" : "false");
        }
        if (reply && (
            reply->frame != c.expectation->frame || reply->width != c.expectation->width ||
            reply->height != c.expectation->height || reply->path != c.expectation->path
        )){
            return c.name + ": wrong fields.";
        }
    }
    return true;
}


UnitTestResult test_parse_keys_and_ok_frame(){
    struct Case{
        std::string name;
        std::function<std::optional<uint64_t>(std::string_view)> parser;
        std::string input;
        std::optional<uint64_t> expected;
    };
    const Case CASES[] = {
        {"KEYS on time", parse_keys_reply, "OK", 0},
        {"KEYS late", parse_keys_reply, "LATE 3", 3},
        {"KEYS late without count", parse_keys_reply, "LATE", std::nullopt},
        {"KEYS error", parse_keys_reply, "ERR usage KEYS <frame> <mask>", std::nullopt},
        {"GRANT reply", parse_ok_frame_reply, "OK 4521", 4521},
        {"bare OK", parse_ok_frame_reply, "OK", std::nullopt},
        {"OK with text", parse_ok_frame_reply, "OK BYE", std::nullopt},
    };
    for (const Case& c : CASES){
        if (c.parser(c.input) != c.expected){
            return c.name + ": unexpected result.";
        }
    }
    return true;
}


UnitTestResult test_frame_clock(){
    using std::chrono::microseconds;
    const WallClock t0 = WallClock() + std::chrono::hours(1000);
    struct Case{
        std::string name;
        uint64_t frame;
        microseconds expected_offset;
    };
    //  Anchored at frame 1000; one frame = 16742.7 us.
    const Case CASES[] = {
        {"anchor frame", 1000, microseconds(0)},
        {"next frame", 1001, microseconds(16743)},
        {"one second of frames", 1060, microseconds(1004562)},
        {"before the anchor (after a LOAD)", 940, microseconds(-1004562)},
    };
    FrameClock clock(t0, 1000);
    for (const Case& c : CASES){
        WallDuration offset = clock.timestamp(c.frame) - t0;
        if (offset != std::chrono::duration_cast<WallDuration>(c.expected_offset)){
            return std::format(
                "{}: offset {} us",
                c.name, std::chrono::duration_cast<microseconds>(offset).count()
            );
        }
    }
    if (clock.resync(t0 + std::chrono::milliseconds(1100), 1060, std::chrono::milliseconds(500))){
        return "resync: 95 ms of drift should keep the anchor.";
    }
    if (!clock.resync(t0 + std::chrono::seconds(10), 1060, std::chrono::milliseconds(500)) ||
        clock.timestamp(1060) != t0 + std::chrono::seconds(10)
    ){
        return "resync: 9 s of drift (mGBA paused) should re-anchor at the read time.";
    }
    return true;
}



//  A bridge stand-in: answers each command line from a script, or refuses
//  the client with "ERR busy" as a bridge that already has one.
class FakeBridge : public AbstractServerSocket::Listener{
public:
    FakeBridge(ThreadPool& thread_pool, bool busy)
        : m_server(thread_pool)
        , m_busy(busy)
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
        if (m_busy){
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
    const bool m_busy;
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
    if (m_busy){
        m_server.close_connection();
        return;
    }
    std::vector<std::string> lines = m_splitter.push(std::string_view((const char*)data, bytes));
    for (const std::string& line : lines){
        {
            std::lock_guard<Mutex> lg(m_lock);
            m_commands.emplace_back(line);
            m_cv.notify_all();
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
    return "ERR unknown " + std::string(line.substr(0, line.find(' '))) + "\n";
}



UnitTestResult test_session(Logger& logger, ThreadPool& thread_pool){
    FakeBridge bridge(thread_pool, false);
    std::optional<uint16_t> port = bridge.start();
    if (!port){
        return UnitTestResult(UnitTestResult::SKIPPED, "No free port for the fake bridge.");
    }
    {
        MgbaLink_Connection connection(logger, thread_pool, "127.0.0.1", *port);
        HelloReply hello = connection.open();
        if (hello.game_code != "BPRE" || hello.mode != "paced"){
            return "HELLO: wrong game code or mode.";
        }
        FrameReply frame = connection.frame();
        if (frame.frame != 120 || frame.path != "/tmp/mgba bridge.png"){
            return "FRAME: wrong frame number or path.";
        }
        if (connection.keys(100, 1) != 20 || connection.keys(130, 1) != 0){
            return "KEYS: expected LATE 20, then on time.";
        }
        try{
            connection.command("FLY");
            return "An ERR reply should throw.";
        }catch (ConnectionException&){}
    }
    const std::vector<std::string> expected{"HELLO 1", "FRAME", "KEYS 100 1", "KEYS 130 1", "FLY", "BYE"};
    if (!bridge.wait_for_commands(expected.size()) || bridge.commands() != expected){
        return std::format("The bridge saw {} command(s); the session should end with BYE.", bridge.commands().size());
    }
    return true;
}

UnitTestResult test_busy_bridge(Logger& logger, ThreadPool& thread_pool){
    FakeBridge bridge(thread_pool, true);
    std::optional<uint16_t> port = bridge.start();
    if (!port){
        return UnitTestResult(UnitTestResult::SKIPPED, "No free port for the fake bridge.");
    }
    MgbaLink_Connection connection(logger, thread_pool, "127.0.0.1", *port);
    try{
        connection.open();
    }catch (ConnectionException& e){
        if (e.message().find("ERR busy") == std::string::npos){
            return "Expected the bridge's ERR busy in the error, got: " + e.message();
        }
        return true;
    }
    return "open() should fail when the bridge already has a client.";
}



class Test_MgbaLink : public UnitTest{
public:
    using Body = std::function<UnitTestResult(Logger&, ThreadPool&)>;

    Test_MgbaLink(std::string name, ThreadPool& thread_pool, Body body)
        : UnitTest("Controllers::MgbaLink - " + std::move(name))
        , m_thread_pool(thread_pool)
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_body(logger, m_thread_pool);
    }

private:
    ThreadPool& m_thread_pool;
    Body m_body;
};

Test_MgbaLink::Body pure(UnitTestResult (*body)()){
    return [body](Logger&, ThreadPool&){ return body(); };
}

}


void add_tests(UnitTestDatabase& database, ThreadPool& thread_pool){
    database.add<Test_MgbaLink>("line framing", thread_pool, pure(test_line_splitter));
    database.add<Test_MgbaLink>("HELLO reply", thread_pool, pure(test_parse_hello));
    database.add<Test_MgbaLink>("FRAME reply", thread_pool, pure(test_parse_frame));
    database.add<Test_MgbaLink>("KEYS and OK replies", thread_pool, pure(test_parse_keys_and_ok_frame));
    database.add<Test_MgbaLink>("frame clock", thread_pool, pure(test_frame_clock));
    database.add<Test_MgbaLink>("fake bridge session", thread_pool, test_session);
    database.add<Test_MgbaLink>("fake bridge busy", thread_pool, test_busy_bridge);
}



}
}

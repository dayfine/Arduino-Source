/*  MgbaLink Key Sender Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <charconv>
#include <chrono>
#include <format>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "Common/Cpp/Exceptions.h"
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
#include "MgbaLink_KeySender.h"
#include "MgbaLink_KeySender_Tests.h"
#include "MgbaLink_Pacer.h"
#include "MgbaLink_SharedLink.h"

namespace PokemonAutomation{
namespace MgbaLink{

namespace{

#ifdef _WIN32
using FakeServerSocket = ServerSocket_WinSocket;
#else
using FakeServerSocket = ServerSocket_POSIX;
#endif

using std::chrono::milliseconds;

//  The fake emulator is always about to run this frame.
constexpr uint64_t BRIDGE_FRAME = 120;


std::string join_lines(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += text.empty() ? "<" : ", <";
        text += line;
        text += ">";
    }
    return text;
}

//  The number after "<prefix> ", if "line" is that.
std::optional<uint64_t> number_after(std::string_view line, std::string_view prefix){
    if (!line.starts_with(prefix)){
        return std::nullopt;
    }
    std::string_view rest = line.substr(prefix.size());
    uint64_t value = 0;
    std::from_chars_result result = std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (result.ec != std::errc() || rest.empty()){
        return std::nullopt;
    }
    return value;
}



//  A bridge stand-in that answers every command the controller side sends.
class FakeBridge : public AbstractServerSocket::Listener{
public:
    explicit FakeBridge(ThreadPool& thread_pool)
        : m_server(thread_pool)
    {
        m_server.add_listener(*this);
    }
    ~FakeBridge(){
        m_server.remove_listener(*this);
    }

    //  Listens on a random port in 40000..59999; nullopt if 8 tries fail.
    std::optional<uint16_t> start(){
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

    //  Waits up to 5 s for "line"; then returns every command seen.
    std::vector<std::string> commands_until(std::string_view line){
        std::unique_lock<Mutex> lg(m_lock);
        m_cv.wait_for(lg, std::chrono::seconds(5), [&]{
            return !m_commands.empty() && m_commands.back() == line;
        });
        return m_commands;
    }

private:
    virtual void on_listen_finished(const std::string& error_message) override{
        std::lock_guard<Mutex> lg(m_lock);
        m_listen_error = error_message;
        m_cv.notify_all();
    }
    virtual void on_receive_data(const void* data, size_t bytes) override{
        std::vector<std::string> lines = m_splitter.push(std::string_view(static_cast<const char*>(data), bytes));
        for (const std::string& line : lines){
            {
                std::lock_guard<Mutex> lg(m_lock);
                m_commands.emplace_back(line);
                m_cv.notify_all();
            }
            std::string reply = reply_for(line);
            m_server.send(reply.data(), reply.size());
            if (line == "BYE"){
                m_server.close_connection();
            }
        }
    }

    static std::string reply_for(std::string_view line){
        if (line == "HELLO 1"){
            return "OK 1 BPRE bridge-0.1 paced\n";
        }
        if (line.starts_with("GRANT ")){
            return std::format("OK {}\n", BRIDGE_FRAME);
        }
        if (line.starts_with("KEYS ")){
            return "OK\n";
        }
        if (line == "BYE"){
            return "OK BYE\n";
        }
        return std::format("ERR unknown {}\n", line.substr(0, line.find(' ')));
    }

private:
    FakeServerSocket m_server;
    LineSplitter m_splitter;
    Mutex m_lock;
    ConditionVariable m_cv;
    std::optional<std::string> m_listen_error;
    std::vector<std::string> m_commands;
};

//  The registry: which connection a lookup finds after publications start
//  and end. Connections are numbered by publish order, from 0.
enum class RegistryStep{
    PUBLISH_FIRST,
    PUBLISH_SECOND,
    END_FIRST,
    END_SECOND,
};
struct RegistryInput{
    std::vector<RegistryStep> steps;
};
struct RegistryExpectation{
    //  The connection found at the end, by number; nullopt = none.
    std::optional<size_t> found;
};
struct RegistryCase{
    std::string name;
    RegistryInput input;
    RegistryExpectation expectation;
};

class Test_Registry : public UnitTest{
public:
    Test_Registry(RegistryCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - link registry - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        ThreadPool& thread_pool = GlobalThreadPools::unlimited_normal();
        std::shared_ptr<Connection> connections[2] = {
            std::make_shared<Connection>(logger, thread_pool, "127.0.0.1", 47801),
            std::make_shared<Connection>(logger, thread_pool, "127.0.0.1", 47801),
        };
        LinkRegistry registry;
        LinkRegistry::Publication publications[2];
        for (RegistryStep step : m_case.input.steps){
            switch (step){
            case RegistryStep::PUBLISH_FIRST:
                publications[0] = registry.publish("127.0.0.1:47801", connections[0]);
                break;
            case RegistryStep::PUBLISH_SECOND:
                publications[1] = registry.publish("127.0.0.1:47801", connections[1]);
                break;
            case RegistryStep::END_FIRST:
                publications[0] = LinkRegistry::Publication();
                break;
            case RegistryStep::END_SECOND:
                publications[1] = LinkRegistry::Publication();
                break;
            }
        }
        std::optional<LinkRegistry::Link> link = registry.find("127.0.0.1:47801");
        std::optional<size_t> found;
        if (link){
            found = link->connection == connections[0] ? 0 : 1;
        }
        if (found != m_case.expectation.found){
            auto text = [](std::optional<size_t> x){
                return x ? std::format("connection {}", *x) : std::string("none");
            };
            return std::format("found {}, expected {}", text(found), text(m_case.expectation.found));
        }
        if (registry.find("127.0.0.1:47802")){
            return "found a connection under another address";
        }
        return true;
    }

private:
    RegistryCase m_case;
};


void add_registry_tests(UnitTestDatabase& database){
    const RegistryCase REGISTRY_CASES[] = {
        {
            .name = "nothing published",
            .input = {{}},
            .expectation = {std::nullopt},
        },
        {
            .name = "published",
            .input = {{RegistryStep::PUBLISH_FIRST}},
            .expectation = {0},
        },
        {
            .name = "publication ended",
            .input = {{RegistryStep::PUBLISH_FIRST, RegistryStep::END_FIRST}},
            .expectation = {std::nullopt},
        },
        {
            //  A reconnect publishes before the old publication ends.
            .name = "a newer publication outlives the older one",
            .input = {{RegistryStep::PUBLISH_FIRST, RegistryStep::PUBLISH_SECOND, RegistryStep::END_FIRST}},
            .expectation = {1},
        },
        {
            .name = "the newer publication ended",
            .input = {{RegistryStep::PUBLISH_FIRST, RegistryStep::PUBLISH_SECOND, RegistryStep::END_SECOND}},
            .expectation = {std::nullopt},
        },
    };
    for (const RegistryCase& test_case : REGISTRY_CASES){
        database.add<Test_Registry>(test_case);
    }
}



//  Runs "body" against a fake bridge with a connection to it published
//  under its address in "registry". Returns every command the bridge saw,
//  BYE last; nullopt if no port was free.
template <typename Body>
std::optional<std::vector<std::string>> with_fake_link(Logger& logger, Body&& body){
    ThreadPool& thread_pool = GlobalThreadPools::unlimited_normal();
    FakeBridge bridge(thread_pool);
    std::optional<uint16_t> port = bridge.start();
    if (!port){
        return std::nullopt;
    }
    {
        LinkRegistry registry;
        std::string address = std::format("127.0.0.1:{}", *port);
        std::shared_ptr<Connection> connection = std::make_shared<Connection>(logger, thread_pool, "127.0.0.1", *port);
        connection->open();
        LinkRegistry::Publication publication = registry.publish(address, connection);
        body(thread_pool, registry, address, connection);
    }
    return bridge.commands_until("BYE");
}



//  The pacer: GRANTs that follow app time, two frames ahead.
struct PacerInput{
    milliseconds run_time{};
};
struct PacerExpectation{
    uint64_t first_grant = 0;
    size_t min_grants = 0;
};
struct PacerCase{
    std::string name;
    PacerInput input;
    PacerExpectation expectation;
};

class Test_Pacer : public UnitTest{
public:
    Test_Pacer(PacerCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - pacer - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        WallClock start;
        WallClock stop;
        std::optional<std::vector<std::string>> commands = with_fake_link(
            logger,
            [&](ThreadPool& thread_pool, LinkRegistry&, const std::string&, std::shared_ptr<Connection> connection){
                start = current_time();
                {
                    Pacer pacer(logger, thread_pool, connection, FrameClock(start, BRIDGE_FRAME));
                    std::this_thread::sleep_for(m_case.input.run_time);
                }
                stop = current_time();
            }
        );
        if (!commands){
            return UnitTestResult(UnitTestResult::SKIPPED, "No free port for the fake bridge.");
        }
        std::vector<uint64_t> grants;
        for (const std::string& line : *commands){
            if (std::optional<uint64_t> grant = number_after(line, "GRANT ")){
                grants.emplace_back(*grant);
            }
        }
        if (grants.size() < m_case.expectation.min_grants){
            return std::format("{} GRANT(s), expected at least {}: {}", grants.size(), m_case.expectation.min_grants, join_lines(*commands));
        }
        if (grants.front() != m_case.expectation.first_grant){
            return std::format("first GRANT {}, expected {}", grants.front(), m_case.expectation.first_grant);
        }
        for (size_t i = 1; i < grants.size(); i++){
            if (grants[i] <= grants[i - 1]){
                return std::format("GRANTs not increasing: {}", join_lines(*commands));
            }
        }
        uint64_t latest = grant_frame(FrameClock(start, BRIDGE_FRAME), stop, KeySchedule::LEAD_FRAMES);
        if (grants.back() > latest){
            return std::format("last GRANT {} is past {}, the grant at the time the pacer stopped", grants.back(), latest);
        }
        return true;
    }

private:
    PacerCase m_case;
};


void add_pacer_tests(UnitTestDatabase& database){
    const PacerCase PACER_CASES[] = {
        {
            //  About 12 frames in 200 ms; one GRANT per new frame, so at
            //  least half of them even on a loaded machine.
            .name = "grants follow app time",
            .input = {milliseconds(200)},
            .expectation = {.first_grant = BRIDGE_FRAME + 2, .min_grants = 6},
        },
    };
    for (const PacerCase& test_case : PACER_CASES){
        database.add<Test_Pacer>(test_case);
    }
}



//  The key sender: what reaches the bridge for a sequence of pushes.
enum class SenderStep{
    PUSH,
    WAIT_FOR_ALL,
    CANCEL,
};
struct SenderAction{
    SenderStep step = SenderStep::PUSH;
    uint16_t mask = 0;
    milliseconds duration{};
};
struct SenderInput{
    bool published = true;
    std::vector<SenderAction> actions;
};
struct SenderExpectation{
    //  "KEYS +<frames after the first KEYS> <mask>", the anchoring
    //  "GRANT 0" and the closing run-on grant, in order; or "no connection".
    std::vector<std::string> commands;
    //  The run lasts no longer than this (a cancel cuts a long hold).
    milliseconds max_run_time{};
};
struct SenderCase{
    std::string name;
    SenderInput input;
    SenderExpectation expectation;
};

SenderAction push(uint16_t mask, int ms){
    return SenderAction{SenderStep::PUSH, mask, milliseconds(ms)};
}
SenderAction wait_for_all(){
    return SenderAction{SenderStep::WAIT_FOR_ALL, 0, milliseconds(0)};
}
SenderAction cancel(){
    return SenderAction{SenderStep::CANCEL, 0, milliseconds(0)};
}

void run_actions(KeySender& sender, const std::vector<SenderAction>& actions){
    for (const SenderAction& action : actions){
        switch (action.step){
        case SenderStep::PUSH:
            sender.push(nullptr, action.mask, action.duration);
            break;
        case SenderStep::WAIT_FOR_ALL:
            sender.wait_for_all(nullptr);
            break;
        case SenderStep::CANCEL:
            sender.cancel();
            break;
        }
    }
}

//  Keeps KEYS (frames relative to the first one), "GRANT 0" and the
//  run-on grant; drops the pacer's grants and the session lines. Also
//  returns the first KEYS frame.
std::vector<std::string> relative_keys(const std::vector<std::string>& commands, std::optional<uint64_t>& first_frame){
    std::vector<std::string> kept;
    const std::string run_on = std::format("GRANT {}", KeySender::RUN_ON_GRANT);
    for (const std::string& line : commands){
        if (line == "GRANT 0" || line == run_on){
            kept.emplace_back(line == run_on ? "GRANT run-on" : line);
            continue;
        }
        std::optional<uint64_t> frame = number_after(line, "KEYS ");
        if (!frame){
            continue;
        }
        if (!first_frame){
            first_frame = frame;
        }
        std::string_view mask = std::string_view(line).substr(line.rfind(' ') + 1);
        kept.emplace_back(std::format("KEYS +{} {}", *frame - *first_frame, mask));
    }
    return kept;
}

class Test_Sender : public UnitTest{
public:
    Test_Sender(SenderCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - key sender - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        WallClock start = current_time();
        std::vector<std::string> results;
        std::optional<std::vector<std::string>> commands = with_fake_link(
            logger,
            [&](ThreadPool& thread_pool, LinkRegistry& registry, const std::string& address, std::shared_ptr<Connection>){
                LinkRegistry other;
                KeySender sender(logger, thread_pool, m_case.input.published ? registry : other, address);
                try{
                    run_actions(sender, m_case.input.actions);
                }catch (ConnectionException&){
                    results.emplace_back("no connection");
                }
            }
        );
        milliseconds run_time = std::chrono::duration_cast<milliseconds>(current_time() - start);
        if (!commands){
            return UnitTestResult(UnitTestResult::SKIPPED, "No free port for the fake bridge.");
        }
        std::optional<uint64_t> first_frame;
        std::vector<std::string> kept = relative_keys(*commands, first_frame);
        results.insert(results.end(), kept.begin(), kept.end());
        if (results != m_case.expectation.commands){
            return std::format("the bridge saw {}, expected {}", join_lines(results), join_lines(m_case.expectation.commands));
        }
        //  Anchored at BRIDGE_FRAME, the first state starts two frames
        //  later (three on a slow machine).
        if (first_frame && (*first_frame < BRIDGE_FRAME + 2 || *first_frame > BRIDGE_FRAME + 4)){
            return std::format("first KEYS on frame {}, expected {}..{}", *first_frame, BRIDGE_FRAME + 2, BRIDGE_FRAME + 4);
        }
        if (run_time > m_case.expectation.max_run_time){
            return std::format("took {} ms, expected at most {} ms", run_time.count(), m_case.expectation.max_run_time.count());
        }
        return true;
    }

private:
    SenderCase m_case;
};


void add_sender_tests(UnitTestDatabase& database){
    const SenderCase SENDER_CASES[] = {
        {
            .name = "press and release",
            .input = {.actions = {push(GBA_KEY_A, 80), push(GBA_KEY_NONE, 80), wait_for_all()}},
            .expectation = {
                .commands = {"GRANT 0", "KEYS +0 1", "KEYS +5 0", "GRANT run-on"},
                .max_run_time = milliseconds(2000),
            },
        },
        {
            .name = "wait for all releases a held key",
            .input = {.actions = {push(GBA_KEY_START, 80), wait_for_all()}},
            .expectation = {
                .commands = {"GRANT 0", "KEYS +0 8", "KEYS +5 0", "GRANT run-on"},
                .max_run_time = milliseconds(2000),
            },
        },
        {
            //  50 ms is 2.99 frames; ends are placed from app time, so two
            //  holds of A take 6 frames, not 3 + 3 rounded separately.
            .name = "same keys are one command",
            .input = {.actions = {push(GBA_KEY_A, 50), push(GBA_KEY_A, 50), push(GBA_KEY_NONE, 50), wait_for_all()}},
            .expectation = {
                .commands = {"GRANT 0", "KEYS +0 1", "KEYS +6 0", "GRANT run-on"},
                .max_run_time = milliseconds(2000),
            },
        },
        {
            //  Up is held from frame +10 on, two states after A. Ends are
            //  placed from app time: 3 x 80 ms is 14.33 frames, so Up is
            //  released on +14, not on 3 x 5 = +15.
            .name = "a later state waits for its turn",
            .input = {.actions = {push(GBA_KEY_A, 80), push(GBA_KEY_NONE, 80), push(GBA_KEY_UP, 80), wait_for_all()}},
            .expectation = {
                .commands = {"GRANT 0", "KEYS +0 1", "KEYS +5 0", "KEYS +10 64", "KEYS +14 0", "GRANT run-on"},
                .max_run_time = milliseconds(2000),
            },
        },
    };
    for (const SenderCase& test_case : SENDER_CASES){
        database.add<Test_Sender>(test_case);
    }
}

void add_sender_cancel_tests(UnitTestDatabase& database){
    const SenderCase SENDER_CASES[] = {
        {
            //  A 3 s hold, cancelled at once: released one frame later.
            .name = "cancel releases at once",
            .input = {.actions = {push(GBA_KEY_B, 3000), cancel(), wait_for_all()}},
            .expectation = {
                .commands = {"GRANT 0", "KEYS +0 2", "KEYS +1 0", "GRANT run-on"},
                .max_run_time = milliseconds(1500),
            },
        },
        {
            .name = "no connection published",
            .input = {.published = false, .actions = {push(GBA_KEY_A, 80)}},
            .expectation = {
                .commands = {"no connection"},
                .max_run_time = milliseconds(2000),
            },
        },
    };
    for (const SenderCase& test_case : SENDER_CASES){
        database.add<Test_Sender>(test_case);
    }
}

}  // namespace


void add_key_sender_tests(UnitTestDatabase& database){
    add_registry_tests(database);
    add_pacer_tests(database);
    add_sender_tests(database);
    add_sender_cancel_tests(database);
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation

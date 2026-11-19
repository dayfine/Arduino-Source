/*  Headless Host
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <expected>
#include <format>
#include <functional>
#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QSocketNotifier>
#include <QTimer>
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Logging/Logger.h"
#include "CommonFramework/PersistentSettings.h"
#include "CommonFramework/ProgramSession.h"
#include "GameConsole/Framework/ConsoleProgramSession.h"
#include "GameConsole/Framework/ConsoleSystemSession.h"
#include "GameConsole/Framework/MultiConsoleProgramSession.h"
#include "GameConsole/Framework/MultiConsoleSystemSession.h"
#include "PanelCatalog.h"
#include "HeadlessHost.h"

namespace PokemonAutomation{
namespace{

constexpr std::chrono::seconds READINESS_TIMEOUT{10};

#if !defined(_WIN32)
constexpr std::chrono::seconds STOP_TIMEOUT{5};
struct SignalState{
    std::atomic<int> write_fd{-1};
    std::atomic<int> last_signal{0};
};

SignalState& signal_state(){
    static SignalState state;
    return state;
}

void posix_signal_handler(int sig){
    SignalState& s = signal_state();
    s.last_signal.store(sig, std::memory_order_relaxed);
    const int fd = s.write_fd.load(std::memory_order_relaxed);
    if (fd >= 0){
        const char byte = static_cast<char>(sig);
        const ssize_t ignored = ::write(fd, &byte, /*nbyte=*/1);
        (void)ignored;
    }
}

class SignalPipe final{
public:
    SignalPipe(Logger& logger, std::function<void(int)> on_signal){
        if (::pipe(m_fds) != 0){
            logger.log("Failed to create signal pipe; SIGINT/SIGTERM will terminate immediately.", COLOR_ORANGE);
            return;
        }
        for (int fd : m_fds){
            const int flags = ::fcntl(fd, F_GETFL, /*arg=*/0);
            if (flags >= 0){
                ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
        }
        signal_state().write_fd.store(m_fds[1], std::memory_order_release);
        struct sigaction sa{};
        sa.sa_handler = posix_signal_handler;
        sigemptyset(&sa.sa_mask);
        m_has_int = (::sigaction(SIGINT, &sa, &m_old_int) == 0);
        m_has_term = (::sigaction(SIGTERM, &sa, &m_old_term) == 0);

        m_notifier = std::make_unique<QSocketNotifier>(m_fds[0], QSocketNotifier::Read);
        QObject::connect(m_notifier.get(), &QSocketNotifier::activated, m_notifier.get(), [this, on_signal = std::move(on_signal)]{
            std::array<char, 64> buf;
            while (::read(m_fds[0], buf.data(), buf.size()) > 0){
            }
            on_signal(signal_state().last_signal.load(std::memory_order_relaxed));
        });
    }

    ~SignalPipe(){
        if (m_has_int){
            ::sigaction(SIGINT, &m_old_int, nullptr);
        }
        if (m_has_term){
            ::sigaction(SIGTERM, &m_old_term, nullptr);
        }
        signal_state().write_fd.store(-1, std::memory_order_release);
        m_notifier.reset();
        for (int& fd : m_fds){
            if (fd >= 0){
                ::close(fd);
                fd = -1;
            }
        }
    }

private:
    int m_fds[2] = {-1, -1};
    struct sigaction m_old_int{};
    struct sigaction m_old_term{};
    bool m_has_int = false;
    bool m_has_term = false;
    std::unique_ptr<QSocketNotifier> m_notifier;
};
#endif

bool system_ready(GameConsole::ConsoleSystemSession& system){
    for (size_t i = 0; i < system.controllers(); i++){
        if (!system.controller(i).ready()){
            return false;
        }
    }
    return true;
}

bool controller_ready(ProgramSession& program){
    if (auto* single = dynamic_cast<GameConsole::ConsoleProgramSession*>(&program)){
        return system_ready(single->system());
    }
    if (auto* multi = dynamic_cast<GameConsole::MultiConsoleProgramSession*>(&program)){
        for (size_t i = 0; i < multi->system().active_consoles(); i++){
            if (!system_ready(multi->system()[i])){
                return false;
            }
        }
    }
    return true;
}

class HeadlessProgramObserver final : public ProgramSession::Listener{
public:
    HeadlessProgramObserver(ProgramSession& program, QObject& context, std::function<void()> on_stopped)
        : m_program(program), m_context(context), m_on_stopped(std::move(on_stopped))
    {}

    bool saw_error() const{ return m_saw_error.load(std::memory_order_acquire); }

    virtual void state_change(ProgramState state) override{
        if (state == ProgramState::STOPPED){
            QMetaObject::invokeMethod(&m_context, [this]{
                if (m_program.current_state() == ProgramState::STOPPED){
                    m_on_stopped();
                }
            }, Qt::QueuedConnection);
        }
    }
    virtual void stats_update(const StatsTracker*, const StatsTracker*) override{}
    virtual void error(const std::string&) override{ m_saw_error.store(true, std::memory_order_release); }
    virtual void download_error(const std::string&) override{}
    virtual void download_added(std::shared_ptr<ResourceDownload>) override{}
    virtual void all_downloads_done() override{}

private:
    ProgramSession& m_program;
    QObject& m_context;
    std::function<void()> m_on_stopped;
    std::atomic<bool> m_saw_error{false};
};

std::expected<JsonValue, std::string> merge_options(const JsonValue& base, const JsonValue& over){
    const JsonObject* over_obj = over.to_object();
    if (over_obj == nullptr){
        return std::unexpected("Options file must contain a JSON object.");
    }
    if (over_obj->get_value("ConsoleSetup") != nullptr){
        return std::unexpected("--options cannot override ConsoleSetup.");
    }
    JsonObject merged;
    if (const JsonObject* base_obj = base.to_object()){
        merged = base_obj->clone();
    }
    for (const auto& [key, value] : *over_obj){
        merged[key] = value.clone();
    }
    return JsonValue(std::move(merged));
}

HeadlessExit load_headless_panel(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options,
    HeadlessHost& host,
    Logger& logger
){
    std::optional<PanelMatch> match = find_panel(lists, options.program_id);
    if (!match){
        logger.log(std::format("Unknown program identifier: \"{}\".", options.program_id), COLOR_RED);
        return HeadlessExit::BAD_REQUEST;
    }
    std::shared_ptr<const PanelDescriptor> descriptor(std::move(match->entry.descriptor));

    JsonValue json;
    if (options.base_panel_json){
        json = options.base_panel_json->clone();
    }else if (const JsonValue* saved = PERSISTENT_SETTINGS().panels.get_value(descriptor->identifier())){
        json = saved->clone();
    }
    if (options.options_override){
        auto merged = merge_options(json, *options.options_override);
        if (!merged){
            logger.log(merged.error(), COLOR_RED);
            return HeadlessExit::BAD_REQUEST;
        }
        json = std::move(*merged);
    }

    try{
        load_panel_session(descriptor, json, host);
    }catch (const UserSetupError& e){
        logger.log(std::format("Device setup error: {}", e.to_str()), COLOR_RED);
        return HeadlessExit::DEVICE;
    }catch (const Exception& e){
        logger.log(std::format("Failed to load program: {}", e.to_str()), COLOR_RED);
        return HeadlessExit::BAD_REQUEST;
    }
    return HeadlessExit::OK;
}

bool wait_for_controller_ready(ProgramSession& program){
    if (controller_ready(program)){
        return true;
    }
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]{
        if (controller_ready(program)){
            loop.quit();
        }
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]{ loop.quit(); });
    poll.start(std::chrono::milliseconds(50));
    timeout.start(READINESS_TIMEOUT);
    loop.exec();
    return controller_ready(program);
}

HeadlessExit run_program_event_loop(ProgramSession& program, bool handle_signals, Logger& logger){
    QEventLoop loop;
    std::optional<HeadlessExit> signal_exit;
    QTimer stop_timer;
    stop_timer.setSingleShot(true);

    HeadlessProgramObserver observer(program, loop, [&]{ loop.quit(); });
    program.add_listener(observer);

#if !defined(_WIN32)
    std::unique_ptr<SignalPipe> pipe;
    if (handle_signals){
        pipe = std::make_unique<SignalPipe>(logger, [&](int sig){
            const HeadlessExit code = sig == SIGTERM ? HeadlessExit::SIGNAL_TERM : HeadlessExit::SIGNAL_INT;
            if (signal_exit){
                logger.log("Second signal received; exiting immediately.", COLOR_RED);
                std::_Exit(static_cast<int>(code));
            }
            signal_exit = code;
            logger.log("Signal received; stopping program...", COLOR_ORANGE);
            program.stop_program();
            QObject::connect(&stop_timer, &QTimer::timeout, &loop, [&logger, code]{
                logger.log("Stop timeout expired; exiting immediately.", COLOR_RED);
                std::_Exit(static_cast<int>(code));
            });
            stop_timer.start(STOP_TIMEOUT);
        });
    }
#else
    (void)handle_signals;
#endif

    const std::string err = program.start_program();
    if (!err.empty()){
        logger.log(std::format("Failed to start program: {}", err), COLOR_RED);
        program.remove_listener(observer);
        return HeadlessExit::DEVICE;
    }
    if (program.current_state() != ProgramState::STOPPED){
        loop.exec();
    }
    program.remove_listener(observer);
    if (signal_exit){
        return *signal_exit;
    }
    return observer.saw_error() ? HeadlessExit::PROGRAM_ERROR : HeadlessExit::OK;
}

} // namespace


void HeadlessHost::clear_panel(){
    m_current_panel.reset();
    m_current_panel_descriptor.reset();
}

void HeadlessHost::load_panel(
    std::shared_ptr<const PanelDescriptor> descriptor,
    std::unique_ptr<PanelSession> panel
){
    clear_panel();
    m_current_panel_descriptor = std::move(descriptor);
    m_current_panel = std::move(panel);
}

HeadlessExit run_headless_program(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options
){
    Logger& logger = global_logger_tagged();
    HeadlessHost host;
    const HeadlessExit load_status = load_headless_panel(lists, options, host, logger);
    if (load_status != HeadlessExit::OK){
        return load_status;
    }
    ProgramSession* program = dynamic_cast<ProgramSession*>(host.current_panel());
    if (program == nullptr){
        logger.log(std::format("Panel \"{}\" is not a runnable program.", options.program_id), COLOR_RED);
        host.clear_panel();
        return HeadlessExit::BAD_REQUEST;
    }
    if (!wait_for_controller_ready(*program)){
        logger.log("Controller did not become ready before timeout.", COLOR_RED);
        host.clear_panel();
        return HeadlessExit::DEVICE;
    }
    const HeadlessExit ret = run_program_event_loop(*program, options.handle_signals, logger);
    host.clear_panel();
    return ret;
}


} // namespace PokemonAutomation

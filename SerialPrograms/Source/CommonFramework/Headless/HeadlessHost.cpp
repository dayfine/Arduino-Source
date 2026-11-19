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
constexpr std::chrono::seconds STOP_TIMEOUT{5};

#if !defined(_WIN32)
struct SignalHandlerGlobals{
    std::atomic<int> write_fd{-1};
    std::atomic<int> last_signal{0};
};

SignalHandlerGlobals& signal_handler_globals(){
    static SignalHandlerGlobals globals;
    return globals;
}

void posix_signal_handler(int sig){
    SignalHandlerGlobals& globals = signal_handler_globals();
    globals.last_signal.store(sig, std::memory_order_relaxed);
    const int fd = globals.write_fd.load(std::memory_order_relaxed);
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
        signal_handler_globals().write_fd.store(m_fds[1], std::memory_order_release);

        struct sigaction sa{};
        sa.sa_handler = posix_signal_handler;
        sigemptyset(&sa.sa_mask);
        if (::sigaction(SIGINT, &sa, &m_old_int) == 0){
            m_installed_int = true;
        }
        if (::sigaction(SIGTERM, &sa, &m_old_term) == 0){
            m_installed_term = true;
        }

        m_notifier = std::make_unique<QSocketNotifier>(m_fds[0], QSocketNotifier::Read);
        QObject::connect(m_notifier.get(), &QSocketNotifier::activated, m_notifier.get(), [this, on_signal = std::move(on_signal)]{
            std::array<char, 64> buf;
            while (::read(m_fds[0], buf.data(), buf.size()) > 0){
            }
            on_signal(signal_handler_globals().last_signal.load(std::memory_order_relaxed));
        });
    }

    ~SignalPipe(){
        if (m_installed_int){
            ::sigaction(SIGINT, &m_old_int, nullptr);
        }
        if (m_installed_term){
            ::sigaction(SIGTERM, &m_old_term, nullptr);
        }
        signal_handler_globals().write_fd.store(-1, std::memory_order_release);
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
    bool m_installed_int = false;
    bool m_installed_term = false;
    std::unique_ptr<QSocketNotifier> m_notifier;
};
#endif

bool system_controllers_ready(GameConsole::ConsoleSystemSession& system){
    for (size_t i = 0; i < system.controllers(); i++){
        if (!system.controller(i).ready()){
            return false;
        }
    }
    return true;
}

bool controller_ready(ProgramSession& program){
    if (auto* console = dynamic_cast<GameConsole::ConsoleProgramSession*>(&program)){
        return system_controllers_ready(console->system());
    }
    if (auto* multi = dynamic_cast<GameConsole::MultiConsoleProgramSession*>(&program)){
        GameConsole::MultiConsoleSystemSession& sys = multi->system();
        for (size_t i = 0; i < sys.active_consoles(); i++){
            if (!system_controllers_ready(sys[i])){
                return false;
            }
        }
        return true;
    }
    return true;
}

class HeadlessProgramObserver final : public ProgramSession::Listener{
public:
    HeadlessProgramObserver(ProgramSession& program, QObject& context, std::function<void()> on_stopped)
        : m_program(program)
        , m_context(context)
        , m_on_stopped(std::move(on_stopped))
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

    virtual void error(const std::string&) override{
        m_saw_error.store(true, std::memory_order_release);
    }

    virtual void download_error(const std::string&) override{}

private:
    ProgramSession& m_program;
    QObject& m_context;
    std::function<void()> m_on_stopped;
    std::atomic<bool> m_saw_error{false};
};

std::expected<JsonValue, std::string> merge_headless_options(
    const JsonValue& base_panel_json,
    const JsonValue& override_json
){
    const JsonObject* override_obj = override_json.to_object();
    if (override_obj == nullptr){
        return std::unexpected("Options file must contain a JSON object.");
    }
    if (override_obj->get_value("ConsoleSetup") != nullptr){
        return std::unexpected("--options cannot override ConsoleSetup.");
    }
    JsonObject merged;
    if (const JsonObject* base_obj = base_panel_json.to_object()){
        merged = base_obj->clone();
    }
    for (const auto& [key, value] : *override_obj){
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

    JsonValue panel_json;
    if (options.base_panel_json){
        panel_json = options.base_panel_json->clone();
    }else if (const JsonValue* saved = PERSISTENT_SETTINGS().panels.get_value(descriptor->identifier())){
        panel_json = saved->clone();
    }
    if (options.options_override){
        std::expected<JsonValue, std::string> merged = merge_headless_options(panel_json, *options.options_override);
        if (!merged){
            logger.log(merged.error(), COLOR_RED);
            return HeadlessExit::BAD_REQUEST;
        }
        panel_json = std::move(*merged);
    }

    try{
        load_panel_session(descriptor, panel_json, host);
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
    QEventLoop ready_loop;
    QTimer poll_timer;
    QTimer timeout_timer;
    timeout_timer.setSingleShot(true);
    QObject::connect(&poll_timer, &QTimer::timeout, &ready_loop, [&]{
        if (controller_ready(program)){
            ready_loop.quit();
        }
    });
    QObject::connect(&timeout_timer, &QTimer::timeout, &ready_loop, [&]{
        ready_loop.quit();
    });
    poll_timer.start(std::chrono::milliseconds(50));
    timeout_timer.start(READINESS_TIMEOUT);
    ready_loop.exec();
    return controller_ready(program);
}

HeadlessExit run_program_event_loop(ProgramSession& program, bool handle_signals, Logger& logger){
    QEventLoop run_loop;
    std::optional<HeadlessExit> signal_exit;
    QTimer stop_timer;
    stop_timer.setSingleShot(true);

    HeadlessProgramObserver observer(program, run_loop, [&]{
        run_loop.quit();
    });
    program.add_listener(observer);

#if !defined(_WIN32)
    std::unique_ptr<SignalPipe> signal_pipe;
    if (handle_signals){
        signal_pipe = std::make_unique<SignalPipe>(logger, [&](int sig){
            const HeadlessExit code = sig == SIGTERM ? HeadlessExit::SIGNAL_TERM : HeadlessExit::SIGNAL_INT;
            if (signal_exit){
                logger.log("Second signal received; exiting immediately.", COLOR_RED);
                std::_Exit(static_cast<int>(code));
            }
            signal_exit = code;
            logger.log("Signal received; stopping program...", COLOR_ORANGE);
            program.stop_program();
            QObject::connect(&stop_timer, &QTimer::timeout, &run_loop, [&logger, code]{
                logger.log("Stop timeout expired; exiting immediately.", COLOR_RED);
                std::_Exit(static_cast<int>(code));
            });
            stop_timer.start(STOP_TIMEOUT);
        });
    }
#else
    (void)handle_signals;
#endif

    const std::string start_error = program.start_program();
    if (!start_error.empty()){
        logger.log(std::format("Failed to start program: {}", start_error), COLOR_RED);
        program.remove_listener(observer);
        return HeadlessExit::DEVICE;
    }
    if (program.current_state() != ProgramState::STOPPED){
        run_loop.exec();
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

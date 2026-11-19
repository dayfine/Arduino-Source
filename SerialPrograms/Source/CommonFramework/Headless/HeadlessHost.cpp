/*  Headless Host
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <atomic>
#include <csignal>
#include <format>
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
#include "PanelCatalog.h"
#include "HeadlessHost.h"

namespace PokemonAutomation{
namespace{

#if !defined(_WIN32)
struct SignalPipeState{
    int fds[2] = {-1, -1};
    std::atomic<int> last_signal{0};
};

SignalPipeState& signal_pipe_state(){
    static SignalPipeState state;
    return state;
}

void posix_signal_handler(int sig){
    SignalPipeState& state = signal_pipe_state();
    state.last_signal.store(sig, std::memory_order_relaxed);
    if (state.fds[1] >= 0){
        const char byte = static_cast<char>(sig);
        const ssize_t ignored = ::write(state.fds[1], &byte, /*nbyte=*/1);
        (void)ignored;
    }
}

void install_signal_pipe(QSocketNotifier*& notifier_out, std::function<void(int)> on_signal){
    SignalPipeState& state = signal_pipe_state();
    if (::pipe(state.fds) != 0){
        return;
    }
    for (int fd : state.fds){
        const int flags = ::fcntl(fd, F_GETFL, /*arg=*/0);
        if (flags >= 0){
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    std::signal(SIGINT, posix_signal_handler);
    std::signal(SIGTERM, posix_signal_handler);
    notifier_out = new QSocketNotifier(state.fds[0], QSocketNotifier::Read);
    QObject::connect(notifier_out, &QSocketNotifier::activated, notifier_out, [on_signal = std::move(on_signal)]{
        SignalPipeState& s = signal_pipe_state();
        std::array<char, 64> buf;
        while (::read(s.fds[0], buf.data(), buf.size()) > 0){
        }
        on_signal(s.last_signal.load(std::memory_order_relaxed));
    });
}

void cleanup_signal_pipe(QSocketNotifier*& notifier){
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    delete notifier;
    notifier = nullptr;
    SignalPipeState& state = signal_pipe_state();
    for (int& fd : state.fds){
        if (fd >= 0){
            ::close(fd);
            fd = -1;
        }
    }
}
#endif

bool controller_ready(ProgramSession& program){
    auto* console = dynamic_cast<GameConsole::ConsoleProgramSession*>(&program);
    if (console == nullptr){
        return true;
    }
    GameConsole::ConsoleSystemSession& system = console->system();
    return system.controllers() > 0 && system.controller(0).ready();
}

class HeadlessProgramObserver final : public ProgramSession::Listener{
public:
    HeadlessProgramObserver(QObject& context, std::function<void()> on_stopped)
        : m_context(context)
        , m_on_stopped(std::move(on_stopped))
    {}

    bool saw_error() const{ return m_saw_error.load(std::memory_order_acquire); }

    virtual void state_change(ProgramState state) override{
        if (state == ProgramState::STOPPED){
            QMetaObject::invokeMethod(&m_context, [this]{
                m_on_stopped();
            }, Qt::QueuedConnection);
        }
    }

    virtual void stats_update(const StatsTracker*, const StatsTracker*) override{}

    virtual void error(const std::string& message) override{
        if (message == "Unable to save stats."){
            return;
        }
        m_saw_error.store(true, std::memory_order_release);
    }

    virtual void download_error(const std::string&) override{}

private:
    QObject& m_context;
    std::function<void()> m_on_stopped;
    std::atomic<bool> m_saw_error{false};
};

int load_headless_panel(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options,
    HeadlessHost& host,
    Logger& logger
){
    std::optional<PanelMatch> match = find_panel(lists, options.program_id);
    if (!match){
        logger.log(std::format("Unknown program identifier: \"{}\".", options.program_id), COLOR_RED);
        return 4;
    }
    std::shared_ptr<const PanelDescriptor> descriptor(std::move(match->entry.descriptor));
    const JsonValue* saved_panel = PERSISTENT_SETTINGS().panels.get_value(descriptor->identifier());
    JsonValue panel_json = saved_panel != nullptr ? saved_panel->clone() : JsonValue();
    if (options.options_path){
        try{
            const JsonValue override_json = load_json_file(*options.options_path);
            std::string merge_error;
            std::optional<JsonValue> merged = merge_headless_options(panel_json, override_json, merge_error);
            if (!merged){
                logger.log(merge_error, COLOR_RED);
                return 4;
            }
            panel_json = std::move(*merged);
        }catch (const Exception& e){
            logger.log(std::format("Failed to read --options \"{}\": {}", *options.options_path, e.to_str()), COLOR_RED);
            return 4;
        }
    }
    try{
        if (!load_panel_session(descriptor, panel_json, host)){
            logger.log("HeadlessHost declined panel load.", COLOR_RED);
            return 4;
        }
    }catch (const UserSetupError& e){
        logger.log(std::format("Device setup error: {}", e.to_str()), COLOR_RED);
        return 5;
    }catch (const Exception& e){
        logger.log(std::format("Failed to load program: {}", e.to_str()), COLOR_RED);
        return 4;
    }
    return 0;
}

bool wait_for_controller_ready(ProgramSession& program, std::chrono::milliseconds timeout){
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
    poll_timer.start(50);
    timeout_timer.start(static_cast<int>(timeout.count()));
    ready_loop.exec();
    return controller_ready(program);
}

int run_program_event_loop(
    ProgramSession& program,
    std::chrono::milliseconds stop_timeout,
    Logger& logger
){
    QEventLoop run_loop;
    std::optional<int> signal_exit_code;
    QTimer stop_timer;
    stop_timer.setSingleShot(true);

    HeadlessProgramObserver observer(run_loop, [&]{
        run_loop.quit();
    });
    program.add_listener(observer);

#if !defined(_WIN32)
    QSocketNotifier* signal_notifier = nullptr;
    install_signal_pipe(signal_notifier, [&](int sig){
        const int code = sig == SIGTERM ? 143 : 130;
        if (signal_exit_code){
            logger.log("Second signal received; exiting immediately.", COLOR_RED);
            run_loop.exit(code);
            return;
        }
        signal_exit_code = code;
        logger.log("Signal received; stopping program...", COLOR_ORANGE);
        program.stop_program();
        QObject::connect(&stop_timer, &QTimer::timeout, &run_loop, [&]{
            logger.log("Stop timeout expired; exiting.", COLOR_RED);
            run_loop.exit(code);
        });
        stop_timer.start(static_cast<int>(stop_timeout.count()));
    });
#endif

    const std::string start_error = program.start_program();
    int exit_code = 0;
    if (!start_error.empty()){
        logger.log(std::format("Failed to start program: {}", start_error), COLOR_RED);
        exit_code = 5;
    }else if (program.current_state() != ProgramState::STOPPED){
        run_loop.exec();
    }

#if !defined(_WIN32)
    cleanup_signal_pipe(signal_notifier);
#endif
    program.remove_listener(observer);
    if (exit_code != 0){
        return exit_code;
    }
    if (signal_exit_code){
        return *signal_exit_code;
    }
    return observer.saw_error() ? 1 : 0;
}

} // namespace


void HeadlessHost::clear_panel(){
    m_current_panel.reset();
    m_current_panel_descriptor.reset();
}

bool HeadlessHost::report_new_panel_intent(const PanelDescriptor& descriptor){
    if (m_current_panel == nullptr){
        return true;
    }
    return &m_current_panel->descriptor() != &descriptor;
}

void HeadlessHost::load_panel(
    std::shared_ptr<const PanelDescriptor> descriptor,
    std::unique_ptr<PanelSession> panel
){
    clear_panel();
    m_current_panel_descriptor = std::move(descriptor);
    m_current_panel = std::move(panel);
}


std::optional<JsonValue> merge_headless_options(
    const JsonValue& base_panel_json,
    const JsonValue& override_json,
    std::string& error
){
    const JsonObject* override_obj = override_json.to_object();
    if (override_obj == nullptr){
        error = "Options file must contain a JSON object.";
        return std::nullopt;
    }
    if (override_obj->get_value("ConsoleSetup") != nullptr){
        error = "--options cannot override ConsoleSetup.";
        return std::nullopt;
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


int run_headless_program(
    const std::vector<std::unique_ptr<PanelListDescriptor>>& lists,
    const HeadlessRunOptions& options
){
    Logger& logger = global_logger_tagged();
    HeadlessHost host;
    const int load_status = load_headless_panel(lists, options, host, logger);
    if (load_status != 0){
        return load_status;
    }

    ProgramSession* program = dynamic_cast<ProgramSession*>(host.current_panel());
    if (program == nullptr){
        logger.log(std::format("Panel \"{}\" is not a runnable program.", options.program_id), COLOR_RED);
        host.clear_panel();
        return 4;
    }

    if (!wait_for_controller_ready(*program, options.readiness_timeout)){
        logger.log("Controller did not become ready before timeout.", COLOR_RED);
        host.clear_panel();
        return 5;
    }

    const int ret = run_program_event_loop(*program, options.stop_timeout, logger);
    host.clear_panel();
    return ret;
}


} // namespace PokemonAutomation

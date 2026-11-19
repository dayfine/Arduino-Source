/*  Headless Host
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
int g_signal_pipe[2] = {-1, -1};
std::atomic<int> g_last_signal{0};

void posix_signal_handler(int sig){
    g_last_signal.store(sig, std::memory_order_relaxed);
    if (g_signal_pipe[1] >= 0){
        const char byte = static_cast<char>(sig);
        const ssize_t ignored = ::write(g_signal_pipe[1], &byte, 1);
        (void)ignored;
    }
}

void install_signal_pipe(QSocketNotifier*& notifier_out, std::function<void(int)> on_signal){
    if (::pipe(g_signal_pipe) != 0){
        return;
    }
    for (int fd : g_signal_pipe){
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0){
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    std::signal(SIGINT, posix_signal_handler);
    std::signal(SIGTERM, posix_signal_handler);
    notifier_out = new QSocketNotifier(g_signal_pipe[0], QSocketNotifier::Read);
    QObject::connect(notifier_out, &QSocketNotifier::activated, notifier_out, [on_signal = std::move(on_signal)]{
        char buf[64];
        while (::read(g_signal_pipe[0], buf, sizeof(buf)) > 0){}
        on_signal(g_last_signal.load(std::memory_order_relaxed));
    });
}

void cleanup_signal_pipe(QSocketNotifier*& notifier){
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    delete notifier;
    notifier = nullptr;
    for (int& fd : g_signal_pipe){
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

} // namespace


void HeadlessHost::clear_panel(){
    m_current_panel.reset();
    m_current_panel_descriptor.reset();
}

bool HeadlessHost::report_new_panel_intent(const PanelDescriptor& descriptor){
    if (m_current_panel == nullptr){
        return true;
    }
    if (&m_current_panel->descriptor() == &descriptor){
        return false;
    }
    return true;
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

    HeadlessHost host;
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

    ProgramSession* program = dynamic_cast<ProgramSession*>(host.current_panel());
    if (program == nullptr){
        logger.log(std::format("Panel \"{}\" is not a runnable program.", options.program_id), COLOR_RED);
        host.clear_panel();
        return 4;
    }

    if (!controller_ready(*program)){
        QEventLoop ready_loop;
        bool timed_out = false;
        QTimer poll_timer;
        QTimer timeout_timer;
        timeout_timer.setSingleShot(true);
        QObject::connect(&poll_timer, &QTimer::timeout, &ready_loop, [&]{
            if (controller_ready(*program)){
                ready_loop.quit();
            }
        });
        QObject::connect(&timeout_timer, &QTimer::timeout, &ready_loop, [&]{
            timed_out = true;
            ready_loop.quit();
        });
        poll_timer.start(50);
        timeout_timer.start(static_cast<int>(options.readiness_timeout.count()));
        ready_loop.exec();
        if (timed_out && !controller_ready(*program)){
            logger.log("Controller did not become ready before timeout.", COLOR_RED);
            host.clear_panel();
            return 5;
        }
    }

    QEventLoop run_loop;
    std::optional<int> signal_exit_code;
    QTimer stop_timer;
    stop_timer.setSingleShot(true);

    HeadlessProgramObserver observer(run_loop, [&]{
        run_loop.quit();
    });
    program->add_listener(observer);

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
        program->stop_program();
        QObject::connect(&stop_timer, &QTimer::timeout, &run_loop, [&]{
            logger.log("Stop timeout expired; exiting.", COLOR_RED);
            run_loop.exit(code);
        });
        stop_timer.start(static_cast<int>(options.stop_timeout.count()));
    });
#endif

    const std::string start_error = program->start_program();
    if (!start_error.empty()){
        logger.log(std::format("Failed to start program: {}", start_error), COLOR_RED);
#if !defined(_WIN32)
        cleanup_signal_pipe(signal_notifier);
#endif
        program->remove_listener(observer);
        host.clear_panel();
        return 5;
    }

    if (program->current_state() != ProgramState::STOPPED){
        run_loop.exec();
    }

#if !defined(_WIN32)
    cleanup_signal_pipe(signal_notifier);
#endif
    program->remove_listener(observer);
    host.clear_panel();

    if (signal_exit_code){
        return *signal_exit_code;
    }
    return observer.saw_error() ? 1 : 0;
}


} // namespace PokemonAutomation

/*  Application Startup Arguments
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "StartupArguments.h"

namespace PokemonAutomation{
// Profile names are single ASCII path components; never silently rewrite an identity.
bool valid_startup_profile(std::string_view profile){
    return !profile.empty() && profile.front() != '-' && std::ranges::all_of(profile, [](char c){
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

namespace{

// Qt owns option values; recognize only its documented application option spellings.
// References: doc.qt.io/qt-6/{qapplication,qguiapplication}.html constructors.
enum class QtArgument{UNKNOWN, ACCEPTED, INVALID};
QtArgument consume_qt_argument(std::span<const std::string_view> arguments, size_t& index, StartupPlan& plan){
    const std::string_view arg = arguments[index];
    if (arg == "-reverse" || arg == "-widgetcount"){
        plan.qt_arguments.emplace_back(arg);
        return QtArgument::ACCEPTED;
    }
    constexpr std::string_view VALUE_OPTIONS[] = {
        "-platform", "-platformpluginpath", "-platformtheme", "-plugin",
        "-qwindowgeometry", "-qwindowicon", "-qwindowtitle", "-session",
        "-display", "-geometry", "-style", "-stylesheet",
    };
    if (std::ranges::find(VALUE_OPTIONS, arg) != std::end(VALUE_OPTIONS)){
        if (index + 1 == arguments.size() || arguments[index + 1].empty() ||
            arguments[index + 1].starts_with('-')){
            return QtArgument::INVALID;
        }
        plan.qt_arguments.emplace_back(arg);
        plan.qt_arguments.emplace_back(arguments[++index]);
        return QtArgument::ACCEPTED;
    }
    for (std::string_view prefix : {"-style=", "-stylesheet=", "-qmljsdebugger="}){
        if (arg.starts_with(prefix)){
            if (arg.size() == prefix.size()){
                return QtArgument::INVALID;
            }
            plan.qt_arguments.emplace_back(arg);
            return QtArgument::ACCEPTED;
        }
    }
    return QtArgument::UNKNOWN;
}

} // namespace

StartupPlan parse_startup_arguments(std::span<const std::string_view> arguments){
    StartupPlan plan{.mode = StartupMode::GUI};
    for (size_t i = 0; i < arguments.size(); i++){
        const std::string_view arg = arguments[i];
        if (arg == "--profile"){
            if (plan.profile || i + 1 == arguments.size() || !valid_startup_profile(arguments[i + 1])){
                return {.error = "--profile requires one unique name containing only ASCII letters, digits, '_' or '-' (not starting with '-')."};
            }
            plan.profile = arguments[++i];
        }else if (arg == "--headless"){
            if (plan.mode == StartupMode::TESTS){
                return {.error = "--headless and --command-line-test-mode cannot be combined."};
            }
            plan.mode = StartupMode::HEADLESS;
        }else if (arg == "--run"){
            if (plan.run_program || i + 1 == arguments.size() || arguments[i + 1].empty() ||
                arguments[i + 1].starts_with('-')){
                return {.error = "--run requires one unique program identifier."};
            }
            plan.run_program = arguments[++i];
        }else if (arg == "--options"){
            if (plan.options_path || i + 1 == arguments.size() || arguments[i + 1].empty() ||
                arguments[i + 1].starts_with('-')){
                return {.error = "--options requires one unique file path."};
            }
            plan.options_path = arguments[++i];
        }else if (arg == "--command-line-test-mode"){
            if (plan.mode == StartupMode::HEADLESS){
                return {.error = "--headless and --command-line-test-mode cannot be combined."};
            }
            plan.mode = StartupMode::TESTS;
        }else if (arg == "--augment-color-distortions"){
            plan.augment_color_distortions = true;
        }else{
            const QtArgument result = consume_qt_argument(arguments, i, plan);
            if (result != QtArgument::ACCEPTED){
                return {.error = "Unknown or incomplete startup argument: " + std::string(arg)};
            }
        }
    }
    if (plan.run_program && plan.mode != StartupMode::HEADLESS){
        return {.error = "--run requires --headless."};
    }
    if (plan.options_path && !plan.run_program){
        return {.error = "--options requires --run."};
    }
    if (plan.mode == StartupMode::HEADLESS && !plan.run_program){
        return {.error = "--headless requires --run <identifier>."};
    }
    if (plan.augment_color_distortions && plan.mode != StartupMode::TESTS){
        return {.error = "--augment-color-distortions requires --command-line-test-mode."};
    }
    if (plan.mode == StartupMode::TESTS && plan.profile){
        return {.error = "Test launches cannot select a console profile."};
    }
    return plan;
}

int dispatch_startup(const StartupPlan& plan, const StartupHandlers& handlers){
    switch (plan.mode){
    case StartupMode::GUI:
        return handlers.gui(plan);
    case StartupMode::HEADLESS:
        return handlers.headless ? handlers.headless(plan) : 2;
    case StartupMode::TESTS:
        return handlers.tests(plan);
    case StartupMode::REJECTED:
        return 2;
    }
    return 2;
}

} // namespace PokemonAutomation

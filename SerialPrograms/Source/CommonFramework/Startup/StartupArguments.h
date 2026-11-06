/*  Lab Startup Arguments
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Parse before Qt, settings, logging or device discovery. Example:
 *  open -n SerialPrograms.app --args --profile lab-B
 */
#ifndef PokemonAutomation_StartupArguments_H
#define PokemonAutomation_StartupArguments_H

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PokemonAutomation{

// Rejected launches must not enter either application entry point.
enum class StartupMode{REJECTED, GUI, TESTS};

// Validated startup intent; error describes a rejected launch.
struct StartupPlan{
    StartupMode mode = StartupMode::REJECTED;
    std::optional<std::string> profile = std::nullopt;
    bool augment_color_distortions = false;
    std::string error = {};
    std::vector<std::string> qt_arguments = {};
};

// Check a single ASCII profile path component without rewriting its identity.
bool valid_startup_profile(std::string_view profile);

// Parse argv without argv[0]; preserve documented Qt options for Qt to interpret.
// Unknown or malformed application/Qt arguments fail before settings or devices.
StartupPlan parse_startup_arguments(std::span<const std::string_view> arguments);

// Application entry points: the GUI callback owns all settings and hardware startup.
struct StartupHandlers{
    std::function<int(const StartupPlan&)> gui;
    std::function<int(const StartupPlan&)> tests;
};

// Dispatch a validated plan. Rejection returns 2 without invoking either callback.
int dispatch_startup(const StartupPlan& plan, const StartupHandlers& handlers);

} // namespace PokemonAutomation
#endif

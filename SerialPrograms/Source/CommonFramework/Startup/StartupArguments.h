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

namespace PokemonAutomation{

// Whether this platform supports isolated macOS settings profiles.
enum class StartupProfiles{UNSUPPORTED, REQUIRED};

// A build-bound profile survives Finder and permission-dialog relaunches.
struct StartupPolicy{
    StartupProfiles profiles = StartupProfiles::UNSUPPORTED;
    std::optional<std::string> bound_profile = std::nullopt;
};

// Rejected launches must not enter either application entry point.
enum class StartupMode{REJECTED, GUI, TESTS};

// Validated startup intent; error describes a rejected launch.
struct StartupPlan{
    StartupMode mode = StartupMode::REJECTED;
    std::optional<std::string> profile = std::nullopt;
    bool augment_color_distortions = false;
    std::string error = {};
};

// Parse arguments excluding argv[0]. Unknown, ambiguous and unsafe inputs fail closed.
StartupPlan parse_startup_arguments(std::span<const std::string_view> arguments, const StartupPolicy& policy);

// Application entry points: the GUI callback owns all settings and hardware startup.
struct StartupHandlers{
    std::function<int(const StartupPlan&)> gui;
    std::function<int(const StartupPlan&)> tests;
};

// Dispatch a validated plan. Rejection returns 2 without invoking either callback.
int dispatch_startup(const StartupPlan& plan, const StartupHandlers& handlers);

} // namespace PokemonAutomation
#endif

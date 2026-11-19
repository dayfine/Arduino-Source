/*  Lab Startup Policy
 *
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_LabStartupPolicy_H
#define PokemonAutomation_LabStartupPolicy_H
#include "StartupArguments.h"

namespace PokemonAutomation{

// Profiles are supported by the macOS settings backend, and required for lab GUIs.
enum class LabProfileMode{UNSUPPORTED, REQUIRED};

// Lab-only restrictions, independent of the general application argument grammar.
struct LabStartupPolicy{
    LabProfileMode profiles = LabProfileMode::UNSUPPORTED;
    std::optional<std::string> bound_profile = std::nullopt;
};

// Apply lab identity restrictions after parsing. Rejected plans stay rejected;
// tests bypass console identity, while GUI launches must satisfy the lab policy.
StartupPlan apply_lab_startup_policy(StartupPlan plan, const LabStartupPolicy& policy);

} // namespace PokemonAutomation
#endif

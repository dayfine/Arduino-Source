/*  Lab Startup Arguments
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <algorithm>
#include "StartupArguments.h"

namespace PokemonAutomation{
namespace{

// Profile names are single ASCII path components; never silently rewrite an identity.
bool valid_profile(std::string_view profile){
    return !profile.empty() && profile.front() != '-' && std::ranges::all_of(profile, [](char c){
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

// Resolve the selected identity only after all arguments are understood.
StartupPlan resolve_profile(StartupPlan plan, const StartupPolicy& policy){
    if (plan.mode == StartupMode::TESTS){
        if (plan.profile){
            return {.error = "Test launches cannot select a console profile."};
        }
        return plan;
    }
    if (policy.profiles == StartupProfiles::UNSUPPORTED){
        if (plan.profile || policy.bound_profile){
            return {.error = "Settings profiles are supported only on macOS."};
        }
        return plan;
    }
    if (policy.bound_profile){
        if (!valid_profile(*policy.bound_profile) ||
            (plan.profile && plan.profile != policy.bound_profile)){
            return {.error = "The requested profile does not match this lab build."};
        }
        plan.profile = policy.bound_profile;
    }
    if (!plan.profile){
        return {.error = "Lab launch requires --profile NAME or a build configured with PA_LAB_PROFILE."};
    }
    return plan;
}

} // namespace

StartupPlan parse_startup_arguments(std::span<const std::string_view> arguments, const StartupPolicy& policy){
    StartupPlan plan{.mode = StartupMode::GUI};
    for (size_t i = 0; i < arguments.size(); i++){
        const std::string_view arg = arguments[i];
        if (arg == "--profile"){
            if (plan.profile || i + 1 == arguments.size() || !valid_profile(arguments[i + 1])){
                return {.error = "--profile requires one unique name containing only ASCII letters, digits, '_' or '-' (not starting with '-')."};
            }
            plan.profile = arguments[++i];
        }else if (arg == "--command-line-test-mode"){
            plan.mode = StartupMode::TESTS;
        }else if (arg == "--augment-color-distortions"){
            plan.augment_color_distortions = true;
        }else{
            return {.error = "Unknown startup argument: " + std::string(arg)};
        }
    }
    if (plan.augment_color_distortions && plan.mode != StartupMode::TESTS){
        return {.error = "--augment-color-distortions requires --command-line-test-mode."};
    }
    return resolve_profile(std::move(plan), policy);
}

int dispatch_startup(const StartupPlan& plan, const StartupHandlers& handlers){
    switch (plan.mode){
    case StartupMode::GUI:
        return handlers.gui(plan);
    case StartupMode::TESTS:
        return handlers.tests(plan);
    case StartupMode::REJECTED:
        return 2;
    }
    return 2;
}

} // namespace PokemonAutomation

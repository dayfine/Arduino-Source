/*  Lab Startup Policy
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include "LabStartupPolicy.h"

namespace PokemonAutomation{

StartupPlan apply_lab_startup_policy(StartupPlan plan, const LabStartupPolicy& policy){
    if (plan.mode != StartupMode::GUI){
        return plan;
    }
    if (policy.profiles == LabProfileMode::UNSUPPORTED){
        if (plan.profile || policy.bound_profile){
            return {.error = "Settings profiles are supported only on macOS."};
        }
        return plan;
    }
    if (policy.bound_profile){
        if (!valid_startup_profile(*policy.bound_profile) ||
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

} // namespace PokemonAutomation

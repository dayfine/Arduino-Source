/*  BDSP Starter Navigation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_StarterNavigation_H
#define PokemonAutomation_PokemonBDSP_StarterNavigation_H

#include <stdint.h>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonBDSP_RngExecution.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


enum class BdspStarter{
    Turtwig,
    Chimchar,
    Piplup,
};


uint8_t starter_cursor_steps(BdspStarter starter);
std::string_view starter_slug(BdspStarter starter);


struct BdspSkipRates{
    //  Measured over 14 attempts spanning 40k to 734k advances, +/-0.01%
    static constexpr double ADVANCES_PER_MINUTE = 19653;
    //  Measured over the same attempts, +/-29
    static constexpr double NAVIGATION_ADVANCES = 6902;
};

namespace detail{
//  How far short of the target to stop quickly skipping advances
inline constexpr double BDSP_SKIP_BUFFER_BASE = 750;
inline constexpr double BDSP_SKIP_BUFFER_PER_ADVANCE = 0.0002;
inline constexpr double BDSP_SKIP_RATE_SIGMA = 0.0001;
inline constexpr double BDSP_SKIP_SCATTER_SIGMA = 67;
} // namespace detail

inline uint64_t bdsp_skip_buffer(double advances_skipped){
    return (uint64_t)(detail::BDSP_SKIP_BUFFER_BASE + detail::BDSP_SKIP_BUFFER_PER_ADVANCE * advances_skipped);
}

inline double bdsp_skip_landing_sigma(double advances_skipped){
    const double from_rate = detail::BDSP_SKIP_RATE_SIGMA * advances_skipped;
    return std::sqrt(from_rate * from_rate + detail::BDSP_SKIP_SCATTER_SIGMA * detail::BDSP_SKIP_SCATTER_SIGMA);
}

inline uint64_t bdsp_lake_advances_needed(double advances_skipped){
    return bdsp_skip_buffer(advances_skipped)
        + (uint64_t)(2 * bdsp_skip_landing_sigma(advances_skipped));
}


void navigate_bedroom_to_skip_spot(Logger& logger, ProControllerContext& context);

void navigate_skip_spot_to_lakefront(Logger& logger, ProControllerContext& context);


void navigate_to_lake_blinks(Logger& logger, ProControllerContext& context);

void navigate_to_pre_briefcase(SingleSwitchProgramEnvironment& env, ProControllerContext& context);


bool issue_starter_sequence(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    BdspStarter starter,
    Language language,
    const PressSchedule& schedule,
    std::string& failure_reason
);


//  Mash A until the battle with the wild Starly is over.
bool clear_starter_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

double seconds_to_move_to_starter(BdspStarter starter);

struct StarterSceneDurations{
    double briefcase_to_starly_ready = 0;
    double select_to_confirm = 0;
    double to_pre_briefcase = 0;

    static StarterSceneDurations measured(Language language);
};

//  Advances the starter scene spends, in this language, beyond what the
//  starter timeline counts. Added to the aim.
int64_t starter_scene_extra_advances(Language language);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

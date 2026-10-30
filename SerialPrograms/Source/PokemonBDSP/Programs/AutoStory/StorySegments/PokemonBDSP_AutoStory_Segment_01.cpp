/*  BDSP AutoStory - Segment 01
 *  From the Jubilife Pokemon Center to Oreburgh City.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_01.h"


using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


std::string AutoStory_Segment_01::name()       const { return "01: Jubilife to Floaroma"; }
std::string AutoStory_Segment_01::start_text() const { return "Start: Standing in front of the Jubilife City Pokemon Center PC."; }
std::string AutoStory_Segment_01::end_text()   const { return "End: In the Floaroma Town Pokemon Center, with the Coal Badge."; }

void AutoStory_Segment_01::run_segment(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    stats.m_segment++;
    env.update_stats();
    env.console.log("Start Segment " + name(), COLOR_ORANGE);
    AutoStory_Checkpoint_0101().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0102().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0103().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0104().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0105().run_checkpoint(env, context, options, stats);
    env.console.log("End Segment " + name(), COLOR_GREEN);
}

}
}
}

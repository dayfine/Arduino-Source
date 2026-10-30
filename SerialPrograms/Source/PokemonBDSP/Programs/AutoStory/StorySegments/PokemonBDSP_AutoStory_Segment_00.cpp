/*  BDSP AutoStory - Segment 00
 *  From player's bedroom to pokedex acquisition.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_00.h"


namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {
        // ---------------------------------------------------------------------------
        //  Segment / Checkpoint classes
        // ---------------------------------------------------------------------------

        std::string AutoStory_Segment_00::name()       const { return "00: Intro to Poketech"; }
        std::string AutoStory_Segment_00::start_text() const { return "Start: Game launched, Standing in the player's room after tv stops playing"; }
        std::string AutoStory_Segment_00::end_text()   const { return "End: Starter Pokemon chosen, received, and Pokedex acquired, standing in front of the PC."; }

        void AutoStory_Segment_00::run_segment(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            stats.m_segment++;
            env.update_stats();
            context.wait_for_all_requests();
            env.console.log("Start Segment " + name(), COLOR_ORANGE);

            AutoStory_Checkpoint_0000().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0001().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0002().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0003().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0004().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0005().run_checkpoint(env, context, options, stats);

            context.wait_for_all_requests();
            env.console.log("End Segment " + name(), COLOR_GREEN);
        }

        
        }
    }
}

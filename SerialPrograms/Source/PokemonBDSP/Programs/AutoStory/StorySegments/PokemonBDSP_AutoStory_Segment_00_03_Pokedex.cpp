/*  BDSP AutoStory - Segment 00.03: Pokedex
 *  Route 201 to Sandgem Town and Rowan's lab, where Rowan gives the
 *  Pokedex.
 *
 *  Start: outside Lake Verity, after the starter battle.
 *  End:   in front of Rowan, after receiving the Pokedex.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "../Utils/PokemonBDSP_AutoStory_GameSettings.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_WildEncounters.h"

#include "PokemonBDSP_AutoStory_Segment_00.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

        // ---------------------------------------------------------------------------
        //  Segment 00-03 begins
        //  Navigates from Exit of lake Verity to receipt of the Pokedex
        // ---------------------------------------------------------------------------

        static void tutorial_part_2_navigation(SingleSwitchProgramEnvironment& env, 
            VideoStream& stream, ProControllerContext& context) {
            pbf_wait(context, 2000ms);
            context.wait_for_all_requests();

            pbf_wait(context, 563ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1700ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -1.000000 }, 510ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 50ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -1.000000 }, 310ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 5500ms, 0ms);
            pbf_wait(context, 3963ms);
            context.wait_for_all_requests();

            wait_for_dialogue(stream, context, "Phase 10 (Professor dialog to mom)");
            pbf_mash_button(context, BUTTON_B, 40000ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 820ms, 0ms);
            pbf_wait(context, 4000ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 800ms, 0ms);
            pbf_wait(context, 555ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 50ms, 0ms);
            pbf_wait(context, 23ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 5020ms, 0ms);
            pbf_wait(context, 169ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 745ms, 0ms);
            DateTime t{ 2026, 1, 1, 0, 0, 50 };
            set_time_fast(env, context, t);
            walk_through_grass(stream, context, 1, 0, 20, "right through grass to path", BattleType::STARTER);
            walk_through_grass(stream, context, 0, 1, 18, "up through grass 2", BattleType::STARTER);
            walk_through_grass(stream, context, 1, 0, 10, "right through grass 2", BattleType::STARTER);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 730ms, 0ms);
            pbf_wait(context, 941ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 1950ms, 0ms);
            wait_for_dialogue(stream, context, "Phase 11 (dawn conversation)");
            pbf_mash_button(context, BUTTON_B, 50000ms);
            wait_for_dialogue(stream, context, "Phase 12 (Nickname reject)");
            pbf_mash_button(context, BUTTON_B, 40000ms);
            wait_for_dialogue(stream, context, "Phase 13 (Pokedex accept)");
            pbf_mash_button(context, BUTTON_A, 40000ms);


            // pokedex received
        }


        // ---------------------------------------------------------------------------
        // End of Segment 00 - 03
        // Standing in front of the professor after receiving the pokedex.
        // ---------------------------------------------------------------------------

        static void checkpoint_0003(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-002: Pokedex acquisition
                    tutorial_part_2_navigation(env, env.console, context);
                },
                true
            );
        }

        // ========================
        // Checkpoint 0003 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0003::name()       const { return "00.03 - Outside Verity after starter"; }
        std::string AutoStory_Checkpoint_0003::start_text() const { return AutoStory_Checkpoint_0002().end_text(); }
        std::string AutoStory_Checkpoint_0003::end_text()   const { return "Standing in front of The professor after recieving the Pokedex"; }

        void AutoStory_Checkpoint_0003::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0003(env, context, options, stats);
        }

        }
    }
}

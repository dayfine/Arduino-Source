#pragma once

#include "PokemonBDSP_OverworldMatchers.h"

#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "../StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
#include "CommonTools/Async/InferenceRoutines.h"

#include "NintendoSwitch/Programs/DateManip/NintendoSwitch_DateManip.h"

#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Utility Watchers
            // ---------------------------------------------------------------------------

            class BattleDialogWatcher : public DetectorToFinder<BattleDialogDetector> {
            public:
                BattleDialogWatcher(Color color = COLOR_RED);
            };

            // ---------------------------------------------------------------------------
            // Time / setup helpers
            // ---------------------------------------------------------------------------

            // Shared visual alignment and dialogue cleanup used by story routes.
            void clear_post_battle_dialogue(VideoStream& stream, ProControllerContext& context,
                Button button = BUTTON_B, Milliseconds quiet_period = Milliseconds(3000),
                const char* instruction_card = nullptr);
            void align_to_overworld_landmarks(VideoStream& stream, ProControllerContext& context,
                OverworldMatchResult (*matcher)(const ImageRGB32&), const std::string& label, bool allow_story_start = false, bool horizontal_first = false,
                bool search_down_first = false,
                OverworldMatchResult (*entry_matcher)(const ImageRGB32&) = nullptr,
                bool* entry_detected = nullptr,
                double arrival_tolerance_x = 0, double arrival_tolerance_y = 0,
                double moderate_entry_confidence = 0,
                bool* rock_acquisition_timed_out = nullptr);

            void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context);

            void set_time_fast(
                SingleSwitchProgramEnvironment& env,
                ProControllerContext& context,
                const DateTime& target,
                bool preserve_date = false    // Keep current year/month/day; set only target time.
            );

            // ---------------------------------------------------------------------------
            // Battles
            // ---------------------------------------------------------------------------

            void fight_starly(
                VideoStream& stream,
                ProControllerContext& context
            );

            // Select the requested 1-based move slot using visual cursor confirmation.
            void fight_trainer_using_move_slot(VideoStream& stream, ProControllerContext& context,
                size_t move_slot, const std::string& label);

            // Any trainer battle, whatever the starter: uses move slot 1 until the
            // battle ends. Slot 1 is a damaging move (Tackle, Scratch or Pound) for
            // every starter.
            void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label);

            // A story battle that may go badly: the best move each turn, Potions
            // under 35 %, the next Pokemon when one faints, and "Keep old moves"
            // on level-up prompts. `doubles` confirms the default target each
            // turn (tag and twin battles). Returns once the battle has ended,
            // won or lost; after a loss we stand in the last Pokemon Center.
            void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, bool doubles = false);

            // ---------------------------------------------------------------------------
            // Catching / healing
            // ---------------------------------------------------------------------------

            void catch_1_pokemon(
                VideoStream& stream,
                ProControllerContext& context
            );

            // Finds one wild Pokemon in the grass by stepping left and right, and
            // fights it with the best move on the menu. Does not heal.
            void battle_1_pokemon(VideoStream& stream, ProControllerContext& context);

            // Burst wiggle variant of catch_1_pokemon for testing.
            // Commented out in the .cpp - uncomment both to enable.
            // void catch_1_pokemon_v2(
            //     SingleSwitchProgramEnvironment& env,
            //     VideoStream& stream,
            //     ProControllerContext& context
            // );

            // ---------------------------------------------------------------------------
            // Grass movement helpers
            // ---------------------------------------------------------------------------

            void walk_through_grass(
                VideoStream& stream,
                ProControllerContext& context,
                int8_t x,
                int8_t y,
                size_t steps,
                const std::string& direction_name,
                BattleType battle_type = BattleType::STANDARD,
                Milliseconds movement_duration = Milliseconds(200),
                OverworldMatchResult (*destination)(const ImageRGB32&) = nullptr
            );

            bool detect_black_screen_after_movement(
                VideoStream& stream,
                ProControllerContext& context,
                Milliseconds timeout
            );

            bool try_right_down_exit_search(
                VideoStream& stream,
                ProControllerContext& context,
                size_t iterations
            );

            void leave_pokemon_center_before_barry_1(
                VideoStream& stream,
                ProControllerContext& context
            );
            // ---------------------------------------------------------------------------
            // Helper function to navigate the poketech challenge clown dialogue.
            // Example usage
            // Move up and talk to clown.
            // clown_dialog_helper(
            //     stream,
            //     context,
            //     DPAD_UP
            // );

            // Move left and talk to clown.
            // clown_dialog_helper(
            //    stream,
            //    context,
            //    DPAD_LEFT
            // );

            // Move right and talk to clown.
            // clown_dialog_helper(
            //    stream,
            //    context,
            //    DPAD_RIGHT
            // );
            // ----------------------------------------------------------------------------
            void clown_dialog_helper(
                VideoStream& stream,
                ProControllerContext& context,
                DpadPosition direction
            );
        }
    }
}
/*  BDSP AutoStory - Story Battles
 *  Fights story and trainer battles with the best move, Potions when HP runs
 *  low, and the dialogue after them.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryBattle_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryBattle_H

#include <string>

#include "CommonFramework/Tools/VideoStream.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
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

            // Shared visual alignment and dialogue cleanup used by story routes.
            void clear_post_battle_dialogue(VideoStream& stream, ProControllerContext& context,
                Button button = BUTTON_B, Milliseconds quiet_period = Milliseconds(3000),
                const char* instruction_card = nullptr);

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

            // Waits for a dialogue-free overworld after a battle.
            void confirm_battle_overworld(VideoStream& stream, ProControllerContext& context,
                const std::string& label);

            // Fights with one move slot (0: the best move each turn) until the battle ends.
            void fight_using_move_slot(VideoStream& stream, ProControllerContext& context,
                BattleType battle_type, size_t move_slot, const std::string& label);

        }
    }
}
#endif

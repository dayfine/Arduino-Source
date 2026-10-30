/*  BDSP AutoStory - Move Selection
 *  Picks the best move on the battle's move menu, and selects a move slot
 *  with the cursor confirmed on screen.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_MoveSelect_H
#define PokemonAutomation_PokemonBDSP_AutoStory_MoveSelect_H

#include <string>

#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // Selects 1-based move `target` (0: the best move on the menu) with
            // the cursor confirmed on screen, then presses A.
            void select_battle_move_slot(VideoStream& stream, ProControllerContext& context,
                BattleType battle_type, size_t target, const std::string& label,
                bool confirm_default_target = false, size_t current_move = 1);

        }
    }
}
#endif

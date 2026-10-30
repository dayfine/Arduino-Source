/*  BDSP AutoStory - Wild Encounters
 *  Finds wild Pokemon in tall grass to train on or catch, and walks through
 *  grass running from them.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WildEncounters_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WildEncounters_H

#include <stdint.h>
#include <string>

#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

#include "PokemonBDSP_OverworldMatchers.h"

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

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

        }
    }
}
#endif

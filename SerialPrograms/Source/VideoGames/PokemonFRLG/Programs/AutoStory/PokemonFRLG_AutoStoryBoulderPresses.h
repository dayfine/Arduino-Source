/*  AutoStory Boulder Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A boulder push's taps on the console (push_boulder_tiles()), each read
 *  from shots of the screen before and after it (read_boulder_press()).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBoulderPresses_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBoulderPresses_H

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include <optional>
#include <string>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_AutoStoryBoulders.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Strength on, at the boulder `direction` of the player on `here`: A,
//  YES to "Would you like to use STRENGTH?", its message closed. If A
//  shows nothing (the overworld stays quiet), no boulder is there: returns
//  that failure at once, not after the YES/NO's timeout. 19.04d's
//  push_into_hole pressed A at the hole the boulder had already fallen
//  into and waited 60 s for a YES/NO (console B, 2026-11-16).
std::optional<std::string> use_strength(ConsoleHandle& console, ProControllerContext& context, const MapPosition& here, Direction direction);

//  A boulder push's taps on the console: step taps, each read from shots of
//  the screen before it and once still after it.
class ConsoleBoulderPresses : public BoulderPresses{
public:
    ConsoleBoulderPresses(ConsoleHandle& console, ProControllerContext& context, PlayerGender player);

    //  A step tap (step_once()), read once the screen is still.
    BoulderPressRead press(Direction direction) override;
    //  The settle rule: looks until the overworld is quiet for
    //  MovementMechanics::AFTER_BATTLE_QUIET; a battle that shows is run
    //  from, a box closed, and the overworld waited for again.
    bool clear_interrupt() override;

private:
    VideoSnapshot still_shot();

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    PlayerGender m_player;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

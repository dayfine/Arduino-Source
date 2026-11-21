/*  BDSP AutoStory PR #1717 Synthetic Video Frames
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthesizes lightweight 960x540 ImageRGB32 frames that accurately trigger
 *  the real BDSP detectors (BlackScreen, WhiteScreen, ShortDialog,
 *  ShortDialogPrompt + SelectionArrow, BattleDialog, BattleMenu + cursor row,
 *  MenuDetector, OverworldDetector, TownBanner, StarterBriefcase, cave_visible).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Frames_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Sim1717_Frames_H

#include <string>
#include "CommonFramework/ImageTypes/ImageRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

enum class SimScreenKind{
    OVERWORLD,
    OVERWORLD_CAVE,          //  Brown corridor + black map void (for Ravaged Path cave_visible)
    OVERWORLD_WITH_BANNER,   //  Overworld + top-left Town Banner
    BLACK_SCREEN,
    WHITE_SCREEN,
    SHORT_DIALOG,
    SHORT_DIALOG_PROMPT_YES, //  Short dialog + SelectionArrow at Yes (0.707, 0.650)
    SHORT_DIALOG_PROMPT_NO,  //  Short dialog + SelectionArrow at No  (0.707, 0.713)
    BRIEFCASE,               //  StarterBriefcase.png blitted in (0.5, 0.1, 0.5, 0.7)
    BATTLE_DIALOG,
    BATTLE_MENU_ROW0,        //  Battle menu with FIGHT selected (row 0)
    BATTLE_MENU_ROW1,        //  Battle menu with POKEMON selected (row 1)
    BATTLE_MENU_ROW2,        //  Battle menu with BAG selected (row 2)
    BATTLE_MENU_ROW3,        //  Battle menu with RUN selected (row 3)
    MAIN_MENU,               //  X-menu passing MenuDetector::detect_menu_chrome
};

//  Returns a cached synthetic frame for `kind`.
std::shared_ptr<const ImageRGB32> sim1717_frame(SimScreenKind kind);

//  Verifies every synthetic frame against the real BDSP detectors.
//  Returns "" on success, or an error description on mismatch.
std::string sim1717_verify_synthetic_frames();

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  BDSP AutoStory - Intro probe
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Telling the player is in control from how X and B change the screen,
 *  when X opens nothing the menu detector knows.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_IntroProbe_H
#define PokemonAutomation_PokemonBDSP_AutoStory_IntroProbe_H

#include <chrono>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  When X opens nothing the menu detector knows, the player is still in
//  control if the screen held still for INTRO_STILL_TIME before X (no
//  dialogue, no fade, no intro sparkles), X changed the menu's area by more
//  than INTRO_X_CHANGE, and B put it back within INTRO_B_RESTORE: the menu
//  opened and closed. Changes are pixel RMSD.
inline constexpr std::chrono::milliseconds INTRO_STILL_TIME = std::chrono::seconds(2);
//  Maximum pixel RMSD threshold for considering the screen still before probing menu.
inline constexpr double INTRO_STILL_CHANGE = 4;
//  Minimum pixel RMSD change indicating menu opened after pressing X.
inline constexpr double INTRO_X_CHANGE = 25;
//  Maximum pixel RMSD difference between pre-X screen and post-B screen.
inline constexpr double INTRO_B_RESTORE = 10;
//  Probe metrics for detecting menu presence via screen changes from X and B.
struct XProbe{
    double still_change;    //  over INTRO_STILL_TIME before X
    double x_change;        //  from before X to after it
    double b_change;        //  from before X to after the B that follows
};
//  Evaluates whether probe metrics indicate player is in control without menu detection.
bool intro_in_control_without_menu(const XProbe& probe);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

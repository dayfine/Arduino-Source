/*  BDSP AutoStory - Location
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the game says the player is: the location banner shown on entering
 *  a map, and the map named on the save screen's card.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Location_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Location_H

#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Lower-case letters and digits only: "Route 202" and "ROUTE 2O2 " compare
//  by "route202".
std::string normalize_location_name(const std::string& text);

//  The location banner (the white box at the top left that names a map for a
//  few seconds after the player enters it), read by OCR and normalized;
//  empty when no banner shows.
std::string read_location_banner(const ImageViewRGB32& screen);
//  Whether the location banner's white box shows, without reading it.
bool location_banner_showing(const ImageViewRGB32& screen);

//  The map named at the top left of the save screen's card ("Sandgem Town",
//  "Route 202"), read by OCR and normalized; empty if nothing reads.
std::string read_save_screen_location(const ImageViewRGB32& screen);

//  Whether the location banner's OCR `read` names the map `expected`. Both
//  are compared as lower-case letters and digits. OCR drops or swaps letters
//  ("andgemltown" for Sandgem Town on r109), so up to a quarter of the name's
//  letters may differ; the digits must match exactly, so Route 201 is never
//  read as Route 202.
bool location_banner_matches(const std::string& read, const std::string& expected);


}
}
}
#endif

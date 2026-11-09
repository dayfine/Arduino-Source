/*  Save Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads where the game would be saved: the place name in blue at the top
 *  of the card the START menu's SAVE shows ("PALLET TOWN" over PLAYER,
 *  BADGES and TIME). It knows the places from a new game to Route 4,
 *  Cerulean City, Route 24 and Route 25 (Bill's Sea Cottage is on it), in
 *  English only so far: in another language nothing is read. Route 3,
 *  Route 4, Cerulean City, Route 24 and Route 25 have no test capture yet.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_SaveCardReader_H
#define PokemonAutomation_PokemonFRLG_SaveCardReader_H

#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The card's title, as a fraction of the game screen. Measured on a
//  capture of the card in Pallet Town (console B, 2026-11-01): the name
//  spans x 31 to 96, y 11 to 19 of the 240x160 screen, centered.
ImageFloatBox save_card_title_box();

//  The place on the save card: "pallet-town", "viridian-city",
//  "viridian-forest", "pewter-city", "route-1" to "route-4", "cerulean-city",
//  "route-10", "route-24", "route-25", "vermilion-city",
//  "lavender-town"; empty if it can't be read, as
//  in a language the reader has no names for.
std::string read_save_card_location(Logger& logger, Language language, const ImageViewRGB32& screen);


//  Registers save card reader unit tests.
void add_tests_SaveCardReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

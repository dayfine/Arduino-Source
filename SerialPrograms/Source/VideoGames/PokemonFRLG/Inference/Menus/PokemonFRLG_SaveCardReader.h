/*  Save Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads where the game would be saved: the place name in blue at the top
 *  of the card the START menu's SAVE shows ("PALLET TOWN" over PLAYER,
 *  BADGES and TIME). It knows every Kanto place, in English only so far:
 *  in another language nothing is read. Only Pallet Town, Viridian Forest
 *  and Pewter City have a test capture yet.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_SaveCardReader_H
#define PokemonAutomation_PokemonFRLG_SaveCardReader_H

#include <optional>
#include <string>
#include <string_view>
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

//  The place on the save card, as a slug ("pallet-town", "route-22",
//  "victory-road"); empty if it can't be read, as in a language the reader
//  has no names for.
std::string read_save_card_location(Logger& logger, Language language, const ImageViewRGB32& screen);

//  The slug read_save_card_location() returns for a save on `map`, a
//  pokefirered map name: a place's map or a building in it
//  ("Route4_PokemonCenter_1F" is on Route 4; "Route24" isn't on Route 2).
//  Nullopt for a map outside Kanto.
std::optional<std::string> save_card_place(std::string_view map);


//  Registers save card reader unit tests.
void add_tests_SaveCardReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

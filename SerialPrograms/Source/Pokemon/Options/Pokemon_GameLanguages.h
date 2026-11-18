/*  Game Languages
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The languages each game can be played in, as dropdown databases for a
 *  program's language option. A program offers its game's set, so a user
 *  can't pick a language the game doesn't have. Every language a game ships
 *  in is listed, tested on console or not. Code that reads text takes this
 *  option and never assumes a language. With no reader for the chosen
 *  language, a name lookup throws InternalProgramError ("No data loaded for
 *  this language.") and a story step falls back to positions, digits and the
 *  screen.
 *
 */

#ifndef PokemonAutomation_Pokemon_GameLanguages_H
#define PokemonAutomation_Pokemon_GameLanguages_H

#include "Common/Cpp/Options/EnumDropdownDatabase.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The nine languages of the Switch games of generations 8 and 9: Sword and
//  Shield, Brilliant Diamond and Shining Pearl, Legends: Arceus, Scarlet and
//  Violet, Legends: Z-A. Japanese, English, French, Italian, German,
//  Spanish, Korean, Chinese (Simplified and Traditional). The games have one
//  Spanish (Spain), hence one "es".
const EnumDropdownDatabase<Language>& Gen8ToGen9Languages_Database();

//  The six languages of FireRed and LeafGreen, the Game Boy Advance
//  releases the Switch versions reissue: Japanese, English, French, Italian,
//  German, Spanish. On Switch each language is a separate version with no
//  in-game language option (Nintendo of America sells English, French and
//  Spanish, the latter two in their European form), so the user picks the
//  version they own.
const EnumDropdownDatabase<Language>& FireRedLeafGreenLanguages_Database();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

/*  Game Languages
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The languages each game can be played in, as dropdown databases for a
 *  program's language option. A program offers its game's set, so a user
 *  can't pick a language the game doesn't have.
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
//  Spanish, Korean, Chinese (Simplified and Traditional).
const EnumDropdownDatabase<Language>& Gen8ToGen9Languages_Database();

//  The six languages of FireRed and LeafGreen: Japanese, English, French,
//  Italian, German, Spanish.
const EnumDropdownDatabase<Language>& FireRedLeafGreenLanguages_Database();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

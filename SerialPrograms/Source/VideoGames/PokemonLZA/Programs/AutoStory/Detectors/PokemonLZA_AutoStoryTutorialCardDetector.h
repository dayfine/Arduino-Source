/*  PokemonLZA AutoStory - Tutorial card detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The tutorial cards the story opens (Map & Missions, Main Missions,
 *  Pokedex, ...). Only A turns their pages (B turns back), so a scene mash
 *  checks for one before it presses its text button.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTutorialCardDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryTutorialCardDetector_H

#include <string_view>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A tutorial card on screen: a bright panel in the middle over dark, even
//  edges.
bool tutorial_card_shows(const ImageViewRGB32& frame);

//  The story backend's reading set to 1 while a tutorial card shows. Only
//  A turns its pages; a catch in the tutorial ends on "Battling wild
//  Pokemon" cards (03.07).
constexpr std::string_view TUTORIAL_CARD_READING = "scene.tutorial_card";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

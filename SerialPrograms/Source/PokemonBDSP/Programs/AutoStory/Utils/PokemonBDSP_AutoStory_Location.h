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
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Lower-case letters and digits only: "Route 202" and "ROUTE 2O2 " compare
//  by "route202".
std::string normalize_location_name(const std::string& text);

//  The game's language, for the reads that still use text (item names, the
//  location banner, a sign's or talk's first box), set once from the
//  program's options (StoryRunOptions) before the story runs; English until
//  then. story_reads_text() says whether those reads have a model and
//  dictionary for it (English only, so far); when not, callers use
//  language-free checks: the banner's box showing, a sign's box opening, the
//  first row of a Bag pocket.
Language story_language();
void set_story_language(Language language);
bool story_reads_text();
bool story_reads_text(Language language);

//  What the reads below return for a banner or save card that showed, when
//  there is no text reader for the game's language (story_reads_text()):
//  location_banner_matches() takes it as a match.
inline const std::string UNREAD_LOCATION = "(unread)";

//  The location banner (the white box at the top left that names a map for a
//  few seconds after the player enters it), read by OCR and normalized;
//  empty when no banner shows.
std::string read_location_banner(const ImageViewRGB32& screen);
//  Whether the location banner's white box shows, without reading it.
bool location_banner_showing(const ImageViewRGB32& screen);
//  The banner box's mean and spread, for the log.
std::string location_banner_stats(const ImageViewRGB32& screen);

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

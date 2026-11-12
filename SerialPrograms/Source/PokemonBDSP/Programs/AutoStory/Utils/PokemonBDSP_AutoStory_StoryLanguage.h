/*  BDSP AutoStory - Story language
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The game's language, and whether the story's text reads cover it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryLanguage_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryLanguage_H

#include <optional>
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The game's language, for the reads that still use text (item names, the
//  location banner, a sign's or talk's first box), set once from the
//  program's options (StoryRunOptions) before the story runs; English until
//  then. story_reads_text() says whether those reads have a model and
//  dictionary for it (English only, so far); when not, callers use
//  language-free checks: the banner's box showing, a sign's box opening, the
//  first row of a Bag pocket.
Language story_language();
//  Sets the active story language.
void set_story_language(Language language);
//  Returns whether text recognition is supported for `language`, by default
//  the current story language.
bool story_reads_text(std::optional<Language> language = std::nullopt);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

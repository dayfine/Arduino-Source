/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the shared range group
 *  (Pokemon_StoryRangeOptions.h) plus the attempts per checkpoint, the
 *  checks level (Types/Pokemon_StoryCheckLevel.h), the language the game's
 *  steps read text in, and whether it reloads the save before its first
 *  checkpoint.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include <string>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint's label in the start and end dropdowns, as SV labels its
//  segments: "00.09 Pewter Gym: Camper Liam, then Brock". The labels of a
//  story's checkpoints, in order, make its dropdowns
//  (story_checkpoint_database()).
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);


//  The range group (START_CHECKPOINT, END_CHECKPOINT, start_index(),
//  end_index()) with the run's options added to the same group, so the
//  saved settings keep one flat object. Each checkpoint that passes saves
//  as its row's execution says (the game's own autosave, or the menu).
class StoryRunOptions : public StoryRangeOptions{
public:
    //  `checkpoints`: story_checkpoint_database() of the game's story, kept
    //  alive by the caller; the dropdowns start on its first and last.
    //  `languages`: the ones the game has (Pokemon_GameLanguages.h).
    StoryRunOptions(const StringSelectDatabase& checkpoints, const EnumDropdownDatabase<Language>& languages);

    //  The checks level, for the game's steps to read.
    StoryCheckLevel check_level() const;
    //  The game's language, for the steps that read text.
    Language language() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    EnumDropdownOption<StoryCheckLevel> CHECKS;
    //  Any language the game has, whether or not a reader supports it:
    //  steps without one use language-free checks, so none is refused.
    EnumDropdownOption<Language> LANGUAGE;
    //  On (the default), the run loads the save before its first
    //  checkpoint. Off, it starts from where the game is; later checkpoints
    //  reload as before.
    BooleanCheckBoxOption RELOAD_AT_START;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

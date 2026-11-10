/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the shared range group
 *  (Pokemon_StoryRangeOptions.h) plus the run policy, read into the shared
 *  SegmentPolicy (Pokemon/AutoStory/Pokemon_StorySegments.h), the checks
 *  level (Types/Pokemon_StoryCheckLevel.h) and the language the game's steps
 *  read text in.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint's label in the start and end dropdowns, as SV labels its
//  segments: "00.09 Pewter Gym: Camper Liam, then Brock".
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);

//  The start/end dropdown database of a story's checkpoints, `rows` in
//  order, slugged by their index in it, as make_story_select_database()
//  slugs SV's. A game whose story is in sections (BDSP's segments 00 and
//  01) passes them joined: a section's entry i is then story_index(true,
//  i, first of the section). The program keeps it in a function-local
//  static: the options hold a reference.
StringSelectDatabase story_checkpoint_database(std::span<const StoryCheckpoint> rows);


//  Verify mode, off by default (verify_checkpoint()): the start checkpoint
//  RUNS times from its save, never saving at its end; verified when at
//  least PASSES pass. For a battle that has lost twice or more, given its
//  own checkpoint saved right before it.
class StoryVerifyOptions : public GroupOption{
public:
    StoryVerifyOptions();

    //  The runs and passes, unset when verify mode is off.
    std::optional<VerifyPolicy> policy() const;
    virtual std::string check_validity() const override;

public:
    SimpleIntegerOption<uint8_t> RUNS;
    SimpleIntegerOption<uint8_t> PASSES;
};


//  The range group (START_CHECKPOINT, END_CHECKPOINT, start_index(),
//  end_index()) with the run policy added to the same group, so the saved
//  settings keep one flat object.
class StoryRunOptions : public StoryRangeOptions{
public:
    //  `checkpoints`: story_checkpoint_database() of the game's story, kept
    //  alive by the caller; the dropdowns start on its first and last.
    //  `languages`: the ones the game has (Pokemon_GameLanguages.h).
    StoryRunOptions(const StringSelectDatabase& checkpoints, const EnumDropdownDatabase<Language>& languages);

    //  The range's check, then verify mode's: one checkpoint only.
    virtual std::string check_validity() const override;

    //  The policy these options set. In-place recoveries are the game's to
    //  set, not the user's.
    SegmentPolicy policy() const;
    //  The checks level, for the game's steps to read.
    StoryCheckLevel check_level() const;
    //  The game's language, for the steps that read text.
    Language language() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption STOP_AT_FIRST_FAILURE;
    EnumDropdownOption<StoryCheckLevel> CHECKS;
    //  Any language the game has, whether or not a reader supports it:
    //  steps without one use language-free checks, so none is refused.
    EnumDropdownOption<Language> LANGUAGE;
    //  On, it runs one checkpoint: the start, which must also be the end.
    StoryVerifyOptions VERIFY;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

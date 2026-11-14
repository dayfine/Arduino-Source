/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the shared range group
 *  (Pokemon_StoryRangeOptions.h) plus the run policy, read into the shared
 *  SegmentPolicy (Pokemon/AutoStory/Pokemon_StorySegments.h), the checks
 *  level (Types/Pokemon_StoryCheckLevel.h), the language the game's steps
 *  read text in, when the run saves (Types/Pokemon_StorySaveMode.h) and
 *  whether it only checks postconditions.
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
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint's label in the start and end dropdowns, as SV labels its
//  segments: "00.09 Pewter Gym: Camper Liam, then Brock". The labels of a
//  story's checkpoints, in order, make its dropdowns
//  (story_checkpoint_database()).
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);


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

    //  The policy these options set, with the save mode.
    SegmentPolicy policy() const;
    //  Verify mode's runs and passes with the save mode; unset when off.
    std::optional<VerifyPolicy> verify_policy() const;
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
    //  On, it runs one checkpoint: the start, which must also be the end.
    StoryVerifyOptions VERIFY;
    //  When the run saves; NONE (testing) by default.
    EnumDropdownOption<StorySaveMode> SAVE_MODE;
    //  On, nothing runs: the game reads where it stands and checks the end
    //  checkpoint's postconditions (Pokemon_StoryPostconditionCheck.h).
    BooleanCheckBoxOption POSTCONDITIONS_ONLY;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options an AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the shared range group
 *  (Pokemon/AutoStory/Options/Pokemon_StoryRangeOptions.h) plus retry,
 *  verify, check level, language, save mode, and breakpoints.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "Pokemon/AutoStory/Options/Pokemon_StoryRangeOptions.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySaveMode.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How hard a run retries a failed checkpoint and when it saves.
struct StoryRunPolicy{
    size_t attempts_per_checkpoint = 3;
    StorySaveMode save_mode = StorySaveMode::NONE;
};

//  How Verify mode runs one checkpoint from its save.
struct VerifyPolicy{
    size_t runs = 3;
    size_t passes_needed = 2;
    StorySaveMode save_mode = StorySaveMode::NONE;
};

//  A checkpoint's label in the start and end dropdowns:
//  "00.01 Bedroom to the Verity Lakefront".
std::string story_checkpoint_label(std::string_view id, std::string_view title);
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);


//  Verify mode, off by default: the start checkpoint RUNS times from its
//  save; verified when at least PASSES pass.
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
    //  `checkpoints`: make_story_select_database() of the game's story, kept
    //  alive by the caller; the dropdowns start on its first and last.
    //  `languages`: the ones the game has (Pokemon_GameLanguages.h).
    StoryRunOptions(const StringSelectDatabase& checkpoints, const EnumDropdownDatabase<Language>& languages);

    //  The range's check, then verify mode's: one checkpoint only.
    virtual std::string check_validity() const override;

    //  The policy these options set, with the save mode.
    StoryRunPolicy policy() const;
    //  Verify mode's runs and passes with the save mode; unset when off.
    std::optional<VerifyPolicy> verify_policy() const;
    //  The checks level, for the game's steps to read.
    StoryCheckLevel check_level() const;
    //  The game's language, for the steps that read text.
    Language language() const;
    //  Where the run pauses.
    StoryBreakpoints breakpoints() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    EnumDropdownOption<StoryCheckLevel> CHECKS;
    EnumDropdownOption<Language> LANGUAGE;
    StoryVerifyOptions VERIFY;
    EnumDropdownOption<StorySaveMode> SAVE_MODE;
    BooleanCheckBoxOption POSTCONDITIONS_ONLY;
    EnumDropdownOption<StoryBreakpoints> BREAKPOINTS;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

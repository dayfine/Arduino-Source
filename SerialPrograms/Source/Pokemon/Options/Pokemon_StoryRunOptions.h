/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the shared range group
 *  (Pokemon_StoryRangeOptions.h) plus the run policy, read into the shared
 *  SegmentPolicy (Pokemon/AutoStory/Pokemon_StorySegments.h), the checks
 *  level (Types/Pokemon_StoryChecks.h) and the language the game's steps
 *  read text in.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryChecks.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A checkpoint's entry in the start and end dropdowns, as SV labels its
//  segments: "00.09 Pewter Gym: Camper Liam, then Brock".
struct StoryCheckpointEntry{
    std::string label;

    const std::string& name() const{
        return label;
    }
};

// Formats a checkpoint identifier and description into a dropdown label.
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);

//  The start/end dropdown database of a story's checkpoints, `rows` in
//  order, slugged by their index in it (make_story_select_database()). A
//  game whose story is in sections (BDSP's segments 00 and 01) passes them
//  joined: a section's entry i is then story_index(true, i, first of the
//  section). The program keeps it in a function-local static: the options
//  hold a reference.
template <typename Row>
StringSelectDatabase story_checkpoint_database(const std::vector<Row>& rows);

template <typename Row>
StringSelectDatabase story_checkpoint_database(const std::vector<Row>& rows){
    std::vector<std::unique_ptr<StoryCheckpointEntry>> entries;
    for (const StoryCheckpoint& row : rows){
        entries.emplace_back(std::make_unique<StoryCheckpointEntry>(StoryCheckpointEntry{.label = story_checkpoint_label(row)}));
    }
    return make_story_select_database(entries, 0, entries.size());
}


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
    explicit StoryRunOptions(const StringSelectDatabase& checkpoints);

    //  The range's check, then verify mode's: one checkpoint only.
    virtual std::string check_validity() const override;

    //  The policy these options set. In-place recoveries are the game's to
    //  set, not the user's.
    SegmentPolicy policy() const;
    //  The checks level, for the game's steps to read.
    StoryChecks checks() const;
    //  The game's language, for the steps that read text.
    Language language() const;

public:
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption STOP_AT_FIRST_FAILURE;
    EnumDropdownOption<StoryChecks> CHECKS;
    //  Any language the game has, whether or not a reader supports it:
    //  steps without one use language-free checks, so none is refused.
    EnumDropdownOption<Language> LANGUAGE;
    //  On, it runs one checkpoint: the start, which must also be the end.
    StoryVerifyOptions VERIFY;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

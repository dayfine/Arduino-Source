/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The options every AutoStory program shows for where to start and end, how
 *  hard to try and how much to check: the same group in each game, read into the shared
 *  SegmentPolicy (Pokemon/Navigation/Pokemon_StorySegments.h) and the
 *  executor's checks level (Pokemon_StorySteps.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRunOptions_H
#define PokemonAutomation_Pokemon_StoryRunOptions_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "CommonTools/Options/StringSelectOption.h"
#include "Pokemon/AutoStory/Pokemon_AutoStorySelect.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"

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
std::string story_checkpoint_label(const StoryCheckpoint& checkpoint);

//  The start/end dropdown database of a story's checkpoints, `rows` in
//  order, slugged by their index in it (make_story_select_database()). A
//  game whose story is in sections (BDSP's segments 00 and 01) passes them
//  joined: a section's entry i is then story_index(true, i, first of the
//  section). The program keeps it in a function-local static: the options
//  hold a reference.
template <typename Row>
StringSelectDatabase story_checkpoint_database(const std::vector<Row>& rows){
    std::vector<std::unique_ptr<StoryCheckpointEntry>> entries;
    for (const StoryCheckpoint& row : rows){
        entries.emplace_back(std::make_unique<StoryCheckpointEntry>(StoryCheckpointEntry{.label = story_checkpoint_label(row)}));
    }
    return make_story_select_database(entries, 0, entries.size());
}


class StoryRunOptions : public GroupOption{
public:
    //  `checkpoints`: story_checkpoint_database() of the game's story, kept
    //  alive by the caller; the dropdowns start on its first and last.
    explicit StoryRunOptions(const StringSelectDatabase& checkpoints);

    //  The checkpoints to run, by index in the database, both inclusive.
    size_t start_index() const;
    size_t end_index() const;
    //  "The start checkpoint comes after the end checkpoint." if it does.
    virtual std::string check_validity() const override;

    //  The policy these options set. In-place recoveries are the game's to
    //  set, not the user's.
    SegmentPolicy policy() const;
    //  The checks level for the game's story step executor (set_checks()).
    StoryChecks checks() const;
    //  The game's language, for the executor (set_language()).
    Language language() const;

public:
    StringSelectOption START_CHECKPOINT;
    StringSelectOption END_CHECKPOINT;
    SimpleIntegerOption<uint8_t> ATTEMPTS_PER_CHECKPOINT;
    BooleanCheckBoxOption STOP_AT_FIRST_FAILURE;
    EnumDropdownOption<StoryChecks> CHECKS;
    //  Any language the game has, whether or not a reader supports it:
    //  steps without one use language-free checks, so none is refused.
    EnumDropdownOption<Language> LANGUAGE;
};



}
}
#endif

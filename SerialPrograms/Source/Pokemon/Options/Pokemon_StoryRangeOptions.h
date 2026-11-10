/*  Story Range Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The start and end checkpoint dropdowns of an AutoStory program, over one
 *  list of the story's checkpoints, and the check that the start does not
 *  come after the end. StoryRunOptions (Pokemon_StoryRunOptions.h) is this
 *  group plus the run policy.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRangeOptions_H
#define PokemonAutomation_Pokemon_StoryRangeOptions_H

#include <string>
#include "Common/Cpp/Options/GroupOption.h"
#include "CommonTools/Options/StringSelectOption.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A program shows the group, or adds START_CHECKPOINT and END_CHECKPOINT
//  to its own options under its own keys (keeping the keys its saved
//  settings already use) and calls check_validity() from its own.
class StoryRangeOptions : public GroupOption{
public:
    //  `checkpoints`: the dropdown database (make_story_select_database()
    //  or story_checkpoint_database()), kept alive by the caller. The
    //  dropdowns start on its first and last entries.
    StoryRangeOptions(std::string label, const StringSelectDatabase& checkpoints);

    //  The checkpoints chosen, by index in the database, both inclusive.
    size_t start_index() const;
    size_t end_index() const;
    //  "The start checkpoint comes after the end checkpoint." if it does.
    virtual std::string check_validity() const override;

public:
    StringSelectOption START_CHECKPOINT;
    StringSelectOption END_CHECKPOINT;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

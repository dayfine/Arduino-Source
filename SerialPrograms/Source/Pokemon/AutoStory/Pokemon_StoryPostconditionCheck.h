/*  Story Postcondition Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint's postconditions (Types/Pokemon_StoryPostconditions.h)
 *  checked against what the game read once the run was over: a step of its
 *  own, not the run's verdict, which can be wrong. A postcondition the game
 *  can't read yet is reported as not checked, never as passed.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPostconditionCheck_H
#define PokemonAutomation_Pokemon_StoryPostconditionCheck_H

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryPostconditions.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What the game read after the run. Each part is unset (or, for items and
//  flags, absent from the map) when the game can't read it: the matching
//  postconditions are then not checked.
struct ObservedState{
    std::optional<std::string> map = std::nullopt;
    std::optional<TilePos> tile = std::nullopt;
    std::optional<Direction> facing = std::nullopt;
    //  The Bag's count of each item read, 0 for one read and not there.
    std::map<std::string, size_t, std::less<>> items = {};
    //  Each story flag read, and whether it is set.
    std::map<std::string, bool, std::less<>> flags = {};
};

// The result of one postcondition.
enum class PostconditionVerdict{
    PASSED,
    FAILED,
    NOT_CHECKED,
};

//  One postcondition checked: "map", "tile", "facing", "item LIFT_KEY",
//  "flag ...", its verdict and what was expected and read.
struct PostconditionResult{
    std::string what = {};
    PostconditionVerdict verdict = PostconditionVerdict::NOT_CHECKED;
    std::string detail = {};
};

// Every postcondition of one checkpoint, checked.
struct PostconditionReport{
    std::string checkpoint = {};
    std::vector<PostconditionResult> results = {};

    size_t count(PostconditionVerdict verdict) const;
    //  Every postcondition was read and held: none failed, none unchecked,
    //  and there was at least one.
    bool all_passed() const;
};

//  Checks the end position (postconditions.position if set, else
//  `fallback_end_pose`), then each item count and flag, against `observed`.
PostconditionReport check_postconditions(
    std::string_view checkpoint_id,
    const StoryPostconditions& postconditions,
    const std::optional<GridPose>& fallback_end_pose,
    const ObservedState& observed
);

//  The report as log lines, one per postcondition and a summary:
//      [Postcondition] 00.31 map: passed: RocketHideout_B3F
//      [Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1
//      [Postcondition] 00.31 flag hideout_lift_key_grunt: not checked: the game can't read it
//      [Postcondition] 00.31: 3 passed, 1 failed, 1 not checked: failed
//  The summary is "passed" only when every postcondition passed;
//  "incomplete" when none failed and some were not checked.
std::vector<std::string> postcondition_log_lines(const PostconditionReport& report);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

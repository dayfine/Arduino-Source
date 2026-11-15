/*  Story Checkpoint Metadata
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a checkpoint's JSON says about the checkpoint rather than how to
 *  play it: its phase (testing, verification or reproduction, below) and
 *  where it came from.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpointMetadata_H
#define PokemonAutomation_Pokemon_StoryCheckpointMetadata_H

#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace Pokemon{


//  Where a checkpoint stands. It moves forward only on passes; a change to
//  its steps moves it back to TESTING.
enum class StoryPhase{
    //  New or changed: one run, never saves.
    TESTING,
    //  Passed its test: 2-3 runs from the same start save, then one save.
    VERIFICATION,
    //  Verified: a playthrough saves at its end.
    REPRODUCTION,
};

//  The wire and log name of a phase: "testing", "verification", "reproduction".
std::string story_phase_name(StoryPhase phase);

//  The phase named `name`; none for another name.
std::optional<StoryPhase> read_story_phase(std::string_view name);

//  A checkpoint's "metadata" object.
struct StoryCheckpointMetadata{
    StoryPhase phase = StoryPhase::TESTING;
    //  Where the steps came from: a walkthrough, a console recording.
    std::string source = {};

    bool operator==(const StoryCheckpointMetadata& other) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

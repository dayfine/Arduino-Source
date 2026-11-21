/*  Story Checkpoint Metadata
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a checkpoint's JSON says about the checkpoint rather than how to
 *  play it: its phase (autostory/SAVE_POLICY.md on the notes branch), the
 *  steps it was verified with, and where it came from. The engine picks
 *  the save mode from the phase, and only while the steps still hash to
 *  the verified ones (Engine/Pokemon_StoryEnginePolicy.h).
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

//  The runs that moved the checkpoint out of testing, as recorded.
struct StoryVerification{
    //  story_steps_hash() of the steps those runs ran: "sha256:<hex>".
    std::string steps_hash = {};
    //  The build those runs used (a commit).
    std::string build = {};
    std::string date = {};
    //  Passes out of runs, e.g. "3/3".
    std::string runs = {};

    bool operator==(const StoryVerification& other) const = default;
};

//  A checkpoint's "metadata" object, less its postconditions, which the
//  checkpoint keeps beside its steps (StoryCheckpoint::postconditions).
struct StoryCheckpointMetadata{
    StoryPhase phase = StoryPhase::TESTING;
    std::optional<StoryVerification> verified = std::nullopt;
    //  Where the steps came from: a walkthrough, a console recording.
    std::string source = {};

    bool operator==(const StoryCheckpointMetadata& other) const = default;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

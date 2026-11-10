/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryRunOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


const EnumDropdownDatabase<StoryCheckLevel>& StoryCheckLevel_Database(){
    static const EnumDropdownDatabase<StoryCheckLevel> database({
        {StoryCheckLevel::TESTING,  "testing",  "Testing: run each checkpoint 3 times to be sure it really passes"},
        {StoryCheckLevel::THOROUGH, "thorough", "Thorough: check everything for the user (Bag items, Pok\u00e9mon stats, preconditions)"},
        {StoryCheckLevel::DEFAULT,  "default",  "Default: check the most important things, assume the rest is fine"},
        {StoryCheckLevel::FAST,     "fast",     "Fast: minimal checks"},
        {StoryCheckLevel::SPEEDRUN, "speedrun", "Speedrun: almost no checks, as fast as possible"},
    });
    return database;
}


std::string story_checkpoint_label(const StoryCheckpoint& checkpoint){
    return checkpoint.title.empty() ? checkpoint.id : checkpoint.id + " " + checkpoint.title;
}



StoryVerifyOptions::StoryVerifyOptions()
    : GroupOption(
        "Verify Mode: run the start checkpoint (also the end) from its save several times, never saving at its end",
        LockMode::LOCK_WHILE_RUNNING,
        EnableMode::DEFAULT_DISABLED
    )
    , RUNS(
        "<b>Runs:</b><br>Runs of the checkpoint, each from its save.",
        LockMode::LOCK_WHILE_RUNNING,
        3, 1, 20
    )
    , PASSES(
        "<b>Passes needed:</b><br>Runs that must pass for the checkpoint to be verified.",
        LockMode::LOCK_WHILE_RUNNING,
        2, 1, 20
    )
{
    PA_ADD_OPTION(RUNS);
    PA_ADD_OPTION(PASSES);
}

std::optional<VerifyPolicy> StoryVerifyOptions::policy() const{
    if (!enabled()){
        return std::nullopt;
    }
    return VerifyPolicy{.runs = RUNS, .passes_needed = PASSES};
}

std::string StoryVerifyOptions::check_validity() const{
    if (enabled() && PASSES > RUNS){
        return "Verify mode can't need more passes than it has runs.";
    }
    return GroupOption::check_validity();
}


StoryRunOptions::StoryRunOptions(const StringSelectDatabase& checkpoints, const EnumDropdownDatabase<Language>& languages)
    : StoryRangeOptions("Story Run", checkpoints)
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per checkpoint:</b><br>A failed checkpoint reloads its save and runs again, up to this many times in all. "
        "1 stops at the first failure, leaving the game where it failed.",
        LockMode::LOCK_WHILE_RUNNING,
        3, 1, 20
    )
    , CHECKS(
        "<b>Checks:</b><br>How much each step reads the screen to confirm what it did, from Testing (the most) to Speedrun (almost none).",
        StoryCheckLevel_Database(),
        LockMode::LOCK_WHILE_RUNNING,
        StoryCheckLevel::DEFAULT
    )
    , LANGUAGE(
        "<b>Game language:</b><br>For the few reads that need text (item names in a shop or the Bag). "
        "Story steps never match dialogue text; with no reader for this language they check positions, digits and the screen instead.",
        languages,
        LockMode::LOCK_WHILE_RUNNING,
        Language::English
    )
{
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(CHECKS);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(VERIFY);
}

std::string StoryRunOptions::check_validity() const{
    std::string error = StoryRangeOptions::check_validity();
    if (!error.empty()){
        return error;
    }
    if (VERIFY.enabled() && start_index() != end_index()){
        return "Verify mode runs one checkpoint: set the end checkpoint to the start checkpoint.";
    }
    return error;
}

SegmentPolicy StoryRunOptions::policy() const{
    SegmentPolicy policy;
    policy.attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT;
    return policy;
}

StoryCheckLevel StoryRunOptions::check_level() const{
    return CHECKS;
}

Language StoryRunOptions::language() const{
    return LANGUAGE;
}



} // namespace Pokemon
} // namespace PokemonAutomation

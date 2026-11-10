/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryRunOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


const EnumDropdownDatabase<StoryChecks>& StoryChecks_Database(){
    static const EnumDropdownDatabase<StoryChecks> database({
        {StoryChecks::THOROUGH, "thorough", "Thorough: confirm purchases, the Bag, status and position on screen"},
        {StoryChecks::FAST,     "fast",     "Fast: trust counts and timings"},
    });
    return database;
}

const EnumDropdownDatabase<Language>& StoryLanguage_Database(){
    static const EnumDropdownDatabase<Language> database({
        {Language::English,            "en",      "English"},
        {Language::Japanese,           "ja",      "Japanese"},
        {Language::Spanish,            "es",      "Spanish"},
        {Language::French,             "fr",      "French"},
        {Language::German,             "de",      "German"},
        {Language::Italian,            "it",      "Italian"},
        {Language::Korean,             "ko",      "Korean"},
        {Language::ChineseSimplified,  "zh-Hans", "Chinese (Simplified)"},
        {Language::ChineseTraditional, "zh-Hant", "Chinese (Traditional)"},
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


StoryRunOptions::StoryRunOptions(const StringSelectDatabase& checkpoints)
    : StoryRangeOptions("Story Run", checkpoints)
    , ATTEMPTS_PER_CHECKPOINT(
        "<b>Attempts per checkpoint:</b><br>A failed checkpoint reloads its save and runs again, up to this many times in all.",
        LockMode::LOCK_WHILE_RUNNING,
        3, 1, 20
    )
    , STOP_AT_FIRST_FAILURE(
        "<b>Stop at the first failure:</b><br>Stop without reloading, leaving the game where it failed for a look.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , CHECKS(
        "<b>Checks:</b><br>Thorough reads the screen to confirm what each step did; Fast trusts counts and timings, for a run in a rush.",
        StoryChecks_Database(),
        LockMode::LOCK_WHILE_RUNNING,
        StoryChecks::THOROUGH
    )
    , LANGUAGE(
        "<b>Game language:</b><br>For the few reads that need text (item names in a shop or the Bag). "
        "Story steps never match dialogue text; with no reader for this language they check positions, digits and the screen instead.",
        StoryLanguage_Database(),
        LockMode::LOCK_WHILE_RUNNING,
        Language::English
    )
{
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(STOP_AT_FIRST_FAILURE);
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
    policy.stop_at_first_failure = STOP_AT_FIRST_FAILURE;
    return policy;
}

StoryChecks StoryRunOptions::checks() const{
    return CHECKS;
}

Language StoryRunOptions::language() const{
    return LANGUAGE;
}



} // namespace Pokemon
} // namespace PokemonAutomation

/*  Story Run Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string_view>
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


const EnumDropdownDatabase<StorySaveMode>& StorySaveMode_Database(){
    static const EnumDropdownDatabase<StorySaveMode> database({
        {StorySaveMode::NONE,               "none",               "None: never save (testing a checkpoint; the start save stays)"},
        {StorySaveMode::AFTER_VERIFICATION, "after_verification", "After verification: save once, at the end of a run that passed"},
        {StorySaveMode::PER_CHECKPOINT,     "per_checkpoint",     "Each checkpoint: save after each checkpoint that passed (a playthrough)"},
    });
    return database;
}


namespace{

constexpr std::string_view SAVE_MODE_LABEL =
    "<b>Save:</b><br>When the run saves the game. A failed run never saves. "
    "None keeps the save the run started from. After verification saves once, at the end of a run that passed "
    "(in Verify Mode, at the end of the last run, when every run passed). Each checkpoint saves after each "
    "checkpoint that passed, for a playthrough of verified checkpoints.";

constexpr std::string_view RELOAD_AT_START_LABEL =
    "<b>Reload the Save at the Start:</b><br>Close the game and load the save before the first checkpoint. "
    "Off: start from wherever the game is (resumed from HOME if need be), for a game already at the start checkpoint. "
    "Later checkpoints reload as usual. Off needs Save set to None.";

} // namespace


std::string story_checkpoint_label(const StoryCheckpoint& checkpoint){
    return checkpoint.title.empty() ? checkpoint.id : std::format("{} {}", checkpoint.id, checkpoint.title);
}



StoryVerifyOptions::StoryVerifyOptions()
    : GroupOption(
        "Verify Mode: run the start checkpoint (also the end) from its save several times. "
        "Only with Save set to After verification does the last run save at its end, when every run passed",
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
    , SAVE_MODE(std::string(SAVE_MODE_LABEL), StorySaveMode_Database(), LockMode::LOCK_WHILE_RUNNING, StorySaveMode::NONE)
    , RELOAD_AT_START(std::string(RELOAD_AT_START_LABEL), LockMode::LOCK_WHILE_RUNNING, true)
    , POSTCONDITIONS_ONLY(
        "<b>Check postconditions only:</b><br>Run nothing: read where the game stands and check the end checkpoint's "
        "position, items and flags. What the game can't read yet is reported as not checked.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
{
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(CHECKS);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(VERIFY);
    PA_ADD_OPTION(SAVE_MODE);
    PA_ADD_OPTION(RELOAD_AT_START);
    PA_ADD_OPTION(POSTCONDITIONS_ONLY);
}

std::string StoryRunOptions::check_validity() const{
    std::string error = StoryRangeOptions::check_validity();
    if (!error.empty()){
        return error;
    }
    if (VERIFY.enabled() && start_index() != end_index()){
        return "Verify mode runs one checkpoint: set the end checkpoint to the start checkpoint.";
    }
    if (VERIFY.enabled() && POSTCONDITIONS_ONLY){
        return "Check postconditions only runs nothing: turn off Verify Mode.";
    }
    if (!RELOAD_AT_START && SAVE_MODE != StorySaveMode::NONE){
        return "Reload the Save at the Start is off: the run starts from wherever the game is, which nothing checked, "
            "so it must not save. Set Save to None or turn the reload on.";
    }
    return error;
}

SegmentPolicy StoryRunOptions::policy() const{
    SegmentPolicy policy;
    policy.attempts_per_checkpoint = ATTEMPTS_PER_CHECKPOINT;
    policy.save_mode = SAVE_MODE;
    return policy;
}

std::optional<VerifyPolicy> StoryRunOptions::verify_policy() const{
    std::optional<VerifyPolicy> policy = VERIFY.policy();
    if (policy){
        policy->save_mode = SAVE_MODE;
    }
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

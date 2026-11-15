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


namespace{

const EnumDropdownDatabase<StoryCheckLevel>& StoryCheckLevel_Database(){
    static const EnumDropdownDatabase<StoryCheckLevel> database({
        {StoryCheckLevel::TESTING,  "testing",  "Testing: check everything, as Thorough does, while a checkpoint is new"},
        {StoryCheckLevel::THOROUGH, "thorough", "Thorough: check everything for the user (Bag items, Pok\u00e9mon stats, preconditions)"},
        {StoryCheckLevel::DEFAULT,  "default",  "Default: check the most important things, assume the rest is fine"},
        {StoryCheckLevel::FAST,     "fast",     "Fast: minimal checks"},
        {StoryCheckLevel::SPEEDRUN, "speedrun", "Speedrun: almost no checks, as fast as possible"},
    });
    return database;
}


constexpr std::string_view RELOAD_AT_START_LABEL =
    "<b>Reload the Save at the Start:</b><br>Close the game and load the save before the first checkpoint. "
    "Off: start from wherever the game is (resumed from HOME if need be), for a game already at the start checkpoint. "
    "Later checkpoints reload as usual.";

} // namespace


std::string story_checkpoint_label(const StoryCheckpoint& checkpoint){
    return checkpoint.title.empty() ? checkpoint.id : std::format("{} {}", checkpoint.id, checkpoint.title);
}



StoryRunOptions::StoryRunOptions(const StringSelectDatabase& checkpoints)
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

    , RELOAD_AT_START(std::string(RELOAD_AT_START_LABEL), LockMode::LOCK_WHILE_RUNNING, true)
{
    PA_ADD_OPTION(ATTEMPTS_PER_CHECKPOINT);
    PA_ADD_OPTION(CHECKS);

    PA_ADD_OPTION(RELOAD_AT_START);
}

StoryCheckLevel StoryRunOptions::check_level() const{
    return CHECKS;
}




} // namespace Pokemon
} // namespace PokemonAutomation

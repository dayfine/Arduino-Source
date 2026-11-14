/*  PokemonLZA AutoStory - Start load
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which save a run loads to reach its first checkpoint's start: the
 *  latest, or the backup save when the game autosaved past the start; or
 *  none, the game kept as it runs.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStartLoad_H
#define PokemonAutomation_PokemonLZA_AutoStoryStartLoad_H

#include <optional>
#include <span>
#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The save a load brings back: the latest, or the backup save (the title
//  screen's Up + B + X: the one before the latest); RUNNING loads none,
//  the game is kept as it runs, resumed from HOME if need be.
enum class StartSave{
    LATEST,
    BACKUP,
    RUNNING,
};

//  The shared launch's request for `save`: LATEST and BACKUP close a
//  running game and load that save; RUNNING keeps a game on the overworld.
Pokemon::StoryLaunchRequest start_launch_request(StartSave save);

//  Where a run takes its first checkpoint's start from, as the user set it.
enum class StartFrom{
    //  The latest save, then the backup save when the latest is past the
    //  start.
    LATEST_SAVE,
    //  The backup save only (START_FROM_BACKUP_SAVE).
    BACKUP_SAVE,
    //  The game as it runs, no reload (RELOAD_AT_START off).
    RUNNING_GAME,
};

//  One load of a run's start save and its start check: whether the game
//  showed the checkpoint's start; nothing when the checkpoint has no check.
struct StartLoad{
    StartSave save = StartSave::LATEST;
    std::optional<bool> at_start;
};

//  What a run does next while loading its start.
enum class StartLoadAction{
    LOAD_LATEST,
    LOAD_BACKUP,
    //  Keep the running game (StartSave::RUNNING).
    RESUME,
    START,
    //  Save with the menu's Report, then start: the latest save is the
    //  start again, so the next run of the checkpoint loads it.
    REPORT_THEN_START,
    STOP,
};

//  The next action after `loads`: the save `from` names first (RESUME for
//  RUNNING_GAME), then the backup save when the latest's start check
//  failed (the game autosaved past the start: catches, heals), and STOP
//  when the last load's check failed otherwise: a kept game is never
//  reloaded. START after a load whose check passed or that has no check,
//  REPORT_THEN_START when that load is the backup save after the latest's
//  check failed. Never a save without a passed check: no save from an
//  unknown spot.
StartLoadAction next_start_load_action(StartFrom from, std::span<const StartLoad> loads);

//  What a checkpoint's start check looks for: the screen event its start
//  shows, and the place that puts the player at, for the log.
struct StartSign{
    ScreenEvent event;
    std::string_view place;
};

//  `row`'s start sign; nothing when `row` has no start check. 03.06 and
//  03.07d start at the Pokemon Center counter, facing the nurse: her "A"
//  talk prompt shows.
std::optional<StartSign> checkpoint_start_sign(std::string_view row);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

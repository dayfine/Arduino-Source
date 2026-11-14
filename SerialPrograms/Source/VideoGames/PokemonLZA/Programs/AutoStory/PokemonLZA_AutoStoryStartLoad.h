/*  PokemonLZA AutoStory - Start load
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which save a run loads to reach its first checkpoint's start: the
 *  latest, or the backup save when the game autosaved past the start.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStartLoad_H
#define PokemonAutomation_PokemonLZA_AutoStoryStartLoad_H

#include <optional>
#include <span>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The save a load brings back: the latest, or the backup save (the title
//  screen's Up + B + X: the one before the latest).
enum class StartSave{
    LATEST,
    BACKUP,
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
    START,
    STOP,
};

//  The next action after `loads`: the latest save first (the backup save
//  with `backup_first`, the run's override), then the backup save when the
//  latest's start check failed (the game autosaved past the start: catches,
//  heals), and STOP when the last load's check failed too. START after a
//  load whose check passed or that has no check.
StartLoadAction next_start_load_action(bool backup_first, std::span<const StartLoad> loads);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

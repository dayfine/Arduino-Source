/*  PokemonLZA AutoStory - Backup save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console's backup save, for a run that rewinds to it
 *  (PokemonLZA_AutoStoryBackups.h). At the title, holding Up with B + X
 *  opens the BACKUP card; A loads it. The backup exists from the first
 *  Pokemon Center (03.05) on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackupSave_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackupSave_H

#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackups.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The backup save on the console `env`, which must outlive it. open_card
//  relaunches PLZA.
BackupSave console_backup_save(StoryEnv& env);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

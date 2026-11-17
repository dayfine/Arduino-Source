/*  PokemonLZA AutoStory - Backup save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console's backup save, for a run that rewinds to it
 *  (PokemonLZA_AutoStoryBackups.h). At the title ("Press A" under the
 *  logo), holding Up with B + X opens the BACKUP card; A loads it, through
 *  "Backup data loaded." and a warning that saving overwrites the current
 *  data, into the game (console B, 2026-11-16). The backup exists from the
 *  first Pokemon Center (03.05) on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackupSave_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackupSave_H

#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackups.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The backup save on the console `env`, which must outlive it:
//    - open_card: closes the game, launches PLZA from HOME
//      (launch_plza_from_home()), waits for the title, holds Up with B + X
//      and reads the card (read_backup_card_registered());
//    - load: A through the card and its messages into the game
//      (gamemenu_to_ingame(), as upstream's backup load);
//    - cancel: B.
BackupSave console_backup_save(StoryEnv& env);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

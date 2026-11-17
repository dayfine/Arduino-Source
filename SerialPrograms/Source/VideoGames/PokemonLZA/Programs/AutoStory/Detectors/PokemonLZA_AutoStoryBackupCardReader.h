/*  PokemonLZA AutoStory - Backup card reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The BACKUP card the title screen's Up + B + X opens: a white card with
 *  the backup's last save time, play time and "Registered in the Pokedex"
 *  count, over the button that loads it (console B, 2026-11-16, Italian:
 *  "Registrati nel Pokedex" 1, "Inizia usando i dati di backup").
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBackupCardReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryBackupCardReader_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On the backup card, its "Registered in the Pokedex" count, read by OCR
//  (numbers read the same in every language). None for a frame with no
//  backup card, an empty one included, or no number there.
std::optional<size_t> read_backup_card_registered(Logger& logger, const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

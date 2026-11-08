/*  FRLG Authored Checkpoint Export
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_AutoStoryExport_H
#define PokemonAutomation_PokemonFRLG_AutoStoryExport_H
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonFRLG_AutoStoryEvents.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
//  Capture evaluated shared fields and every FRLG checkpoint extension.
//  Native supplies are bookkeeping, not observed state or entry requirements.
//  Runtime options, map pins, effective policies and execution remain unbound.
JsonValue story_checkpoint_export(const StoryCheckpoint& checkpoint);
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

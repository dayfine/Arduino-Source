/*  Legacy FRLG Checkpoint JSON Writer
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_PokemonFRLG_CheckpointJsonWriter_H
#define PokemonAutomation_PokemonFRLG_CheckpointJsonWriter_H

#include "Common/Cpp/Json/JsonValue.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Migration diagnostics only: capture shared fields and FRLG bookkeeping.
//  This is not the authored production format or observed entry requirements.
//  Runtime configuration, map pins, policies and executor bindings remain unbound.
JsonValue story_checkpoint_export(const StoryCheckpoint& checkpoint);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

#endif

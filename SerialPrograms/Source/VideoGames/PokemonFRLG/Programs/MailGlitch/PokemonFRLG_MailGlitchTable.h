/*  Mail Glitch Plan Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Loads the embedded pokesim-authored-checkpoint/1 plan files (mg-01..mg-05)
 *  for the Mail Glitch Activation program.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitchTable_H
#define PokemonAutomation_PokemonFRLG_MailGlitchTable_H

#include <string>
#include <utility>
#include <vector>
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCheckpoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  (name, JSON text) of each file in Programs/MailGlitch/PlanData/, generated
//  at build time by pa_embed_text_files().
const std::vector<std::pair<std::string, std::string>>& mail_glitch_checkpoint_json();

//  All Mail Glitch Activation checkpoints in execution order (mg-01..mg-05).
const std::vector<StoryCheckpoint>& mail_glitch_checkpoints();


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

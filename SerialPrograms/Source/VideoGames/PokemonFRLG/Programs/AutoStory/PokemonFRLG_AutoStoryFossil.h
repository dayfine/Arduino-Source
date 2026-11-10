/*  AutoStory Fossil
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The fossils Mt. Moon offers and the story table's fossil CHOICE step.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFossil_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFossil_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The `fossil` option: HELIX by default (the world record's).
enum class Fossil{
    DOME,
    HELIX,
};
//  The human-readable name of a fossil choice.
std::string_view fossil_name(Fossil fossil);
//  Parses a fossil choice from its name.
std::optional<Fossil> fossil_from_name(std::string_view name);

//  Where `fossil` lies on MtMoon_B2F (map.json object_events).
Pokemon::TilePos fossil_tile(Fossil fossil);

//  The GAME action of the fossil CHOICE: fossil WORD (required), "helix"
//  or "dome".
inline const std::string FOSSIL_ACTION = "fossil";
//  The fossil a FOSSIL_ACTION step's args choose; nothing if it isn't one.
std::optional<Fossil> fossil_of(const Pokemon::StoryActionArgs& args);

//  The table's CHOICE(fossil) step, the player facing the fossil.
Pokemon::StoryStep fossil_step(std::string name, std::string why, Fossil fossil);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

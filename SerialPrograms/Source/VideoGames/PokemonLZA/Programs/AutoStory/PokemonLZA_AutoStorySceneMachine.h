/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Scene progression state machine for PLZA AutoStory.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How long a Plus press is held to skip a cutscene.
constexpr Milliseconds CUTSCENE_SKIP_HOLD = Milliseconds(2000);

//  Configuration for run_scene_machine(): terminal conditions.
struct SceneMachineConfig{
    bool end_on_black_fade = false;
};

//  Formats `look`'s active events and readings as a comma-separated string, or "nothing".
std::string describe_scene_look(const ScreenObservation& look);

//  Advances scenes according to `config` until a terminal screen is reached or `timeout` elapses.
//  Returns failure codes: "game_not_running" or "scene_timeout".
std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

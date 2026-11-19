/*  PokemonLZA AutoStory - Action table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Every action a PLZA plan file may name, each parsing its own args from
 *  the checkpoint's JSON. Walks, waits and the scene machine run on any
 *  backend; the actions that need the console's own code run it through
 *  PlzaConsoleHooks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActionTable_H
#define PokemonAutomation_PokemonLZA_AutoStoryActionTable_H

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryAutosaves.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryHeading.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A console-only action. `timeout` is the step's "timeout_ms": the gate
//  runs read it, the others ignore it.
using PlzaConsoleAction = MoveOnlyFunction<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend, Milliseconds timeout)>;
using PlzaConsoleActions = std::map<std::string, PlzaConsoleAction, std::less<>>;

//  Fights the battle on screen until it ends.
using PlzaBattleRunner = MoveOnlyFunction<std::optional<Pokemon::StoryFailure>(PlzaBackend& backend)>;

//  The console's own code that plza_story_actions() composes into the
//  actions: each console-only action name forwards to `console`, a walk
//  with a "heading" reads `arrow`, "story_battle_start" holds ZL with
//  `lock_on` and "story_battle" runs `battle`. Off a console (the sim, the
//  logic tests) all are empty, so the same plan files load and run there
//  and a step that needs the console fails as "console_only".
struct PlzaConsoleHooks{
    PlzaConsoleActions console = {};
    std::optional<MinimapArrowReader> arrow = std::nullopt;
    std::optional<LockOnWatch> lock_on = std::nullopt;
    PlzaBattleRunner battle = {};
    //  Where a menu save counts as an autosave; none: nowhere.
    AutosaveSightings* sightings = nullptr;
    //  The stick heading of a walk with "turn_to_starter" (01.03's starter pick).
    double starter_turn = -70;
};

//  Every action a PLZA checkpoint may name, bound to `hooks`, which must
//  outlive the steps parsed with them.
Pokemon::StoryActions plza_story_actions(PlzaConsoleHooks& hooks);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

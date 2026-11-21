/*  Trainer Battle Functions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_TrainerBattle_H
#define PokemonAutomation_PokemonLZA_TrainerBattle_H

#include <functional>
#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{

template <typename Type> class ControllerContext;
class ProgramEnvironment;

namespace NintendoSwitch{

class ProController;
using ProControllerContext = ControllerContext<ProController>;

namespace PokemonLZA{




class TrainerBattleState{
public:
    TrainerBattleState(BattleAIOption& settings);

    // Try to lock onto an opponent pokemon and launch an attack.
    // - use_move_ai: if True, select available moves based on type effectiveness. Otherwise, select first
    //   available move in the order of top, left right and bottom.
    // - use_plus_moves: if True, press + to launch plus moves.
    // - allow_button_B_press: if true, press button B if it cannot detect move effectiveness symbols.
    //   This lets the function clear any pre-battle transparent dialog window it encounters.
    //   If false, it will be external code's responsibility to clear those transparent dialog windows.
    bool attempt_one_attack(
        ProgramEnvironment& env,
        ConsoleHandle& console,
        ProControllerContext& context
    );


private:
    void run_lock_recovery(ConsoleHandle& console, ProControllerContext& context);


private:
    BattleAIOption& m_settings;

    size_t m_consecutive_failures;
};


// Fight a battle until `end_watcher` fires: mash B (it clears battle dialogs)
// until the battle menu (RunFromBattleWatcher) or `end_watcher` shows; on the
// battle menu, attack once with `battle_state` and repeat.
// - after_failed_attack: if set, called when an attack finds no move to use.
// Return:
// -  0 if `end_watcher` fires
// - -1 if neither the battle menu nor `end_watcher` shows for `mash_time`
int run_battle_until(
    ProgramEnvironment& env,
    ConsoleHandle& console,
    ProControllerContext& context,
    TrainerBattleState& battle_state,
    VisualInferenceCallback& end_watcher,
    Milliseconds mash_time = Milliseconds(120000),
    const std::function<void(ProControllerContext&)>& after_failed_attack = nullptr
);




}
}
}
#endif

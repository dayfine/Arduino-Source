/*  BDSP AutoStory - Wild battle escape policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonBDSP_AutoStory_WildBattlePolicy.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


WildEscapeResult escape_wild_battle(WildBattleEscapeActions& actions, const WildBattleEscapePolicy& policy){
    size_t refusals = 0;
    for (size_t attempt = 1; attempt <= policy.attempts; attempt++){
        actions.note(std::format("Run attempt {}/{}.", attempt, policy.attempts));
        switch (actions.run()){
        case WildRunOutcome::ESCAPED:
            return WildEscapeResult::ESCAPED;
        case WildRunOutcome::TRAPPED:
            refusals++;
            actions.note(std::format("The game refused the escape ({} so far).", refusals));
            if (refusals >= policy.run_refusals && policy.fallback == TrappedFallback::FIGHT){
                actions.note("Trapped: fighting the battle to its end instead.");
                actions.fight();
                return WildEscapeResult::FOUGHT;
            }
            break;
        case WildRunOutcome::NO_RUNNING:
            actions.note("A Trainer battle: there is no running; fighting it as a trainer battle.");
            actions.fight_trainer();
            return WildEscapeResult::TRAINER_FOUGHT;
        case WildRunOutcome::UNCERTAIN:
            break;
        }
    }
    return WildEscapeResult::UNCONFIRMED;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

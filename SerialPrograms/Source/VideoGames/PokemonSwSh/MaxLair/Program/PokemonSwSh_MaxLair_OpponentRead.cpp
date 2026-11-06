/*  Max Lair Opponent Read
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "PokemonSwSh_MaxLair_OpponentRead.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{
namespace{

bool process_header_read(
    Logger& logger,
    GlobalState& state,
    std::set<std::string> mon
){
    if (mon.size() == 1){
        state.opponent = std::move(mon);
        return true;
    }
    if (mon.size() > 1){
        logger.log("Ambiguous Read Result: " + set_to_str(mon), COLOR_PURPLE);
        if (state.opponent.size() == 1 && mon.find(*state.opponent.begin()) != mon.end()){
            logger.log(
                "Using previous known value to disambiguate: " + set_to_str(mon),
                COLOR_PURPLE
            );
            return true;
        }
    }
    if (state.opponent.size() == 1){
        logger.log(
            "Failed to read opponent from battle. Using previously known value: " + set_to_str(state.opponent),
            COLOR_ORANGE
        );
        return true;
    }

    return false;
}


void read_summary(Logger& logger, GlobalState& state, OpponentReadActions& actions){
    logger.log("Unable to read opponent from battle. Attempting to read from summary.", COLOR_ORANGE);
    actions.open_summary();
    actions.wait_for_inputs();
    state.opponent = actions.read_summary();
    actions.close_summary();
    if (state.in_boss_battle_with_known_boss()){
        if (!state.opponent.empty() && *state.opponent.begin() != state.boss){
            logger.log(
                "Inconsistent Boss: Expected " + state.boss + ", Read: " + *state.opponent.begin(),
                COLOR_RED
            );
        }
        state.opponent = {state.boss};
    }
}

} // namespace

bool read_battle_opponent(Logger& logger, GlobalState& state, OpponentReadActions& actions){
    bool retry = !process_header_read(logger, state, actions.read_header());
    if (!state.accept_opponent()){
        retry = true;
    }
    if (retry){
        read_summary(logger, state, actions);
    }
    actions.wait_for_inputs();
    if (!state.accept_opponent()){
        logger.log("Opponent does not match the battle stage after summary read.", COLOR_RED);
        actions.report_invalid();
        return false;
    }
    return true;
}

} // namespace MaxLairInternal
} // namespace PokemonSwSh
} // namespace NintendoSwitch
} // namespace PokemonAutomation

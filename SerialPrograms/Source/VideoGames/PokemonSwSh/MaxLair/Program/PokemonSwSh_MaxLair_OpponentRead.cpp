/*  Max Lair Opponent Read
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#include <format>
#include <string_view>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "PokemonSwSh_MaxLair_OpponentRead.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{
namespace{

//  Records the state around each acceptance pass without invoking it again.
void log_opponent_state(Logger& logger, const GlobalState& state, std::string_view phase){
    logger.log(std::format(
        "[MaxLair][opponent] {}: wins={}, expected_stage={}, candidates={}, known_boss={}",
        phase, state.wins, state.wins == 3 ? "boss" : "rental",
        set_to_str(state.opponent), state.boss
    ));
}

bool process_header_read(
    Logger& logger,
    GlobalState& state,
    std::set<std::string> mon
){
    logger.log(std::format("[MaxLair][opponent] header: raw={}, cached={}", set_to_str(mon), set_to_str(state.opponent)));
    if (mon.size() == 1){
        state.opponent = std::move(mon);
        return true;
    }
    if (mon.size() > 1){
        logger.log("Ambiguous Read Result: " + set_to_str(mon), COLOR_PURPLE);
        if (state.opponent.size() == 1 && mon.find(*state.opponent.begin()) != mon.end()){
            logger.log(
                "Using previous known value to disambiguate: " + set_to_str(state.opponent),
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
    log_opponent_state(logger, state, "raw summary read");
    actions.close_summary();
    if (state.in_boss_battle_with_known_boss()){
        if (!state.opponent.empty() && *state.opponent.begin() != state.boss){
            logger.log(
                "Inconsistent Boss: Expected " + state.boss + ", Read: " + *state.opponent.begin(),
                COLOR_RED
            );
        }
        logger.log(std::format("[MaxLair][opponent] summary override: raw={}, using known_boss={}", set_to_str(state.opponent), state.boss));
        state.opponent = {state.boss};
    }
}

} // namespace

bool read_battle_opponent(Logger& logger, GlobalState& state, OpponentReadActions& actions){
    log_opponent_state(logger, state, "begin read");
    bool retry = !process_header_read(logger, state, actions.read_header());
    log_opponent_state(logger, state, "before header acceptance");
    const bool header_accepted = state.accept_opponent();
    logger.log(std::format("[MaxLair][opponent] header acceptance: accepted={}, header_unresolved={}", header_accepted, retry));
    log_opponent_state(logger, state, "after header acceptance");
    if (!header_accepted){
        retry = true;
    }
    if (retry){
        read_summary(logger, state, actions);
    }
    actions.wait_for_inputs();
    log_opponent_state(logger, state, retry ? "before final acceptance (summary path)" : "before final acceptance (header path)");
    if (!state.accept_opponent()){
        log_opponent_state(logger, state, "final acceptance rejected; requesting recovery");
        logger.log("Opponent does not match the battle stage after summary read.", COLOR_RED);
        actions.report_invalid();
        return false;
    }
    log_opponent_state(logger, state, "final acceptance succeeded");
    return true;
}

} // namespace MaxLairInternal
} // namespace PokemonSwSh
} // namespace NintendoSwitch
} // namespace PokemonAutomation

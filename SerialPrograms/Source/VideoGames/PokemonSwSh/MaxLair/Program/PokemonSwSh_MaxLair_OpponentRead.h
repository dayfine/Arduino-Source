/*  Max Lair Opponent Read
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
#ifndef PokemonAutomation_PokemonSwSh_MaxLair_OpponentRead_H
#define PokemonAutomation_PokemonSwSh_MaxLair_OpponentRead_H
#include <set>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "VideoGames/PokemonSwSh/MaxLair/Framework/PokemonSwSh_MaxLair_State.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{

// Screen reads and controller operations used to identify a battle's opponent.
class OpponentReadActions{
public:
    virtual ~OpponentReadActions() = default;
    // Candidates read from the battle-menu header.
    virtual std::set<std::string> read_header() = 0;
    // Queue opening the opponent's summary from the battle menu.
    virtual void open_summary() = 0;
    // Candidates read after the summary's opening inputs have drained.
    virtual std::set<std::string> read_summary() = 0;
    // Queue closing the summary back to the battle menu.
    virtual void close_summary() = 0;
    // Finish queued inputs before reading or returning to the caller.
    virtual void wait_for_inputs() = 0;
    // Capture diagnostics for an invalid read; no further battle inputs.
    virtual void report_invalid() = 0;
};

// From the battle menu, identify the opponent, falling back to its summary.
// Returns after draining inputs, including any summary-close inputs. False
// means recover instead of reading moves. Retains the existing known-boss override.
bool read_battle_opponent(Logger& logger, GlobalState& state, OpponentReadActions& actions);

} // namespace MaxLairInternal
} // namespace PokemonSwSh
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  BBQ Points Ledger
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonSV_BBQPointsLedger.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


void BBQPointsLedger::record(int read, WallClock when){
    if (read < 0){
        return;
    }
    if (starting_balance < 0){
        starting_balance = read;
        first_read = when;
    }else if (read > balance){
        earned += read - balance;
    }else{
        spent += balance - read;
    }
    balance = read;
    last_read = when;
}

int64_t BBQPointsLedger::net_per_hour() const{
    const int64_t span_ms = std::chrono::duration_cast<std::chrono::milliseconds>(last_read - first_read).count();
    if (starting_balance < 0 || span_ms <= 0){
        return 0;
    }
    return net() * 3600000 / span_ms;
}


}
}
}

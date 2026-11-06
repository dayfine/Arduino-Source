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
        //  The next read may include quest income, so this window can't be checked.
        m_pending_unchecked |= m_pending_spend > 0;
        return;
    }
    if (m_pending_spend > 0 && starting_balance >= 0 && !m_pending_unchecked){
        checked_windows++;
        const int change = read - balance;
        if (change != -m_pending_spend){
            mismatched_windows++;
            last_mismatch = "rerolls should have cost " + std::to_string(m_pending_spend)
                + " BP, but the balance went from " + std::to_string(balance)
                + " to " + std::to_string(read);
        }
    }
    m_pending_spend = 0;
    m_pending_unchecked = false;
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

void BBQPointsLedger::expect_spend(int bp){
    m_pending_spend += bp;
    expected_spent += bp;
}
void BBQPointsLedger::skip_spend_check(){
    m_pending_unchecked = true;
}

bool BBQPointsLedger::reconciled() const{
    return starting_balance >= 0
        && (int64_t)balance - starting_balance == net()
        && mismatched_windows == 0;
}
std::string BBQPointsLedger::reconciliation() const{
    if (starting_balance < 0){
        return "BP check: no balance was read.";
    }
    std::string str = "BP check: " + std::string(reconciled() ? "OK" : "MISMATCH")
        + ". Balance " + std::to_string(starting_balance) + " -> " + std::to_string(balance)
        + " (change " + std::to_string((int64_t)balance - starting_balance)
        + "), stats net " + std::to_string(net())
        + ". Rerolls expected to cost " + std::to_string(expected_spent)
        + " BP; " + std::to_string(checked_windows - mismatched_windows) + "/"
        + std::to_string(checked_windows) + " reroll windows matched the reads.";
    if (mismatched_windows > 0){
        str += " Last mismatch: " + last_mismatch + ".";
    }
    return str;
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

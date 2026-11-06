/*  BBQ Points Ledger
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQPointsLedger_H
#define PokemonAutomation_PokemonSV_BBQPointsLedger_H

#include <stdint.h>
#include <string>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  BP accounting from successive balance reads of the quest panel.
//  A rise between two reads is income (quest rewards) and a drop is an
//  expense (rerolls), so income and expense are only split correctly when
//  they don't fall between the same two reads. The net is always exact.
struct BBQPointsLedger{
    int starting_balance = -1;  //  -1 until the first successful read.
    int balance = -1;
    WallClock first_read;
    WallClock last_read;
    uint64_t earned = 0;
    uint64_t spent = 0;

    //  Record a balance read at "when". Negative reads (OCR failures) are ignored.
    void record(int read, WallClock when);

    //  The program paid "bp" for a reroll it believes went through. The next
    //  successful read must show exactly that drop, since the farmer reads the
    //  panel again before doing any quest. A mismatch means a reroll that didn't
    //  happen, an unexpected BP change or a misread balance.
    void expect_spend(int bp);
    //  A reroll whose cost is unknown (an unreadable slot): don't check this window.
    void skip_spend_check();

    uint64_t expected_spent = 0;    //  Sum of every expect_spend().
    uint64_t checked_windows = 0;
    uint64_t mismatched_windows = 0;
    std::string last_mismatch;

    //  End-of-run reconciliation, one line for the log: the balance change
    //  against the stats' net, and the reroll spends against the reads.
    bool reconciled() const;
    std::string reconciliation() const;

    int64_t net() const{
        return (int64_t)earned - (int64_t)spent;
    }

    //  Net BP per hour between the first and the latest read. 0 until two reads
    //  are at least a millisecond apart.
    int64_t net_per_hour() const;

private:
    int m_pending_spend = 0;
    bool m_pending_unchecked = false;
};


}
}
}
#endif

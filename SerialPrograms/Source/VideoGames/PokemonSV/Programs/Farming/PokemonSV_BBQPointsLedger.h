/*  BBQ Points Ledger
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQPointsLedger_H
#define PokemonAutomation_PokemonSV_BBQPointsLedger_H

#include <stdint.h>
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

    int64_t net() const{
        return (int64_t)earned - (int64_t)spent;
    }

    //  Net BP per hour between the first and the latest read. 0 until two reads
    //  are at least a millisecond apart.
    int64_t net_per_hour() const;
};


}
}
}
#endif

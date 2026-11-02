/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryStats_H
#define PokemonAutomation_Pokemon_AutoStoryStats_H

#include "CommonFramework/ProgramStats/StatsTracking.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Saved checkpoints, segments started and game resets of an AutoStory run.
//  The names are the keys of the saved stats; do not rename them.
struct AutoStoryStats : public StatsTracker{
    AutoStoryStats()
        : m_checkpoint(m_stats["Checkpoint"])
        , m_segment(m_stats["Segment"])
        , m_reset(m_stats["Reset"])
    {
        m_display_order.emplace_back("Checkpoint");
        m_display_order.emplace_back("Segment");
        m_display_order.emplace_back("Reset");
    }
    std::atomic<uint64_t>& m_checkpoint;
    std::atomic<uint64_t>& m_segment;
    std::atomic<uint64_t>& m_reset;
};


}
}
#endif

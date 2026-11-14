#ifndef PokemonAutomation_PokemonSV_IsolatedHuntStats_H
#define PokemonAutomation_PokemonSV_IsolatedHuntStats_H
#include "PokemonSV_LetsGoTools.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

class IsolatedHuntStats : public LetsGoEncounterBotStats{
public:
    IsolatedHuntStats()
        : passes(m_stats["Clearing Passes"]), refusals(m_stats["Refusals"])
        , refreshes(m_stats["Spawn Refreshes"]), sandwiches(m_stats["Sandwiches"])
        , hunt_seconds(m_stats["Hunt Seconds"])
    {
        m_display_order.emplace_back("Clearing Passes");
        m_display_order.emplace_back("Refusals");
        m_display_order.emplace_back("Spawn Refreshes");
        m_display_order.emplace_back("Sandwiches");
        m_display_order.emplace_back("Hunt Seconds");
    }
    std::atomic<uint64_t>& passes;
    std::atomic<uint64_t>& refusals;
    std::atomic<uint64_t>& refreshes;
    std::atomic<uint64_t>& sandwiches;
    std::atomic<uint64_t>& hunt_seconds;
};

}
}
}
#endif

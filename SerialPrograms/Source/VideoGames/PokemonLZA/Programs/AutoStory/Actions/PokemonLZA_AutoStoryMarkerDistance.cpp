/*  PokemonLZA AutoStory - Marker distance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

bool near(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

constexpr size_t AGREEING_UNTAKEN = 2;
constexpr size_t UNTAKEN_KEPT = 3;

} // namespace


bool MarkerDistance::read(int distance){
    if (m_floor && distance < *m_floor){
        return false;
    }
    if (!m_last || !near(distance, *m_last)){
        size_t agreeing = 0;
        for (int untaken : m_untaken){
            if (near(distance, untaken)){
                agreeing++;
            }
        }
        if (agreeing < AGREEING_UNTAKEN){
            if (m_untaken.size() == UNTAKEN_KEPT){
                m_untaken.erase(m_untaken.begin());
            }
            m_untaken.push_back(distance);
            return false;
        }
    }
    m_untaken.clear();
    m_last = distance;
    return true;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

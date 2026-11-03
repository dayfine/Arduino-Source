/*  AutoStory Pace Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <utility>
#include "PokemonFRLG_AutoStoryPaceSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;


SimPatch::SimPatch(OverworldSim& sim, Direction pace, Direction back, std::function<void()> back_to_patch)
    : m_sim(sim)
    , m_pace(pace)
    , m_back(back)
    , m_back_to_patch(std::move(back_to_patch))
{}

bool SimPatch::step(){
    const Direction way = m_displaced ? m_back : m_pace;
    if (m_sim.state().facing != way){
        m_sim.tap(way);
    }
    m_sim.tap(way);
    m_displaced = !m_displaced;
    return m_sim.watch({SimWatcher::FLASH}, std::chrono::milliseconds(1000)).has_value();
}

bool SimPatch::battle_shows(){
    return m_sim.watch({SimWatcher::BATTLE_SCREEN}, std::chrono::milliseconds(5000)).has_value();
}

bool SimPatch::overworld_shows(){
    return m_sim.state().screen == SimScreen::OVERWORLD;
}

bool SimPatch::message_shows(){
    return m_sim.state().screen == SimScreen::MESSAGE;
}

std::string SimPatch::fight_trainer(){
    if (!m_sim.close_message() || m_sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
        return "no trainer's battle";
    }
    if (m_back_to_patch){
        m_back_to_patch();
        m_displaced = false;
    }
    return m_sim.state().trainers_beaten.back();
}

void SimPatch::dump_screen(std::string_view){}



}
}
}

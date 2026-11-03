/*  AutoStory Foe Tracker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryFoeTracker.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Looks in a row with the foe's box gone and the lead's there: a faint,
//  not a frame between two animations (as for the lead's, FAINT_POLLS in
//  PokemonFRLG_AutoStoryBattle.cpp).
constexpr int FOE_GONE_POLLS = 3;

}


TrainerFoeTracker::TrainerFoeTracker(std::string_view trainer, const PartyMember& lead)
    : m_lead(own_battler(lead))
{
    const std::optional<SimTrainer> sim = trainer.empty() ? std::nullopt : sim_trainer(trainer);
    if (!sim){
        return;
    }
    m_party = sim->party;
    for (const SimBattler& battler : m_party){
        m_species.push_back(battler.species);
    }
}

void TrainerFoeTracker::boxes_seen(BattleBoxes boxes){
    if (m_party.empty() || boxes.foe || !boxes.lead){
        m_gone_polls = boxes.foe ? 0 : m_gone_polls;
        return;
    }
    if (++m_gone_polls == FOE_GONE_POLLS && !m_next_due){
        m_fainted.insert(m_out);
        m_party[m_out].hp = 0;
        m_next_due = true;
    }
}

void TrainerFoeTracker::at_menu(std::string_view read){
    if (m_party.empty()){
        return;
    }
    if (m_next_due && m_lead){
        m_out = ai_next_foe(m_party, m_out, *m_lead);
    }
    m_next_due = false;
    if (read.empty() || m_species[m_out] == read){
        return;
    }
    //  The name says otherwise: the first of that species not fainted.
    for (size_t slot = 0; slot < m_party.size(); slot++){
        if (m_species[slot] == read && !m_fainted.contains(slot)){
            m_out = slot;
            return;
        }
    }
}

std::optional<size_t> TrainerFoeTracker::out() const{
    return m_party.empty() ? std::nullopt : std::optional<size_t>(m_out);
}



}
}
}

/*  AutoStory Battle Decisions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <iterator>
#include "PokemonFRLG_AutoStoryBattleDecisions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr size_t PARTY_SLOTS = 6;
//  A wild Pokemon is fought (WILD_FIGHT) only with green HP and this many
//  Potions left; otherwise it is run from.
constexpr int MIN_POTIONS_TO_FIGHT = 2;
//  RUN this many times ("Can't escape!") before fighting instead.
constexpr int RUN_TRIES = 4;
//  PSN read at this many turns in a row before an Antidote.
constexpr int PSN_READS = 2;
//  An evolution screen held this long was an evolution, not a text box
//  between two others.
constexpr Milliseconds EVOLUTION_SEEN = std::chrono::milliseconds(5000);
//  Nothing pressed for at most this long in all in a battle: an evolution
//  and the next member's, with room to spare.
constexpr Milliseconds EVOLUTION_HOLDS = std::chrono::milliseconds(45000);
//  No battle menu, HP box or YES/NO for this long: START, to see whether
//  the overworld is back. Longer than a turn's text between two HP boxes.
constexpr Milliseconds BATTLE_GONE = std::chrono::milliseconds(3000);
//  START at most this often: each look takes over a second.
constexpr Milliseconds OVERWORLD_CHECK = std::chrono::milliseconds(3000);
//  A and B pressed blind for this long with no battle menu: give up.
constexpr Milliseconds BLIND_PRESSES = std::chrono::milliseconds(30000);
//  Turns a move turned down is left alone: the longest Disable (pret
//  battle_script_commands.c Cmd_disablelastusedattack, 2 to 5 turns).
constexpr int DISABLE_TURNS = 5;
//  The battle menu back this soon after a move was chosen, with the move
//  list between: the move wasn't used.
constexpr Milliseconds REFUSED_WITHIN = std::chrono::milliseconds(7000);
//  Below this much PP left in the lead's attacks, the nurse before a fight
//  that needs them: about a trainer's worth of turns.
constexpr size_t LOW_ATTACK_PP = 10;

}


void healed_by_nurse(Supplies& supplies){
    supplies.hurt = false;
    supplies.hp_red = false;
    supplies.status = StatusCondition::NONE;
    restore_pp(supplies.lead);
}

bool attacks_low_on_pp(const Supplies& supplies){
    return attack_slot(supplies.lead).has_value() && attack_pp(supplies.lead) < LOW_ATTACK_PP;
}


BattleDecisions::BattleDecisions(Supplies& supplies, BattleKind kind)
    : m_supplies(supplies)
    , m_kind(kind)
{}

TurnAction BattleDecisions::turn(const PartyStatus& status){
    m_turns++;
    count_down_refused();
    m_last_move_slot.reset();
    if (!m_lost){
        return lead_turn(status);
    }
    //  The next member is out: it runs from a wild Pokemon for as long as
    //  it takes, and against a trainer uses its first move.
    if (m_kind != BattleKind::TRAINER){
        return TurnAction{.choice = TurnChoice::RUN};
    }
    return TurnAction{.choice = TurnChoice::MOVE, .slot = TOP_LEFT_MOVE};
}

TurnAction BattleDecisions::lead_turn(const PartyStatus& status){
    const bool hurt = status.hp_red || status.hp_yellow;
    m_supplies.hurt = hurt;
    m_supplies.status = status.status;
    m_psn_reads = status.status == StatusCondition::POISON ? m_psn_reads + 1 : 0;
    //  A wild Pokemon is fought for EXP only at full strength: fighting
    //  every one in Viridian Forest used up all 4 Potions and then the
    //  starter (console B, 2026-11-01). With a status, it is run from too.
    const bool weak = hurt || status.status != StatusCondition::NONE || m_supplies.potions < MIN_POTIONS_TO_FIGHT;
    const bool run = m_kind == BattleKind::WILD_RUN || (m_kind == BattleKind::WILD_FIGHT && weak);
    if (run && m_run_tries < RUN_TRIES){
        m_run_tries++;
        return TurnAction{.choice = TurnChoice::RUN};
    }
    if (!m_items_off && hurt && m_supplies.potions > 0){
        return TurnAction{.choice = TurnChoice::POTION};
    }
    if (!m_items_off && m_psn_reads >= PSN_READS && !m_antidote_used && m_supplies.antidotes > 0){
        m_antidote_used = true;
        return TurnAction{.choice = TurnChoice::ANTIDOTE};
    }
    return move_turn();
}

//  The plan's attack with PP left, or against known foes (a trainer's
//  party, the wild Pokemon read), the one that does them most damage. While the story doesn't know
//  all four moves, the bottom left one (Bubble from Lv7), which the cursor
//  can't reach while that slot is empty, so before Bubble it stays on
//  Tackle.
TurnAction BattleDecisions::move_turn(){
    PartyMember usable = m_supplies.lead;
    for (const auto& refused : m_refused){
        if (refused.first < usable.moves.size()){
            usable.moves[refused.first].pp = 0;
        }
    }
    const std::optional<size_t> slot = attack_slot_against(usable, m_kind == BattleKind::TRAINER ? m_supplies.foes : m_supplies.wild_foes);
    if (!slot){
        return TurnAction{.choice = TurnChoice::MOVE, .slot = BOTTOM_LEFT_MOVE};
    }
    spend_pp(m_supplies.lead, *slot);
    m_last_move_slot = slot;
    return TurnAction{.choice = TurnChoice::MOVE, .slot = *slot};
}

void BattleDecisions::count_down_refused(){
    for (auto it = m_refused.begin(); it != m_refused.end();){
        it = --it->second <= 0 ? m_refused.erase(it) : std::next(it);
    }
}

void BattleDecisions::move_refused(){
    if (!m_last_move_slot){
        return;
    }
    const size_t slot = *m_last_move_slot;
    if (slot < m_supplies.lead.moves.size()){
        m_supplies.lead.moves[slot].pp++;
    }
    //  One more: the next turn() counts down the one that didn't happen.
    m_refused[slot] = DISABLE_TURNS + 1;
    m_last_move_slot.reset();
}

void BattleDecisions::turn_done(TurnChoice choice, TurnResult result){
    if (result == TurnResult::REFUSED){
        move_refused();
        return;
    }
    const bool done = result == TurnResult::DONE;
    if (!done && choice == TurnChoice::MOVE){
        if (m_last_move_slot){
            no_pp_left(m_supplies.lead, *m_last_move_slot);
        }
        return;
    }
    if (!done){
        m_items_off = true;
        return;
    }
    switch (choice){
    case TurnChoice::POTION:
        m_supplies.potions--;
        m_supplies.hurt = false;
        m_supplies.hp_red = false;
        return;
    case TurnChoice::ANTIDOTE:
        m_supplies.antidotes--;
        return;
    case TurnChoice::RUN:
    case TurnChoice::MOVE:
        return;
    }
}

PromptAnswer BattleDecisions::prompt(bool lead_box_gone){
    if (lead_box_gone){
        lead_fainted();
        //  NO tries to run; if that fails, the party list opens.
        return PromptAnswer{.answer = m_kind == BattleKind::TRAINER ? YesNo::YES : YesNo::NO};
    }
    if (m_declined_delete){
        //  "Stop learning <move>?" YES; NO would ask "Delete a move?" again.
        m_declined_delete = false;
        return PromptAnswer{.answer = YesNo::YES};
    }
    PartyMember learning = m_supplies.lead;
    const LearnDecision decision = learn_move(learning);
    if (decision.forget_slot){
        return PromptAnswer{.answer = YesNo::YES, .forget_slot = decision.forget_slot, .learned = std::move(learning)};
    }
    m_supplies.lead = std::move(learning);
    m_declined_delete = true;
    return PromptAnswer{.answer = YesNo::NO};
}

std::optional<size_t> BattleDecisions::next_member_slot(){
    lead_fainted();
    //  A slot sent before and still offered was empty or fainted: the next.
    for (size_t slot = 1; slot < PARTY_SLOTS; slot++){
        const bool kept = std::ranges::find(m_supplies.protected_slots, slot) != m_supplies.protected_slots.end();
        if (!kept && !m_sent.contains(slot)){
            m_sent.insert(slot);
            return slot;
        }
    }
    return std::nullopt;
}

void BattleDecisions::lead_fainted(){
    m_lost = true;
}

EvolutionHold::EvolutionHold(PartyMember& lead)
    : m_lead(lead)
{}

void EvolutionHold::turn_taken(TurnChoice choice){
    m_attacked = choice == TurnChoice::MOVE;
}

bool EvolutionHold::holds(const BattleTextScreen& screen) const{
    return m_attacked && m_held < EVOLUTION_HOLDS && screen.text_box && !screen.hp_box && !screen.arrow && !screen.yes_no;
}

void EvolutionHold::held(Milliseconds held, bool lead_fainted){
    m_held += held;
    //  Only the lead fights, so only it evolves; once a battle.
    if (held < EVOLUTION_SEEN || m_evolved || lead_fainted){
        return;
    }
    m_evolved = true;
    evolve(m_lead);
}

void BattleEndWatch::seen(BattleSeen what, WallClock now){
    if (what != BattleSeen::NONE){
        m_last_battle = now;
    }
    if (what == BattleSeen::MENU){
        m_blind_since.reset();
    }
}

bool BattleEndWatch::check_overworld(WallClock now){
    if (!m_last_battle || now - *m_last_battle < BATTLE_GONE){
        return false;
    }
    if (m_last_check && now - *m_last_check < OVERWORLD_CHECK){
        return false;
    }
    m_last_check = now;
    return true;
}

void MoveRefusalWatch::move_chosen(WallClock now){
    m_chosen = now;
    m_list_back = false;
}

void MoveRefusalWatch::no_battle_screen(){
    m_list_back = m_chosen.has_value();
}

bool MoveRefusalWatch::refused(WallClock now){
    const bool refused = m_chosen && m_list_back && now - *m_chosen < REFUSED_WITHIN;
    m_chosen.reset();
    m_list_back = false;
    return refused;
}

bool BattleEndWatch::pressed_blind(WallClock now){
    if (!m_blind_since){
        m_blind_since = now;
    }
    return now - *m_blind_since >= BLIND_PRESSES;
}



}
}
}

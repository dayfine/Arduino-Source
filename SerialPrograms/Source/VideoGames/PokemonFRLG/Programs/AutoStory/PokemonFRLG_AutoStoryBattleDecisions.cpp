/*  AutoStory Battle Decisions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
#include <string>
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryBattleSim.h"

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
//  The move prompts a turn can bring: two moves offered, NO then YES each.
constexpr int MOVE_PROMPTS = 4;

//  The trainer's Pokemon out, at full HP (its damage taken isn't read),
//  when the battle loop knows which.
std::optional<SimBattler> foe_out(const Supplies& supplies){
    if (supplies.trainer.empty() || !supplies.foe_out){
        return std::nullopt;
    }
    const std::vector<TrainerMon> party = trainer_mons(supplies.trainer);
    return *supplies.foe_out < party.size() ? trainer_battler(party[*supplies.foe_out]) : std::nullopt;
}

//  A trainer's battle: against the foe out, the best expected damage at
//  BattleRisk::LOW, the sim's planned move at HIGH (its items taken as not
//  used yet); else, or while the story doesn't know all the lead's
//  moves, attack_slot_against() the whole party.
std::optional<size_t> trainer_attack_slot(const PartyMember& usable, const Supplies& supplies){
    const std::optional<SimBattler> foe = foe_out(supplies);
    const std::optional<SimBattler> own = own_battler(usable);
    std::optional<size_t> slot;
    if (foe && own && attack_slot(usable)){
        const std::optional<TrainerData> trainer = trainer_data(supplies.trainer);
        slot = supplies.risk == BattleRisk::HIGH && trainer
            ? planned_attack_slot(*own, *foe, trainer->items, trainer->party.size())
            : best_damage_slot(*own, *foe);
    }
    return slot ? slot : attack_slot_against(usable, supplies.foes);
}

//  The item a turn uses, if any, and why one was or wasn't, for the log.
struct ItemTurn{
    TurnChoice choice = TurnChoice::MOVE;
    std::string reason = {};
};

const char* bar_name(const PartyStatus& status){
    return status.hp_red ? "red" : status.hp_yellow ? "yellow" : "green";
}

//  Against a foe not known (`seen_hit` the HP its last hit took, 0 if not
//  seen): a Potion whenever the bar isn't green, unless the last hit took
//  a Potion's worth or more; asleep, an Awakening unless the bar is red.
ItemTurn unknown_foe_items(const Supplies& supplies, const PartyStatus& status, double seen_hit, int awakenings){
    const bool hurt = status.hp_red || status.hp_yellow;
    const bool asleep = status.status == StatusCondition::SLEEP;
    if (hurt && supplies.potions > 0 && seen_hit < POTION_HP){
        return {TurnChoice::POTION, std::format("a Potion: HP {}, the foe not known, its last hit {:.0f} HP", bar_name(status), seen_hit)};
    }
    if (asleep && awakenings > 0 && !status.hp_red){
        return {TurnChoice::AWAKENING, std::format("an Awakening: asleep at HP {}, the foe not known", bar_name(status))};
    }
    if (!hurt){
        return {};
    }
    return {TurnChoice::MOVE, supplies.potions <= 0
        ? std::string("no Potion: none left")
        : std::format("no Potion: the foe not known, its last hit {:.0f} HP would undo one", seen_hit)
    };
}

//  Why no Potion against a known foe, at `c`.
std::string no_potion_reason(const Supplies& supplies, const PartyStatus& status, const PotionCase& c){
    if (supplies.potions <= 0){
        return "none left";
    }
    const bool asked = supplies.risk == BattleRisk::HIGH ? status.hp_red : status.hp_red || status.hp_yellow;
    if (!asked){
        return std::format("HP {} and the next hit can't faint the lead", bar_name(status));
    }
    return std::format(
        "the next turn ({:.0f}% of the time {} HP or less) undoes it and it outlasts the foe no longer",
        100 * POTION_COVER_PERCENTILE, c.likely_hit
    );
}

//  The foe's move chances, for the log: "vine-whip 42%, sleep-powder 42%".
std::string move_chances_text(const PotionCase& c){
    std::string text;
    for (const auto& [move, chance] : c.move_chances){
        if (chance > 0){
            text += std::format("{}{} {:.0f}%", text.empty() ? "" : ", ", move, 100 * chance);
        }
    }
    return text.empty() ? "none" : text;
}

//  Against a known foe: a Potion at BattleRisk::HIGH when its next turn
//  could faint the lead (potion_now(), the HP from the bar's share) or the
//  bar is red, at LOW whenever the bar isn't green; either only if
//  potion_pays(). Else, asleep, an Awakening if awakening_now().
ItemTurn known_foe_items(
    const Supplies& supplies, const PartyStatus& status, int awakenings,
    const SimBattler& own, const PotionCase& c
){
    const bool hurt = status.hp_red || status.hp_yellow;
    const std::string numbers = std::format(
        "{}/{} HP, the foe's next turn {} HP or less {:.0f}% of the time (mean {:.0f}, up to {}; {}), {} of them before the lead's attacks faint it",
        c.hp, c.max_hp, c.likely_hit, 100 * POTION_COVER_PERCENTILE, c.expected_hit, c.max_hit, move_chances_text(c), c.hits_to_ko
    );
    if (awakening_now(c, own, awakenings)){
        return {TurnChoice::AWAKENING, "an Awakening: asleep, and the lead outlasts the turn it costs; " + numbers};
    }
    const bool high = supplies.risk == BattleRisk::HIGH;
    if (high && potion_now(c, supplies.potions)){
        return {TurnChoice::POTION, "a Potion: the next turn could faint the lead, and it covers that turn; " + numbers};
    }
    if (supplies.potions > 0 && (high ? status.hp_red : hurt) && potion_pays(c)){
        return {TurnChoice::POTION, "a Potion: it covers the next turn, or outlasts the foe; " + numbers};
    }
    if (!hurt && own.status != SimStatus::SLEEP){
        return {};
    }
    return {TurnChoice::MOVE, "no Potion: " + no_potion_reason(supplies, status, c) + "; " + numbers};
}

//  The item this turn, from the bar and the badge as read.
ItemTurn item_turn(const Supplies& supplies, const PartyStatus& status, double seen_hit, int awakenings){
    const std::optional<SimBattler> foe = foe_out(supplies);
    std::optional<SimBattler> own = own_battler(supplies.lead, status.hp_seen ? status.hp_fill : 1.0);
    const std::optional<SimTrainer> trainer = sim_trainer(supplies.trainer);
    if (!foe || !own || !trainer){
        return unknown_foe_items(supplies, status, seen_hit, awakenings);
    }
    own->status = status.status == StatusCondition::SLEEP ? SimStatus::SLEEP : SimStatus::NONE;
    return known_foe_items(supplies, status, awakenings, *own, potion_case(*own, *foe, *trainer, seen_hit));
}

//  The lead's HP from the bar's share, if it showed.
std::optional<double> lead_hp(const Supplies& supplies, const PartyStatus& status){
    const std::optional<SimBattler> own = own_battler(supplies.lead);
    if (!status.hp_seen || !own){
        return std::nullopt;
    }
    return status.hp_fill * own->body.stats.hp;
}

}


void healed_by_nurse(Supplies& supplies){
    supplies.hurt = false;
    supplies.hp_red = false;
    supplies.hp_fill = 1.0;
    supplies.status = StatusCondition::NONE;
    restore_pp(supplies.lead);
}

bool attacks_low_on_pp(const Supplies& supplies){
    return attack_slot(supplies.lead).has_value() && attack_pp(supplies.lead) < LOW_ATTACK_PP;
}

bool nurse_before_battle(const Supplies& supplies){
    return supplies.hurt || supplies.status != StatusCondition::NONE || attacks_low_on_pp(supplies);
}


BattleDecisions::BattleDecisions(Supplies& supplies, BattleKind kind)
    : m_supplies(supplies)
    , m_kind(kind)
{}

TurnAction BattleDecisions::turn(const PartyStatus& status){
    m_turns++;
    m_move_prompts = 0;
    m_before_forget.reset();
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
    if (status.hp_seen){
        m_supplies.hp_fill = status.hp_fill;
    }
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
    see_hit(status);
    //  Awakenings are kept for trainers: a wild battle with a status is run from.
    const int awakenings = m_kind == BattleKind::TRAINER ? m_supplies.awakenings : 0;
    const ItemTurn items = m_items_off ? ItemTurn{} : item_turn(m_supplies, status, m_seen_hit, awakenings);
    if (items.choice != TurnChoice::MOVE){
        return TurnAction{.choice = items.choice, .heal_reason = items.reason};
    }
    if (!m_items_off && m_psn_reads >= PSN_READS && !m_antidote_used && m_supplies.antidotes > 0){
        m_antidote_used = true;
        return TurnAction{.choice = TurnChoice::ANTIDOTE, .heal_reason = items.reason};
    }
    TurnAction action = move_turn();
    action.heal_reason = items.reason;
    return action;
}

void BattleDecisions::see_hit(const PartyStatus& status){
    if (m_supplies.foe_out != m_hit_by){
        m_seen_hit = 0;
        m_hit_by = m_supplies.foe_out;
    }
    const std::optional<double> hp = lead_hp(m_supplies, status);
    if (hp && m_last_hp){
        const double taken = *m_last_hp + (m_healed ? POTION_HP : 0) - *hp;
        if (taken > 0){
            m_seen_hit = taken;
        }
    }
    m_last_hp = hp;
    m_healed = false;
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
    const std::optional<size_t> slot = m_kind == BattleKind::TRAINER
        ? trainer_attack_slot(usable, m_supplies)
        : attack_slot_against(usable, m_supplies.wild_foes);
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
        m_healed = true;
        m_supplies.potions--;
        m_supplies.hurt = false;
        m_supplies.hp_red = false;
        return;
    case TurnChoice::ANTIDOTE:
        m_supplies.antidotes--;
        return;
    case TurnChoice::AWAKENING:
        m_supplies.awakenings--;
        m_supplies.status = StatusCondition::NONE;
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
    m_move_prompts++;
    if (m_before_forget){
        //  A YES/NO right after the move list: it closed with no move
        //  forgotten, and this is "Stop learning <move>?" (console B,
        //  2026-11-03, Miguel's battle). The lead keeps its moves.
        m_supplies.lead = std::move(*m_before_forget);
        m_before_forget.reset();
        return PromptAnswer{.answer = YesNo::YES};
    }
    if (m_move_prompts > MOVE_PROMPTS){
        //  More than the move prompts can be: YES ends either one, as
        //  "Delete a move?" opens the move list, which the loop's B closes.
        return PromptAnswer{.answer = YesNo::YES};
    }
    if (m_declined_delete){
        //  "Stop learning <move>?" YES; NO would ask "Delete a move?" again.
        m_declined_delete = false;
        return PromptAnswer{.answer = YesNo::YES};
    }
    PartyMember learning = m_supplies.lead;
    const LearnDecision decision = learn_move(learning);
    if (decision.forget_slot){
        m_before_forget = m_supplies.lead;
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

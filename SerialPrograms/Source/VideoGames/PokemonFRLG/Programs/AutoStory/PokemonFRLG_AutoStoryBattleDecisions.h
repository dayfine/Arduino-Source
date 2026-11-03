/*  AutoStory Battle Decisions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story's battle loop chooses at each screen, apart from reading
 *  the screens and pressing the buttons, so the choices run in tests:
 *  each turn's action, the YES/NO prompts (a move to learn, "Use next
 *  POKEMON?"), when not to press B for an evolution, and the lead's PP.
 *
 *  The party is the lead (the starter) and members that don't fight: the
 *  lead is never switched out. If it faints, a wild battle is run from
 *  ("Use next POKEMON?" NO); a trainer's, or a failed run, sends the next
 *  member not in Supplies::protected_slots, which only runs or uses its
 *  first move; with only protected ones left, the program stops instead.
 *  The battle counts as lost either way.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleDecisions_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleDecisions_H

#include <cstddef>
#include <optional>
#include <set>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class TurnChoice{
    RUN,
    POTION,
    ANTIDOTE,
    MOVE,
};

struct TurnAction{
    TurnChoice choice = TurnChoice::MOVE;
    //  For MOVE, the move list's slot.
    size_t slot = BOTTOM_LEFT_MOVE;
};

enum class YesNo{
    YES,
    NO,
};

//  A battle screen as the loop reads it, for the evolution check.
struct BattleTextScreen{
    //  The battle's text box.
    bool text_box = false;
    //  The battle menu or an HP box.
    bool hp_box = false;
    //  The text waits for a button.
    bool arrow = false;
    //  A YES/NO box.
    bool yes_no = false;
};

struct PromptAnswer{
    YesNo answer = YesNo::NO;
    //  After YES to "Delete a move?": the slot to forget on the move list
    //  that opens, its cursor on the top move.
    std::optional<size_t> forget_slot = {};
};


class BattleDecisions{
public:
    BattleDecisions(Supplies& supplies, BattleKind kind);

    //  At the battle menu, with the player's box as read.
    TurnAction turn(const PartyStatus& status);
    //  What turn() chose was done, or not: an item not used (another item
    //  in its row), or a move the game said has no PP left.
    void turn_done(TurnChoice choice, bool done);

    //  A YES/NO in or after the battle. With the lead's box gone and the
    //  opponent's there, it is "Use next POKEMON?"; else the move prompts:
    //  "Delete a move to make room?", then "Stop learning?" after a NO.
    PromptAnswer prompt(bool lead_box_gone);

    //  The party list opened with the lead fainted: the slot to send in,
    //  or none if every member left is protected.
    std::optional<size_t> next_member_slot();

    //  The lead's box gone for good: it fainted.
    void lead_fainted();
    bool lost() const{ return m_lost; }

    size_t turns() const{ return m_turns; }

private:
    TurnAction lead_turn(const PartyStatus& status);
    TurnAction move_turn();

    Supplies& m_supplies;
    const BattleKind m_kind;
    size_t m_turns = 0;
    int m_run_tries = 0;
    int m_psn_reads = 0;
    bool m_antidote_used = false;
    //  An item read as another in the bag: none is used again this battle.
    bool m_items_off = false;
    bool m_lost = false;
    //  NO was answered to "Delete a move?": "Stop learning?" comes next.
    bool m_declined_delete = false;
    std::optional<size_t> m_last_move_slot;
    //  The members sent in after the lead fainted.
    std::set<size_t> m_sent;
};


//  No B while an evolution may be on screen: any B cancels it (pret
//  evolution_scene.c; console B, 2026-11-02: Squirtle stayed Lv16 after the
//  battle loop's B presses).
class EvolutionHold{
public:
    explicit EvolutionHold(PartyMember& lead);

    //  The loop took a turn: only one that attacked can end the battle in a
    //  win, and so in an evolution.
    void turn_taken(TurnChoice choice);
    //  Whether `screen` may be an evolution: after a turn that attacked (not
    //  after RUN: console B, 2026-11-03, a minute without B after "Got away
    //  safely!"), the text box with no HP box and no arrow or YES/NO, as
    //  "What? <Pokemon> is evolving!" shows through the animation. Nothing
    //  is pressed while it shows, for at most EVOLUTION_HOLDS in all in a
    //  battle, in case it is something else.
    bool holds(const BattleTextScreen& screen) const;
    //  Such a screen showed for `held` with nothing pressed: long enough,
    //  and the lead evolved (unless it had fainted).
    void held(Milliseconds held, bool lead_fainted);

private:
    PartyMember& m_lead;
    bool m_attacked = false;
    bool m_evolved = false;
    Milliseconds m_held = Milliseconds::zero();
};


//  What the battle loop saw of the battle at a poll.
enum class BattleSeen{
    NONE,
    BATTLE,     //  as battle_shows() reads it
    MENU,       //  the battle menu
};

//  When the battle loop presses START to see whether the overworld is
//  back, and when it gives up pressing A and B for a screen nothing reads.
//  The battle counts as on screen by its menu, an HP box or a YES/NO only:
//  its text box alone also reads on the black below a dark cave's corridor
//  (console B, 2026-11-03, Mt. Moon B2F: after "Got away safely!" the loop
//  took the cave for the battle and pressed A and B for 9 minutes).
class BattleEndWatch{
public:
    //  What the loop read at `now`.
    void seen(BattleSeen what, WallClock now);
    //  Whether to press START at `now`: a battle was seen, and none since
    //  for BATTLE_GONE. At most every OVERWORLD_CHECK.
    bool check_overworld(WallClock now);
    //  A or B was pressed blind at `now`. True once that has gone on for
    //  BLIND_PRESSES with no battle menu: the loop stops, and the walker,
    //  which finds the player again, goes on.
    bool pressed_blind(WallClock now);

private:
    std::optional<WallClock> m_last_battle;
    std::optional<WallClock> m_last_check;
    std::optional<WallClock> m_blind_since;
};


void add_tests_AutoStoryParty(UnitTestDatabase& database);



}
}
}
#endif

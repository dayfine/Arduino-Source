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
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  High-level action chosen for a battle turn.
enum class TurnChoice{
    RUN,
    POTION,
    ANTIDOTE,
    AWAKENING,
    MOVE,
};

//  Decided battle action including slot or item index.
struct TurnAction{
    TurnChoice choice = TurnChoice::MOVE;
    //  For MOVE, the move list's slot.
    size_t slot = BOTTOM_LEFT_MOVE;
    //  Why a Potion was or wasn't used, for the log; empty when none was
    //  weighed (green HP, awake).
    std::string heal_reason = {};
};

//  What came of a TurnAction.
enum class TurnResult{
    DONE,
    //  An item not used (another item in its row), or a move the game said
    //  has no PP left. The first time for a move it is taken as turned
    //  down (REFUSED): Kadabra's Disable, or a misread (console B,
    //  2026-11-03, m68: "no PP left" read after the battle's last turn); the
    //  second time its PP is 0.
    NOT_DONE,
    //  The game turned the move down although it has PP left: a foe's
    //  Disable (console B, 2026-11-03, Mt. Moon: Miguel's Grimer disabled
    //  Water Gun, and the loop chose it 17 more times while no turn passed).
    //  Its PP comes back, and it isn't chosen for as long as Disable can
    //  last (pret: 2 to 5 turns).
    REFUSED,
};

//  Binary prompt decision.
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
    //  The overworld of a cave dark without Flash (dark_cave_screen()),
    //  whose black reads as the text box.
    bool dark_cave = false;
};

//  Answer to an in-battle prompt dialogue.
struct PromptAnswer{
    YesNo answer = YesNo::NO;
    //  After YES to "Delete a move?": the slot to forget on the move list
    //  that opens, its cursor on the top move.
    std::optional<size_t> forget_slot = {};
    //  With forget_slot: the lead once the move is learned, to keep only if
    //  the move list opened (console B, 2026-11-03: a YES/NO read where no
    //  move list followed was taken as Bite over Tail Whip, and the loop
    //  later chose Tail Whip for Bite).
    std::optional<PartyMember> learned = {};
};


//  State machine deciding turn choices and prompt answers in battles.
class BattleDecisions{
public:
    BattleDecisions(Supplies& supplies, BattleKind kind);

    //  At the battle menu, with the player's box as read.
    TurnAction turn(const PartyStatus& status);
    //  What came of what turn() chose.
    void turn_done(TurnChoice choice, TurnResult result);

    //  A YES/NO in or after the battle. With the lead's box gone and the
    //  opponent's there, it is "Use next POKEMON?"; else the move prompts:
    //  "Delete a move to make room?", then "Stop learning?" after a NO (in
    //  the same turn), or after YES and a move list closed with nothing
    //  forgotten. Past MOVE_PROMPTS since the last turn, a plain YES, which
    //  ends them either way (console B, 2026-11-03: NO and YES out of step
    //  with the game went round for 75 s). With no move the lead can have
    //  reached to offer (none left, or one more than MAX_LEVELS_IN_BATTLE
    //  above its level read before the battle), it is no move prompt, such
    //  as the trainer's "Will <player> change POKEMON?": NO, and the lead
    //  is left as it was.
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
    //  The HP the lead lost since the last turn, from the bar.
    void see_hit(const PartyStatus& status);
    void count_down_refused();
    void move_refused();
    //  The chance the foe's last turn seeded the lead taken in.
    void see_seed_chance();

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
    //  The move prompts since the last turn.
    int m_move_prompts = 0;
    //  YES was answered to "Delete a move?": the lead as it was before.
    //  A YES/NO before the next turn means nothing was forgotten.
    std::optional<PartyMember> m_before_forget;
    std::optional<size_t> m_last_move_slot;
    //  The lead's HP at the last turn, from the bar, and whether a Potion
    //  was used on it since.
    std::optional<double> m_last_hp;
    bool m_healed = false;
    //  The HP the last hit seen took, from the foe out `m_hit_by`; 0 if none
    //  was seen.
    double m_seen_hit = 0;
    std::optional<size_t> m_hit_by;
    //  The slots the game turned down, and the turns until they are tried again.
    std::map<size_t, int> m_refused;
    //  The slots "no PP left" was read for once.
    std::set<size_t> m_no_pp_read;
    //  The highest level a move offered can be for, from the lead's level
    //  read before the battle; none if it wasn't read.
    std::optional<uint8_t> m_level_cap;
    //  The chance the lead isn't seeded, from the foes' Leech Seed chances
    //  at each turn (the game doesn't show it), and the chance at the turn
    //  just chosen, taken in at the next one.
    double m_unseeded = 1.0;
    double m_seed_pending = 0;
    //  The members sent in after the lead fainted.
    std::set<size_t> m_sent;
};


//  No B while an evolution may be on screen: any B cancels it (pret
//  evolution_scene.c; console B, 2026-11-02: Squirtle stayed Lv16 after the
//  battle loop's B presses).
class EvolutionHold{
public:
    //  The longest one hold lasts: as long as exit_wild_battle()
    //  (PokemonFRLG_Navigation.cpp) waits for an evolution.
    static constexpr Milliseconds MAX_HOLD = std::chrono::milliseconds(20000);

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
    //  True once START has looked for the overworld and no battle has shown
    //  for NO_OVERWORLD: neither is coming back (console B, 2026-11-03,
    //  after losing to Lt. Surge: 8 minutes of + and B on a grey screen).
    bool overworld_lost(WallClock now) const;

private:
    std::optional<WallClock> m_last_battle;
    std::optional<WallClock> m_last_check;
    std::optional<WallClock> m_blind_since;
};


//  Whether the game turned down the move chosen, at the battle menu after
//  it: the move list came back (no battle screen reads on it) and the menu
//  followed sooner than any turn (console B, 2026-11-03: 5.7 s from the
//  choice for a disabled move, 8.3 s and up for a turn).
class MoveRefusalWatch{
public:
    void move_chosen(WallClock now);
    //  Whether a "no PP left" read at `now` counts: no later than a
    //  refusal's menu would come (console B, 2026-11-03, m68: one read 37 s
    //  after the battle's last move, as the battle ended).
    bool no_pp_due(WallClock now) const;
    //  No battle screen read while the battle goes on.
    void no_battle_screen();
    //  At the battle menu: true if the move chosen before it was turned down.
    bool refused(WallClock now);

private:
    std::optional<WallClock> m_chosen;
    bool m_list_back = false;
};


//  Registers AutoStory party management unit tests.
void add_tests_AutoStoryParty(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

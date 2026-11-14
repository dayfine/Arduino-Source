/*  AutoStory Battle Turns
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the battle loop decides each turn and at each prompt: the turn's
 *  choice and what came of it, and the answer to a YES/NO.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleTurns_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleTurns_H

#include <cstddef>
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
    //  has no PP left.
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

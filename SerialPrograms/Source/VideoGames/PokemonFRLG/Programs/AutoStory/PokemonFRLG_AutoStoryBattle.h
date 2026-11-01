/*  AutoStory Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle loop of the FRLG story, for a party of one (the starter), as
 *  the by-hand run to Brock played it (console B, 2026-11-01): at each turn
 *  the cursor is put back on FIGHT and the move list's top left, since the
 *  game remembers both; the move is the bottom left one (Bubble), which the
 *  cursor can't reach while that slot is empty, so before Bubble it stays on
 *  Tackle. A Potion is used when the HP bar turns yellow or red, an Antidote
 *  (once a battle) when the status reads PSN at two turns in a row; after
 *  the battle, PSN and PAR are cured from the bag.
 *  "Delete a move to make room?" is answered NO and "Stop learning?" YES.
 *
 *  Also the overworld interrupts the story walks routes with: wild battles
 *  run from (or fought, for EXP), and messages, after which a trainer's
 *  battle is fought instead of stopping the route.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattle_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattle_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A status condition, as far as the story treats them: the two it carries
//  items for, and the rest, which only a nurse cures.
enum class StatusCondition{
    NONE,
    POISON,
    PARALYSIS,
    OTHER,      //  SLP, BRN, FRZ
};
const char* status_name(StatusCondition status);

//  The healing items in the Items pocket, in the order they were bought:
//  Potions, Antidotes, then Parlyz Heals.
struct Supplies{
    int potions = 0;
    int antidotes = 0;
    int parlyz_heals = 0;
    //  The starter's status at the last battle screen that showed it, and
    //  not cured since.
    StatusCondition status = StatusCondition::NONE;
    //  The starter's HP bar wasn't green (or was red) at the last battle
    //  screen that showed it, and no Potion or nurse healed it since.
    bool hurt = false;
    bool hp_red = false;
};

enum class BattleKind{
    WILD_RUN,       //  RUN, or fight after a few "Can't escape!"
    WILD_FIGHT,     //  fight for EXP with green HP, no status and 2 Potions, else RUN
    TRAINER,
};

enum class BattleEnd{
    WON,
    //  The starter fainted: a blackout, except in the lab's rival battle,
    //  which the story goes on from.
    LOST,
};

//  What the HP bar and the status badge of the player's Pokemon show, on a
//  battle screen with its box. Colors measured on console B's captures
//  (2026-11-01): the bar's fill green (100, 251, 171), yellow
//  (255, 228, 23), red (255, 113, 68); the badge's place off-white
//  (255, 255, 217) with no status, (228, 168, 212) for PSN.
struct PartyStatus{
    bool hp_seen = false;
    bool hp_yellow = false;
    bool hp_red = false;
    StatusCondition status = StatusCondition::NONE;
};
PartyStatus read_party_status(const ImageViewRGB32& screen);

//  From anywhere in a battle (or its opening), plays it to the end as
//  `kind` says, using `supplies` as needed. Returns once START opens the
//  menu on the overworld: text after the battle (a gym leader's speech) is
//  read with B on the way. Throws OperationFailedException if it runs over 10 minutes.
BattleEnd fight_battle(ConsoleHandle& console, ProControllerContext& context, Supplies& supplies, BattleKind kind);


//  From the overworld, after a battle: an Antidote for PSN, a Parlyz Heal
//  for PAR, then a Potion if the HP bar wasn't green, each if one is left.
//  The bag's inputs are not checked on the console yet.
void recover_after_battle(ConsoleHandle& console, ProControllerContext& context, Supplies& supplies);


//  The interrupts for walking story routes. Wild battles are run from, or
//  fought if `fight_wild`. A message is advanced until the overworld is
//  back; a battle that starts after it (a trainer who saw the player) is
//  fought. A battle lost (a blackout) throws OperationFailedException.
OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild);



}
}
}
#endif

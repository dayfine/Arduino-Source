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
 *  (once a battle) when the status reads PSN at two turns in a row.
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


//  The healing items in the Items pocket, in the order they were bought:
//  Potions first, then Antidotes.
struct Supplies{
    int potions = 0;
    int antidotes = 0;
};

enum class BattleEnd{
    WON,
    //  The starter fainted: a blackout, except in the lab's rival battle,
    //  which the story goes on from.
    LOST,
};

//  What the HP bar and the status of the player's Pokemon show.
struct PartyStatus{
    bool hp_yellow = false;
    bool hp_red = false;
    bool poisoned = false;
};
PartyStatus read_party_status(const ImageViewRGB32& screen);

//  From anywhere in a battle (or its opening), fights it to the end, using
//  `supplies` as needed. `trainer` uses a Potion earlier. Returns once the
//  battle's screens are gone; text after it (a gym leader's speech) is left
//  for the caller. Throws OperationFailedException if it runs over
//  10 minutes.
BattleEnd fight_battle(ConsoleHandle& console, ProControllerContext& context, Supplies& supplies, bool trainer);


//  The interrupts for walking story routes. Wild battles are run from, or
//  fought if `fight_wild`. A message is advanced until the overworld is
//  back; a battle that starts after it (a trainer who saw the player) is
//  fought. A battle lost (a blackout) throws OperationFailedException.
OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild);



}
}
}
#endif

/*  AutoStory Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle loop of the FRLG story, the starter leading, as the by-hand
 *  run to Brock played it (console B, 2026-11-01): at each turn the cursor
 *  is put back on FIGHT and the move list's top left, since the game
 *  remembers both, then onto the move BattleDecisions picks. A Potion is
 *  used when the HP bar turns yellow or red, an Antidote (once a battle)
 *  when the status reads PSN at two turns in a row, an Awakening in a
 *  trainer's battle when it reads SLP; each turn's reason for a Potion or
 *  none is logged. After the battle, PSN
 *  and PAR are cured from the bag. Move prompts, an evolution after the
 *  battle and a fainted lead are as PokemonFRLG_AutoStoryBattleDecisions.h
 *  says.
 *
 *  Also the overworld interrupts the story walks routes with: wild battles
 *  run from (or fought, for EXP), and messages, after which a trainer's
 *  battle is fought instead of stopping the route.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattle_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattle_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryStatusBadge.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  Type of battle engagement strategy.
enum class BattleKind{
    WILD_RUN,       //  RUN, or fight after a few "Can't escape!"
    WILD_FIGHT,     //  fight for EXP with green HP, no status and 2 Potions, else RUN
    TRAINER,
};

//  Terminal state of a battle encounter.
enum class BattleEnd{
    WON,
    //  The starter fainted: a blackout, or a wild battle run from (or won)
    //  by the next member, except in the lab's rival battle, which the
    //  story goes on from.
    LOST,
};



//  What the HP bar (read_player_hp_bar()) and the status badge of the
//  player's Pokemon show, on a battle screen with its box. The badge's
//  place measured on console B's captures (2026-11-01): off-white
//  (255, 255, 217) with no status, (228, 168, 212) for PSN, (202, 203, 168)
//  for SLP (2026-11-03, after the Cerulean rival's Sleep Powder).
struct PartyStatus{
    bool hp_seen = false;
    bool hp_yellow = false;
    bool hp_red = false;
    //  The HP bar's filled share of its width, 0 to 1, for the log.
    double hp_fill = 0;
    StatusCondition status = StatusCondition::NONE;
};
//  Reads current party health and status from battle screen.
PartyStatus read_party_status(const ImageViewRGB32& screen);

//  Whether `screen` shows a battle, for the battle loop's end: the battle
//  menu, an HP box or a YES/NO. Not the text box alone, which also reads on
//  the black below a small room or a dark cave's corridor.
bool battle_shows(const ImageViewRGB32& screen);

//  From anywhere in a battle (or its opening), plays it to the end as
//  `kind` says, using `supplies` as needed. Returns once START opens the
//  menu on the overworld: text after the battle (a gym leader's speech) is
//  read with B on the way. After 30 s of A and B for a screen nothing reads,
//  returns anyway, for the walk to find the player. Throws
//  OperationFailedException if it runs over 10 minutes.
//  THOROUGH: an item's name is read in the bag before it is used, and a
//  different one isn't used.
BattleEnd fight_battle(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, BattleKind kind, Pokemon::StoryCheckLevel checks, Language language
);



//  The interrupts for walking story routes. Wild battles are run from, or
//  fought if `fight_wild`. A message is advanced until the overworld is
//  back; a battle that starts after it (a trainer who saw the player) is
//  fought. A battle lost (a blackout) throws OperationFailedException.
OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild, Pokemon::StoryCheckLevel checks, Language language);


//  Registers AutoStory item usage unit tests.
void add_tests_AutoStoryItems(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

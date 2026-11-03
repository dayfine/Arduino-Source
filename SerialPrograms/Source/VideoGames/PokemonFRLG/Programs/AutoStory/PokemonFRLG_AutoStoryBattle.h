/*  AutoStory Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle loop of the FRLG story, the starter leading, as the by-hand
 *  run to Brock played it (console B, 2026-11-01): at each turn the cursor
 *  is put back on FIGHT and the move list's top left, since the game
 *  remembers both, then onto the move BattleDecisions picks. A Potion is
 *  used when the HP bar turns yellow or red, an Antidote (once a battle)
 *  when the status reads PSN at two turns in a row; after the battle, PSN
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
    //  The Items pocket's order, by slug (see read_item_list_name()): the
    //  order bought, or as read from the bag. An item with none left takes
    //  no row.
    std::vector<std::string> pocket = {"potion", "antidote", "parlyz-heal"};
    //  The starter's status at the last battle screen that showed it, and
    //  not cured since.
    StatusCondition status = StatusCondition::NONE;
    //  The starter's HP bar wasn't green (or was red) at the last battle
    //  screen that showed it, and no Potion or nurse healed it since.
    bool hurt = false;
    bool hp_red = false;
    //  The starter's moves and their PP, as far as the story knows them:
    //  a new game's Squirtle unless a checkpoint's row says otherwise.
    PartyMember lead = new_member("squirtle", 5);
    //  Party slots (from 0, the lead) that are never sent in when the lead
    //  faints, so they gain no EXP and learn or evolve nothing: a member
    //  kept for later, or one the user keeps for a trade.
    std::vector<size_t> protected_slots = {};
    //  The party of the trainer the table's battle step fights, when the
    //  story knows it (trainer_party()): the lead's attacks are chosen
    //  against it while the one out isn't known. Empty otherwise; wild
    //  battles never use it.
    std::vector<std::string> foes = {};
    //  That trainer (story_trainer()), "" otherwise.
    std::string trainer = {};
    //  How carefully that battle is fought (story_battle_risk()).
    BattleRisk risk = BattleRisk::LOW;
    //  The slot of the trainer's party out, as the battle loop tracks it
    //  (TrainerFoeTracker): the lead's attacks are chosen against it.
    std::optional<size_t> foe_out = {};
    //  The wild Pokemon of the battle going on, as read at its first battle
    //  menu (read_wild_foes()): the lead's attacks are chosen against it.
    //  Empty when it wasn't read; trainer battles never use it.
    std::vector<std::string> wild_foes = {};
};

//  The nurse healed the party: HP, status and PP.
void healed_by_nurse(Supplies& supplies);

//  The lead's attacks are low on PP: the nurse before a fight that needs them.
bool attacks_low_on_pp(const Supplies& supplies);

//  Before a trainer's battle, once the items are used: the nurse for HP
//  not green (no Potion left to heal it), a status no item cured, or
//  attacks low on PP. Yellow HP counts (console B, 2026-11-03: 17/55 HP
//  and no Potion into Miguel's battle, which only red would have stopped).
bool nurse_before_battle(const Supplies& supplies);

enum class BattleKind{
    WILD_RUN,       //  RUN, or fight after a few "Can't escape!"
    WILD_FIGHT,     //  fight for EXP with green HP, no status and 2 Potions, else RUN
    TRAINER,
};

enum class BattleEnd{
    WON,
    //  The starter fainted: a blackout, or a wild battle run from (or won)
    //  by the next member, except in the lab's rival battle, which the
    //  story goes on from.
    LOST,
};

//  How many of `item` (a slug) the bag holds, as far as known.
int item_count(const Supplies& supplies, const std::string& item);

//  The row (from 0) of `item` in the Items pocket, or -1 if none is left.
int item_row(const Supplies& supplies, const std::string& item);

//  Sets the counts and the pocket's order from the Items pocket as read.
//  A row whose count couldn't be read still holds at least one: it keeps
//  the count known before (the order bought), or 1. Returns those rows'
//  items, for the log.
std::vector<std::string> set_from_bag(Supplies& supplies, const std::vector<BagRow>& rows);

//  After a save is loaded: the Items pocket as read, taken into `supplies`
//  and `saved` (what a reload restores) in place of the row's counts, as
//  set_from_bag() does. With nothing read both keep the row's. Returns the
//  items whose count couldn't be read.
std::vector<std::string> set_from_loaded_bag(Supplies& supplies, Supplies& saved, const std::vector<BagRow>& rows);

//  From the overworld: opens the Items pocket, reads it, and closes the
//  bag again. Empty if nothing could be read.
std::vector<BagRow> read_items_pocket(ConsoleHandle& console, ProControllerContext& context, Language language);


//  What the HP bar (read_player_hp_bar()) and the status badge of the
//  player's Pokemon show, on a battle screen with its box. The badge's
//  place measured on console B's captures (2026-11-01): off-white
//  (255, 255, 217) with no status, (228, 168, 212) for PSN.
struct PartyStatus{
    bool hp_seen = false;
    bool hp_yellow = false;
    bool hp_red = false;
    //  The HP bar's filled share of its width, 0 to 1, for the log.
    double hp_fill = 0;
    StatusCondition status = StatusCondition::NONE;
};
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
    Supplies& supplies, BattleKind kind, StoryChecks checks, Language language
);


//  From the overworld, after a battle: an Antidote for PSN, a Parlyz Heal
//  for PAR, then a Potion if the HP bar wasn't green, each if one is left.
//  The bag's inputs are not checked on the console yet.
void recover_after_battle(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, StoryChecks checks, Language language
);


//  The interrupts for walking story routes. Wild battles are run from, or
//  fought if `fight_wild`. A message is advanced until the overworld is
//  back; a battle that starts after it (a trainer who saw the player) is
//  fought. A battle lost (a blackout) throws OperationFailedException.
OverworldInterruptSet story_interrupts(Supplies& supplies, bool fight_wild, StoryChecks checks, Language language);


void add_tests_AutoStoryItems(UnitTestDatabase& database);



}
}
}
#endif

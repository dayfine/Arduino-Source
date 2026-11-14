/*  AutoStory Supplies
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story carries into a battle: the healing items, the lead's
 *  status, HP and moves, and the foes it knows; and when to see the nurse.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySupplies_H
#define PokemonAutomation_PokemonFRLG_AutoStorySupplies_H

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


//  A status condition, as far as the story treats them: the ones it carries
//  items for, and the rest, which only a nurse cures.
enum class StatusCondition{
    NONE,
    POISON,
    PARALYSIS,
    SLEEP,
    OTHER,      //  BRN, FRZ
};
//  String representation of a status condition.
std::string_view status_name(StatusCondition status);

//  The healing items in the Items pocket, in the order they were bought:
//  Potions, Antidotes, then Parlyz Heals; Awakenings from Cerulean's Mart.
struct Supplies{
    int potions = 0;
    int antidotes = 0;
    int parlyz_heals = 0;
    int awakenings = 0;
    //  The Items pocket's order, by slug (see read_item_list_name()): the
    //  order bought, or as read from the bag, with "" for a row whose name
    //  couldn't be read. A counted item with none left takes no row; the
    //  others always take theirs.
    std::vector<std::string> pocket = {"potion", "antidote", "parlyz-heal"};
    //  The starter's status at the last battle screen that showed it, and
    //  not cured since.
    StatusCondition status = StatusCondition::NONE;
    //  The starter's HP bar wasn't green (or was red) at the last battle
    //  screen that showed it, and no Potion or nurse healed it since.
    bool hurt = false;
    bool hp_red = false;
    //  The starter's HP bar's filled share at the last battle screen that
    //  showed it, 1 after the nurse or a Potion on the overworld.
    double hp_fill = 1.0;
    //  The starter's moves and their PP, as far as the story knows them:
    //  a new game's Squirtle unless a checkpoint's row says otherwise.
    PartyMember lead = new_member("squirtle", 5);
    //  Party slots (from 0, the lead) that are never sent in when the lead
    //  faints, so they gain no EXP and learn or evolve nothing: a member
    //  kept for later, or one the user keeps for a trade.
    std::vector<size_t> protected_slots = {};
    //  The party of the trainer the battle step fights, as its plan lists
    //  it (the step's "foes"): the lead's attacks are chosen against it
    //  while the one out isn't known. Empty otherwise; wild battles never
    //  use it.
    std::vector<std::string> foes = {};
    //  That trainer, as its plan names it (the step's "trainer"), "" otherwise.
    std::string trainer = {};
    //  How carefully that battle is fought (default_battle_risk()).
    BattleRisk risk = BattleRisk::LOW;
    //  The species of the trainer's Pokemon out, as read at the last battle
    //  menu (read_trainer_foe()), "" if not read: the lead's attacks are
    //  chosen against it.
    std::string foe_out = {};
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  AutoStory Items Pocket Change
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <vector>
#include "PokemonFRLG_AutoStoryCaptureStep.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryFossil.h"
#include "PokemonFRLG_AutoStoryGrindStep.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryItemsPocketChange.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryPcTransfer.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStorySlope.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"
#include "PokemonFRLG_AutoStoryTrashCanLayout.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::StoryStepKind;

//  A row of items_pocket_change(): an empty `action` stands for every
//  action of a kind other than GAME without a row of its own.
struct ItemsPocketChangeRow{
    StoryStepKind kind;
    std::string_view action;
    ItemsPocketChange change;
};

//  The GAME step that uses a PP item on the lead
//  (PokemonFRLG_AutoStoryHealingSteps.h).
constexpr std::string_view RESTORE_PP = "restore_pp";

//  Every place the story changes the Items pocket. COUNTED: a battle's
//  items (spend_item()) and the recovery after it, a READY's, a GRIND's
//  battles, the healing steps (took_recovery_item()), count_items
//  (set_from_bag()). STALE: the Marts (SHOP, mart: bought and sold), a
//  TALK, its ANSWER or the text after it (gifts, item balls), a use_item
//  step (Repels, Rare Candies). A walk's Repel renewed when one wears off
//  is marked where it is used (StoryWildBattle). TMs, HMs, Poke Balls and
//  key items have pockets of their own: a TEACH, a capture or a fossil
//  leave the Items pocket as it is.
const std::vector<ItemsPocketChangeRow>& items_pocket_change_rows(){
    static const std::vector<ItemsPocketChangeRow> ROWS = {
        {StoryStepKind::NEW_GAME,   "", ItemsPocketChange::NONE},
        {StoryStepKind::OPTIONS,    "", ItemsPocketChange::NONE},
        {StoryStepKind::ROUTE,      "", ItemsPocketChange::NONE},
        {StoryStepKind::LEGS,       "", ItemsPocketChange::NONE},
        {StoryStepKind::STEP,       "", ItemsPocketChange::NONE},
        {StoryStepKind::TALK,       "", ItemsPocketChange::STALE},
        {StoryStepKind::ANSWER,     "", ItemsPocketChange::STALE},
        {StoryStepKind::MASH,       "", ItemsPocketChange::STALE},
        {StoryStepKind::BATTLE,     "", ItemsPocketChange::COUNTED},
        {StoryStepKind::TRAIN,      "", ItemsPocketChange::COUNTED},
        {StoryStepKind::HEAL,       "", ItemsPocketChange::NONE},
        {StoryStepKind::SHOP,       "", ItemsPocketChange::STALE},
        {StoryStepKind::SAVE,       "", ItemsPocketChange::NONE},
        {StoryStepKind::READY,      "", ItemsPocketChange::COUNTED},
        {StoryStepKind::GAME,       CLOSE_TRAINER_CARD,     ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       LEAVE_GIFT_SUMMARY,     ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       CUT_TREE_VARIANT,       ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       PUSH_BOULDER_VARIANT,   ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       TRASH_CANS_VARIANT,     ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       PC_ACTION,              ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       CAPTURE_ACTION,         ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       FOSSIL_ACTION,          ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       LEVEL_ACTION,           ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       MART_ACTION,            ItemsPocketChange::STALE},
        {StoryStepKind::GAME,       TEACH_ACTION,           ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       GRIND_ACTION,           ItemsPocketChange::COUNTED},
        {StoryStepKind::GAME,       GRIND_CHECK_ACTION,     ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       SLOPE_VARIANT,          ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       COUNT_ITEMS_VARIANT,    ItemsPocketChange::COUNTED},
        {StoryStepKind::GAME,       POSTER_SWITCH_VARIANT,  ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       LIFT_FLOOR_VARIANT,     ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       USE_ITEM_VARIANT,       ItemsPocketChange::STALE},
        {StoryStepKind::GAME,       RIDE_BIKE_VARIANT,      ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       REGISTER_BIKE_VARIANT,  ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       CENTER_ENTER_VARIANT,   ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       CENTER_EXIT_VARIANT,    ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       FLY_VARIANT,            ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       SURF_VARIANT,           ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       HEAL_TO_FULL_ACTION,    ItemsPocketChange::COUNTED},
        {StoryStepKind::GAME,       RESTORE_PP,             ItemsPocketChange::COUNTED},
        {StoryStepKind::GAME,       CHECK_FULL_HP_ACTION,   ItemsPocketChange::NONE},
        {StoryStepKind::GAME,       BAG_CHECK_ACTION,       ItemsPocketChange::NONE},
    };
    return ROWS;
}

} // namespace

std::optional<ItemsPocketChange> items_pocket_change(StoryStepKind kind, std::string_view action){
    std::optional<ItemsPocketChange> kind_change;
    for (const ItemsPocketChangeRow& row : items_pocket_change_rows()){
        if (row.kind != kind){
            continue;
        }
        if (row.action == action){
            return row.change;
        }
        if (row.action.empty() && kind != StoryStepKind::GAME){
            kind_change = row.change;
        }
    }
    return kind_change;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

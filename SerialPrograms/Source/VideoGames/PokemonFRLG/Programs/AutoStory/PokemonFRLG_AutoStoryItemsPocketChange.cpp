/*  AutoStory Items Pocket Change
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include <vector>
#include "PokemonFRLG_AutoStoryCaptureStep.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryFossil.h"
#include "PokemonFRLG_AutoStoryGrindStep.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryHallOfFame.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryItemsPocketChange.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryPcTransfer.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRepelStep.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStorySlope.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"
#include "PokemonFRLG_AutoStoryTrashCanLayout.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view RESTORE_PP = "restore_pp";

const std::vector<std::pair<std::string_view, ItemsPocketChange>>& items_pocket_change_rows(){
    static const std::vector<std::pair<std::string_view, ItemsPocketChange>> ROWS = {
        {"new_game",            ItemsPocketChange::NONE},
        {"options",             ItemsPocketChange::NONE},
        {"route",               ItemsPocketChange::NONE},
        {"legs",                ItemsPocketChange::NONE},
        {"step",                ItemsPocketChange::NONE},
        {"talk",                ItemsPocketChange::STALE},
        {"answer",              ItemsPocketChange::STALE},
        {"scene",               ItemsPocketChange::STALE},
        {"battle",              ItemsPocketChange::COUNTED},
        {"heal",                ItemsPocketChange::NONE},
        {"shop",                ItemsPocketChange::STALE},
        {"ready",               ItemsPocketChange::COUNTED},
        {CLOSE_TRAINER_CARD,    ItemsPocketChange::NONE},
        {LEAVE_GIFT_SUMMARY,    ItemsPocketChange::NONE},
        {CUT_TREE_VARIANT,      ItemsPocketChange::NONE},
        {PUSH_BOULDER_VARIANT,  ItemsPocketChange::NONE},
        {TRASH_CANS_VARIANT,    ItemsPocketChange::NONE},
        {PC_ACTION,             ItemsPocketChange::NONE},
        {CAPTURE_ACTION,        ItemsPocketChange::NONE},
        {FOSSIL_ACTION,         ItemsPocketChange::NONE},
        {LEVEL_ACTION,          ItemsPocketChange::NONE},
        {MART_ACTION,           ItemsPocketChange::STALE},
        {TEACH_ACTION,          ItemsPocketChange::NONE},
        {GRIND_ACTION,          ItemsPocketChange::COUNTED},
        {GRIND_CHECK_ACTION,    ItemsPocketChange::NONE},
        {SLOPE_VARIANT,         ItemsPocketChange::NONE},
        {COUNT_ITEMS_VARIANT,   ItemsPocketChange::COUNTED},
        {POSTER_SWITCH_VARIANT, ItemsPocketChange::NONE},
        {LIFT_FLOOR_VARIANT,    ItemsPocketChange::NONE},
        {USE_ITEM_VARIANT,      ItemsPocketChange::STALE},
        {USE_REPEL_VARIANT,     ItemsPocketChange::STALE},
        {RIDE_BIKE_VARIANT,     ItemsPocketChange::NONE},
        {REGISTER_BIKE_VARIANT, ItemsPocketChange::NONE},
        {CENTER_ENTER_VARIANT,  ItemsPocketChange::NONE},
        {CENTER_EXIT_VARIANT,   ItemsPocketChange::NONE},
        {FLY_VARIANT,           ItemsPocketChange::NONE},
        {SURF_VARIANT,          ItemsPocketChange::NONE},
        {HEAL_TO_FULL_ACTION,   ItemsPocketChange::COUNTED},
        {RESTORE_PP,            ItemsPocketChange::COUNTED},
        {CHECK_FULL_HP_ACTION,  ItemsPocketChange::NONE},
        {BAG_CHECK_ACTION,      ItemsPocketChange::NONE},
        {HALL_OF_FAME_ACTION,   ItemsPocketChange::NONE},
        {LEAGUE_TOP_UP_ACTION,  ItemsPocketChange::COUNTED},
        {NUGGET_FARM_ACTION,    ItemsPocketChange::STALE},
    };
    return ROWS;
}

} // namespace

std::optional<ItemsPocketChange> items_pocket_change(std::string_view action){
    for (const auto& [name, change] : items_pocket_change_rows()){
        if (name == action){
            return change;
        }
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

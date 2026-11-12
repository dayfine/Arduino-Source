/*  AutoStory Items Pocket
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <string>
#include <vector>
#include "PokemonFRLG_AutoStoryItemsPocket.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The count the story keeps for `item`, or null for an item it doesn't count.
static int Supplies::* count_member(const std::string& item){
    if (item == "potion"){
        return &Supplies::potions;
    }
    if (item == "antidote"){
        return &Supplies::antidotes;
    }
    if (item == "parlyz-heal"){
        return &Supplies::parlyz_heals;
    }
    if (item == "awakening"){
        return &Supplies::awakenings;
    }
    if (item == "full-heal"){
        return &Supplies::full_heals;
    }
    return nullptr;
}

static int* count_slot(Supplies& supplies, const std::string& item){
    int Supplies::* const member = count_member(item);
    return member == nullptr ? nullptr : &(supplies.*member);
}

int item_count(const Supplies& supplies, const std::string& item){
    int Supplies::* const member = count_member(item);
    return member == nullptr ? 0 : supplies.*member;
}

void spend_item(Supplies& supplies, const std::string& item){
    if (int* count = count_slot(supplies, item)){
        (*count)--;
    }
}

//  Whether the story keeps a count for `item`.
static bool counted(const std::string& item){
    return count_member(item) != nullptr;
}

int item_row(const Supplies& supplies, const std::string& item){
    int row = 0;
    for (const std::string& slot : supplies.pocket){
        if (counted(slot) && item_count(supplies, slot) <= 0){
            continue;
        }
        if (slot == item){
            return row;
        }
        row++;
    }
    return -1;
}

std::vector<std::string> set_from_bag(Supplies& supplies, const std::vector<BagRow>& rows){
    const Supplies before = supplies;
    supplies.potions = 0;
    supplies.antidotes = 0;
    supplies.parlyz_heals = 0;
    supplies.awakenings = 0;
    supplies.full_heals = 0;
    supplies.pocket.clear();
    supplies.other_counts.clear();
    std::vector<std::string> unread;
    for (const BagRow& row : rows){
        supplies.pocket.push_back(row.item);
        int* slot = count_slot(supplies, row.item);
        if (slot == nullptr){
            if (!row.item.empty() && row.count > 0){
                supplies.other_counts[row.item] = row.count;
            }
            continue;
        }
        if (row.count > 0){
            *slot = row.count;
            continue;
        }
        //  Taking an unread count for none left the starter unhealed at
        //  9 HP until it fainted (console B, 2026-11-02).
        *slot = std::max(item_count(before, row.item), 1);
        unread.push_back(row.item);
    }
    return unread;
}

std::vector<std::string> set_from_loaded_bag(Supplies& supplies, Supplies& saved, const std::vector<BagRow>& rows){
    if (rows.empty()){
        return {};
    }
    std::vector<std::string> unread = set_from_bag(supplies, rows);
    saved.potions = supplies.potions;
    saved.antidotes = supplies.antidotes;
    saved.parlyz_heals = supplies.parlyz_heals;
    saved.awakenings = supplies.awakenings;
    saved.full_heals = supplies.full_heals;
    saved.pocket = supplies.pocket;
    saved.other_counts = supplies.other_counts;
    return unread;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

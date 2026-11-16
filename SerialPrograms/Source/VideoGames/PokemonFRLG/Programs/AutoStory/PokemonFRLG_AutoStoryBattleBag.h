/*  AutoStory Battle Bag
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Items used from the Items pocket in battle, on the console or a test's
 *  bag: each found at its row, the pocket read again when the row holds
 *  another item.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBattleBag_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBattleBag_H

#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  In the bag, with the cursor on `row` (from 0), shown as row `shown`:
//  whether that row is `item`, read when the check level runs MENU_ROW
//  checks; taken as it otherwise. A row that can't be read (as in a
//  language the reader has no names for) is taken by its position; only
//  another item's name stops it.
bool bag_row_is(
    ConsoleHandle& console, ProControllerContext& context,
    int row, int shown, const std::string& item, Pokemon::StoryCheckLevel checks, Language language
);

//  The bag in battle, from the battle menu and back to it: the console's
//  or a test's.
class BattleBag{
public:
    virtual ~BattleBag() = default;
    //  The Items pocket read whole (read_whole_pocket()). Empty if nothing
    //  could be read.
    virtual std::vector<BagRow> read_pocket(const Supplies& supplies) = 0;
    //  The cursor on `row` (from 0) of the Items pocket, shown as row
    //  `shown`: if that row is `item`, it is used and true; else the bag
    //  closed with nothing used, and false.
    virtual bool use_row(const Supplies& supplies, int row, int shown, const std::string& item) = 0;
};

//  `item` from the Items pocket in battle, at its row as `supplies` has
//  it. A row that reads as another item gets the pocket read again, its
//  counts and order taken from it (set_from_bag()), and the item tried
//  once more at its row there: one mismatch skipped the Route 22 rival's
//  X Specials (console B, 2026-11-16). Returns whether it was used; the
//  counts aren't spent here.
bool use_battle_item(const std::string& item, Logger& logger, BattleBag& bag, Supplies& supplies);

//  What an item in battle is used on.
enum class ItemTarget{
    //  A Potion or a cure: the party list opens, its cursor on the starter.
    PARTY_MEMBER,
    //  An X item: the Pokemon out, with no party list.
    POKEMON_OUT,
};

//  The console's bag in battle, an item used on `target`. It opens on the
//  pocket used last; Items is the first.
class ConsoleBattleBag : public BattleBag{
public:
    ConsoleBattleBag(ConsoleHandle& console, ProControllerContext& context, ItemTarget target, Pokemon::StoryCheckLevel checks, Language language);
    virtual std::vector<BagRow> read_pocket(const Supplies& supplies) override;
    virtual bool use_row(const Supplies& supplies, int row, int shown, const std::string& item) override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    ItemTarget m_target;
    Pokemon::StoryCheckLevel m_checks;
    Language m_language;
};

//  The battle's supply line, for its turn log: the Potions and Super
//  Potions, every other HP medicine the bag was read to hold, then the
//  cures. 6 Super Potions went unlisted at the Route 22 rival (console B,
//  2026-11-16).
std::string supply_line(const Supplies& supplies);


//  use_battle_item() on a made-up bag, and the supply line.
void add_tests_AutoStoryBattleBag(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

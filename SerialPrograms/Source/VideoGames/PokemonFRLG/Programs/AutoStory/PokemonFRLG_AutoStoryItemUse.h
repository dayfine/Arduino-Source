/*  AutoStory Item Use
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Items the story uses from the bag on the overworld, as PokeGuy's run
 *  does: a Repel or Super Repel before long walks through grass and caves,
 *  renewed when it wears off; an Escape Rope out of the Rocket Hideout;
 *  and the Bicycle. The bag is read with read_bag_pocket(), so the step
 *  finds its item wherever the pocket keeps it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryItemUse_H
#define PokemonAutomation_PokemonFRLG_AutoStoryItemUse_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that uses an item from the bag on the overworld:
//  "use_item <slug>", the slug as read_item_list_name() names it.
inline constexpr std::string_view USE_ITEM_PREFIX = "use_item ";
//  The GAME step that gets on the Bicycle (the Key Items pocket's BICYCLE,
//  USE). The ROUTE steps after it ride (BIKE_VARIANT).
inline const std::string RIDE_BIKE_VARIANT = "ride_bike";

//  The bag's pockets, in the order LEFT and RIGHT go through them.
enum class BagPocket{
    ITEMS,
    KEY_ITEMS,
    POKE_BALLS,
};

//  The pocket of an item the story uses from the bag: repel, super-repel,
//  max-repel, escape-rope (Items) or bicycle (Key Items); nothing for
//  another.
std::optional<BagPocket> usable_item_pocket(std::string_view item);
//  Whether `item` is a Repel of any kind.
bool is_repel(std::string_view item);

//  The variant that uses `item`.
std::string use_item_variant(std::string_view item);
//  The item a "use_item <slug>" variant uses, or nothing if it isn't one
//  or the item isn't one the story uses from the bag.
std::optional<std::string> parse_use_item_variant(std::string_view variant);
//  Whether `variant` is one of this file's steps.
bool is_item_use_variant(std::string_view variant);

//  The row of `item` in `rows` (read_bag_pocket()'s, from the top), if it
//  has one left: a count of -1 (unread) counts as some.
std::optional<size_t> bag_row_of(const std::vector<BagRow>& rows, std::string_view item);


//  The bag, as use_item_from_bag() drives it: the console's, or a test's.
class ItemUseActions{
public:
    virtual ~ItemUseActions() = default;
    //  From the overworld, opens the bag on `pocket`, scrolled to its top.
    virtual bool open_pocket(BagPocket pocket) = 0;
    //  The open pocket's rows, from the top.
    virtual std::vector<BagRow> read_pocket() = 0;
    //  Moves to row `row`, USE, and through what the item says.
    virtual void use_row(size_t row) = 0;
    //  Back to the overworld (nothing to do if the item closed the bag).
    virtual void close_bag() = 0;
};

//  How use_item_from_bag() ended.
enum class ItemUseEnd{
    USED,
    //  The pocket has none left (or the name isn't read).
    NONE_LEFT,
    //  The bag didn't open.
    NO_BAG,
    //  Not an item the story uses from the bag.
    UNKNOWN_ITEM,
};
//  "used", "none_left": the end's name in logs and failures.
const char* item_use_end_name(ItemUseEnd end);

//  Opens the item's pocket, finds it and uses it, then back to the
//  overworld.
ItemUseEnd use_item_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item);

//  After "REPEL's effect wore off..." has closed: FRLG asks nothing, so
//  another of the same Repel is used if the bag has one. NONE_LEFT is no
//  failure: the walk goes on without.
ItemUseEnd renew_repel(Logger& logger, ItemUseActions& actions, std::string_view item);


//  The console's bag.
class ConsoleItemUseActions : public ItemUseActions{
public:
    ConsoleItemUseActions(ConsoleHandle& console, ProControllerContext& context, Language language);

    virtual bool open_pocket(BagPocket pocket) override;
    virtual std::vector<BagRow> read_pocket() override;
    virtual void use_row(size_t row) override;
    virtual void close_bag() override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
};


//  Registers the item use tests.
void add_tests_AutoStoryItemUse(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

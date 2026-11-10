/*  AutoStory Item Use Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  use_item_from_bag() and renew_repel() on a made-up bag, and the bike's
 *  pace.
 *
 */

#include <format>
#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemUse.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A bag on the sim: its pockets, each item used takes one, a bag that may
//  not open.
class SimBag : public ItemUseActions{
public:
    std::map<BagPocket, std::vector<BagRow>> pockets;
    bool opens = true;
    std::vector<std::string> used;
    bool open = false;

    virtual bool open_pocket(BagPocket pocket) override{
        m_pocket = pocket;
        open = opens;
        return opens;
    }
    virtual std::vector<BagRow> read_pocket() override{
        return pockets[m_pocket];
    }
    virtual void use_row(size_t row) override{
        BagRow& item = pockets[m_pocket][row];
        used.push_back(item.item);
        if (item.count > 0){
            item.count--;
        }
    }
    virtual void close_bag() override{
        open = false;
    }

private:
    BagPocket m_pocket = BagPocket::ITEMS;
};

struct ItemUseInput{
    std::string name;
    std::string variant;
    std::vector<BagRow> items;
    std::vector<BagRow> key_items;
    bool opens = true;
    //  renew_repel() instead of use_item_from_bag().
    bool renew = false;
};
struct ItemUseExpectation{
    ItemUseEnd end;
    std::vector<std::string> used;
};
struct ItemUseCase{
    ItemUseInput input;
    ItemUseExpectation expectation;
};

class Test_AutoStoryItemUse : public UnitTest{
public:
    explicit Test_AutoStoryItemUse(ItemUseCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryItemUse - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        SimBag bag;
        bag.pockets[BagPocket::ITEMS] = m_case.input.items;
        bag.pockets[BagPocket::KEY_ITEMS] = m_case.input.key_items;
        bag.opens = m_case.input.opens;
        const std::string variant = m_case.input.variant;
        const std::string item = variant == RIDE_BIKE_VARIANT ? "bicycle" : parse_use_item_variant(variant).value_or(variant);
        const ItemUseEnd end = m_case.input.renew ? renew_repel(logger, bag, item) : use_item_from_bag(logger, bag, item);
        if (end != m_case.expectation.end || bag.used != m_case.expectation.used || bag.open){
            return std::format(
                "\"{}\": {} using {} item(s), bag {}; expected {} using {}.", variant,
                item_use_end_name(end), bag.used.size(), bag.open ? "open" : "closed",
                item_use_end_name(m_case.expectation.end), m_case.expectation.used.size()
            );
        }
        return true;
    }

private:
    ItemUseCase m_case;
};

const ItemUseCase ITEM_USE_CASES[] = {
    {
        .input = {
            .name = "a Repel below the Potions",
            .variant = "use_item repel",
            .items = {{"potion", 15}, {"repel", 5}, {"escape-rope", 2}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .input = {
            .name = "the Escape Rope with its count unread",
            .variant = "use_item escape-rope",
            .items = {{"potion", 15}, {"", -1}, {"escape-rope", -1}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"escape-rope"}},
    },
    {
        .input = {
            .name = "no Super Repel bought",
            .variant = "use_item super-repel",
            .items = {{"potion", 15}, {"repel", 5}},
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .input = {
            .name = "the bag doesn't open",
            .variant = "use_item repel",
            .items = {{"repel", 5}},
            .opens = false,
        },
        .expectation = {.end = ItemUseEnd::NO_BAG, .used = {}},
    },
    {
        .input = {
            .name = "the Bicycle from the Key Items",
            .variant = "ride_bike",
            .items = {{"bicycle", 1}},
            .key_items = {{"", -1}, {"bicycle", -1}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"bicycle"}},
    },
    {
        .input = {
            .name = "a Repel wore off: another used",
            .variant = "use_item repel",
            .items = {{"repel", 1}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .input = {
            .name = "the last Repel wore off: none left",
            .variant = "use_item repel",
            .items = {{"repel", 0}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .input = {
            .name = "an Escape Rope is no Repel to renew",
            .variant = "use_item escape-rope",
            .items = {{"escape-rope", 2}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::UNKNOWN_ITEM, .used = {}},
    },
};

class Test_AutoStoryItemUseVariants : public UnitTest{
public:
    Test_AutoStoryItemUseVariants()
        : UnitTest("PokemonFRLG::AutoStoryItemUse - variants and the bike's pace")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (parse_use_item_variant("use_item potion") || parse_use_item_variant("use_item") || !parse_use_item_variant(use_item_variant("super-repel"))){
            return "Expected only the story's bag items as use_item variants.";
        }
        if (!is_item_use_variant(RIDE_BIKE_VARIANT) || is_item_use_variant("poster_switch")){
            return "Expected ride_bike, not poster_switch, as an item use.";
        }
        StoryStep ride;
        ride.kind = Pokemon::StoryStepKind::ROUTE;
        ride.variant = BIKE_VARIANT;
        if (move_speed(ride) != MoveSpeed::BIKE){
            return "A bike ROUTE isn't ridden.";
        }
        RouteStep outdoors;
        outdoors.tiles = 10;
        RouteStep indoors = outdoors;
        indoors.walk_only = true;
        if (OverworldWalkSteps::tile_time(outdoors, MoveSpeed::BIKE) != MovementMechanics::BIKE_TILE_TIME
            || OverworldWalkSteps::tile_time(indoors, MoveSpeed::BIKE) != MovementMechanics::TILE_TIME
        ){
            return "Expected the bike's pace outdoors and a walk indoors.";
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryItemUse(UnitTestDatabase& database){
    for (const ItemUseCase& item : ITEM_USE_CASES){
        database.add<Test_AutoStoryItemUse>(item);
    }
    database.add<Test_AutoStoryItemUseVariants>();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BikeSpinMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryFieldSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A use_item step's positional key, for game_step_of().
const std::string_view ITEM[] = {"item"};

//  A bag on the sim: its pockets, each item used takes one, a bag that may
//  not open.
class SimBag : public ItemUseActions{
public:
    std::map<BagPocket, std::vector<BagRow>> pockets;
    bool opens = true;
    //  The lead's level-up text never shows.
    bool no_level_up = false;
    std::vector<std::string> used;
    std::vector<std::string> used_on_lead;
    std::vector<std::string> registered;
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
    virtual bool use_row_on_lead(size_t row) override{
        if (no_level_up){
            return false;
        }
        use_row(row);
        used_on_lead.push_back(pockets[m_pocket][row].item);
        return true;
    }
    virtual void register_row(size_t row) override{
        registered.push_back(pockets[m_pocket][row].item);
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
    //  The step's count (use_items_from_bag()).
    size_t count = 1;
    bool no_level_up = false;
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
        bag.no_level_up = m_case.input.no_level_up;
        const std::string variant = m_case.input.variant;
        const std::string item = variant == RIDE_BIKE_VARIANT ? "bicycle" : parse_use_item_step(game_step_of(variant, ITEM)).value_or(variant);
        const ItemUseEnd end = m_case.input.renew
            ? renew_repel(logger, bag, item)
            : use_items_from_bag(logger, bag, item, m_case.input.count).end;
        const bool lead_ok = is_party_item(item) ? bag.used_on_lead == bag.used : bag.used_on_lead.empty();
        if (end != m_case.expectation.end || bag.used != m_case.expectation.used || bag.open || !lead_ok){
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
            .name = "two Rare Candies on the lead, the bag holding five",
            .variant = "use_item rare-candy",
            .items = {{"potion", 3}, {"rare-candy", 5}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"rare-candy", "rare-candy"}},
    },
    {
        .input = {
            .name = "two Rare Candies asked, one in the bag: the one is used",
            .variant = "use_item rare-candy",
            .items = {{"rare-candy", 1}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"rare-candy"}},
    },
    {
        .input = {
            .name = "no Rare Candy in the bag",
            .variant = "use_item rare-candy",
            .items = {{"potion", 3}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .input = {
            .name = "a Rare Candy whose level-up never shows",
            .variant = "use_item rare-candy",
            .items = {{"rare-candy", 2}},
            .count = 2,
            .no_level_up = true,
        },
        .expectation = {.end = ItemUseEnd::NOT_USED, .used = {}},
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

UnitTestResult test_variants_and_bike(){
    if (parse_use_item_step(game_step_of("use_item potion", ITEM)) || parse_use_item_step(game_step_of("use_item", ITEM))
        || parse_use_item_step(game_step_of("use_item repel slot=1", ITEM))
        || !parse_use_item_step(game_step_of("use_item super-repel", ITEM))
        || parse_use_item_step(game_step_of("use_item rare-candy slot=1", ITEM))
        || parse_use_item_step(game_step_of("use_item rare-candy count=0", ITEM))
        || !parse_use_item_step(game_step_of("use_item rare-candy count=2", ITEM))
        || use_item_count(game_step_of("use_item rare-candy count=2", ITEM)) != 2
        || use_item_count(game_step_of("use_item rare-candy", ITEM)) != 1
    ){
        return "Expected only the story's bag items as use_item steps.";
    }
    if (!is_item_use_step(game_step_of(RIDE_BIKE_VARIANT)) || is_item_use_step(game_step_of("poster_switch"))){
        return "Expected ride_bike, not poster_switch, as an item use.";
    }
    StoryStep ride;
    ride.kind = Pokemon::StoryStepKind::ROUTE;
    ride.action = BIKE_VARIANT;
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

struct AutoStoryItemUseBodyInput{
    std::string name;
    UnitTestResult (*body)();
};
struct AutoStoryItemUseBodyExpectation{
    std::string description;
};
struct AutoStoryItemUseBodyCase{
    AutoStoryItemUseBodyInput input;
    AutoStoryItemUseBodyExpectation expectation;
};

class Test_AutoStoryItemUseBody : public UnitTest{
public:
    explicit Test_AutoStoryItemUseBody(AutoStoryItemUseBodyCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryItemUse - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    AutoStoryItemUseBodyCase m_case;
};

const AutoStoryItemUseBodyCase ITEM_USE_BODY_CASES[] = {
    {
        .input = {.name = "variants and the bike's pace", .body = test_variants_and_bike},
        .expectation = {.description = "the story's bag items, ride_bike, and the bike's pace outdoors only"},
    },
};

} // namespace


void add_tests_AutoStoryItemUse(UnitTestDatabase& database){
    for (const ItemUseCase& item : ITEM_USE_CASES){
        database.add<Test_AutoStoryItemUse>(item);
    }
    for (const AutoStoryItemUseBodyCase& item : ITEM_USE_BODY_CASES){
        database.add<Test_AutoStoryItemUseBody>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

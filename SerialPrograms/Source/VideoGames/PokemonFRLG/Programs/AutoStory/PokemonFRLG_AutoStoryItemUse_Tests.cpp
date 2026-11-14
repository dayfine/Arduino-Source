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
#include <optional>
#include <string>
#include <string_view>
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
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryTable.h"

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
    //  The bag never comes back after an item's message on the lead.
    bool no_bag_back = false;
    //  What a Repel's USE says; one that lingers isn't used.
    RepelUse repel_says = RepelUse::USED;
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
        if (is_repel(item.item) && repel_says == RepelUse::LINGERED){
            return;
        }
        used.push_back(item.item);
        if (item.count > 0){
            item.count--;
        }
    }
    virtual RepelUse repel_use() override{
        return repel_says;
    }
    virtual bool use_row_on_lead(size_t row) override{
        if (no_level_up){
            return false;
        }
        use_row(row);
        used_on_lead.push_back(pockets[m_pocket][row].item);
        return true;
    }
    virtual bool use_row_on_lead_to_bag(size_t row) override{
        if (no_bag_back){
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
    std::string variant;
    std::vector<BagRow> items;
    std::vector<BagRow> key_items;
    bool opens = true;
    //  renew_repel() instead of use_item_from_bag().
    bool renew = false;
    //  The step's count (use_items_from_bag()).
    size_t count = 1;
    bool no_level_up = false;
    bool no_bag_back = false;
    RepelUse repel_says = RepelUse::USED;
};
struct ItemUseExpectation{
    ItemUseEnd end;
    std::vector<std::string> used;
};
struct ItemUseCase{
    std::string name;
    ItemUseInput input;
    ItemUseExpectation expectation;
};

class Test_AutoStoryItemUse : public UnitTest{
public:
    explicit Test_AutoStoryItemUse(ItemUseCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryItemUse - " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        SimBag bag;
        bag.pockets[BagPocket::ITEMS] = m_case.input.items;
        bag.pockets[BagPocket::KEY_ITEMS] = m_case.input.key_items;
        bag.opens = m_case.input.opens;
        bag.no_level_up = m_case.input.no_level_up;
        bag.no_bag_back = m_case.input.no_bag_back;
        bag.repel_says = m_case.input.repel_says;
        const std::string variant = m_case.input.variant;
        const std::string item = variant == RIDE_BIKE_VARIANT ? "bicycle" : parse_use_item_step(game_step_of(variant, ITEM)).value_or(variant);
        const ItemUseEnd end = m_case.input.renew
            ? renew_repel(logger, bag, item)
            : use_items_from_bag(logger, bag, item, m_case.input.count).end;
        const bool lead_ok = is_party_item(item) || is_lead_recovery_item(item) ? bag.used_on_lead == bag.used : bag.used_on_lead.empty();
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
        .name = "a Repel below the Potions",
        .input = {
            .variant = "use_item repel",
            .items = {{"potion", 15}, {"repel", 5}, {"escape-rope", 2}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .name = "the Escape Rope with its count unread",
        .input = {
            .variant = "use_item escape-rope",
            .items = {{"potion", 15}, {"", -1}, {"escape-rope", -1}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"escape-rope"}},
    },
    {
        .name = "no Super Repel bought",
        .input = {
            .variant = "use_item super-repel",
            .items = {{"potion", 15}, {"repel", 5}},
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .name = "the bag doesn't open",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 5}},
            .opens = false,
        },
        .expectation = {.end = ItemUseEnd::NO_BAG, .used = {}},
    },
    {
        .name = "the Bicycle from the Key Items",
        .input = {
            .variant = "ride_bike",
            .items = {{"bicycle", 1}},
            .key_items = {{"", -1}, {"bicycle", -1}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"bicycle"}},
    },
    {
        .name = "a Repel wore off: another used",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 1}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .name = "the last Repel wore off: none left",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 0}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    //  Console B, 2026-11-13 (12.03 repel_pallet): the step's Repel
    //  lingered from before the save and wore off on Route 21, with none
    //  used after it.
    {
        .name = "a Repel still on: lingered, none used",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 4}},
            .repel_says = RepelUse::LINGERED,
        },
        .expectation = {.end = ItemUseEnd::LINGERED, .used = {}},
    },
    {
        .name = "a Repel's message unread: taken as used",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 4}},
            .repel_says = RepelUse::UNREAD,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .name = "the last Repel wore off: a Super Repel instead",
        .input = {
            .variant = "use_item repel",
            .items = {{"potion", 3}, {"repel", 0}, {"super-repel", 5}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"super-repel"}},
    },
    {
        .name = "two Repels asked: one, as a second would linger",
        .input = {
            .variant = "use_item repel",
            .items = {{"repel", 4}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"repel"}},
    },
    {
        .name = "two Rare Candies on the lead, the bag holding five",
        .input = {
            .variant = "use_item rare-candy",
            .items = {{"potion", 3}, {"rare-candy", 5}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"rare-candy", "rare-candy"}},
    },
    {
        .name = "two Rare Candies asked, one in the bag: the one is used",
        .input = {
            .variant = "use_item rare-candy",
            .items = {{"rare-candy", 1}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"rare-candy"}},
    },
    {
        .name = "no Rare Candy in the bag",
        .input = {
            .variant = "use_item rare-candy",
            .items = {{"potion", 3}},
            .count = 2,
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .name = "a Rare Candy whose level-up never shows",
        .input = {
            .variant = "use_item rare-candy",
            .items = {{"rare-candy", 2}},
            .count = 2,
            .no_level_up = true,
        },
        .expectation = {.end = ItemUseEnd::NOT_USED, .used = {}},
    },
    {
        .name = "an Escape Rope is no Repel to renew",
        .input = {
            .variant = "use_item escape-rope",
            .items = {{"escape-rope", 2}},
            .renew = true,
        },
        .expectation = {.end = ItemUseEnd::UNKNOWN_ITEM, .used = {}},
    },
    //  The healing steps' items, by slug: a use_item step doesn't take them.
    {
        .name = "an Elixir on the lead, back to the bag",
        .input = {
            .variant = "elixir",
            .items = {{"potion", 4}, {"max-elixir", 1}, {"elixir", 1}, {"full-restore", 12}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"elixir"}},
    },
    {
        .name = "an Elixir whose bag never comes back",
        .input = {
            .variant = "elixir",
            .items = {{"elixir", 1}},
            .no_bag_back = true,
        },
        .expectation = {.end = ItemUseEnd::NOT_USED, .used = {}},
    },
    {
        .name = "no Elixir in the bag, only a Max Elixir",
        .input = {
            .variant = "elixir",
            .items = {{"max-elixir", 1}},
        },
        .expectation = {.end = ItemUseEnd::NONE_LEFT, .used = {}},
    },
    {
        .name = "a Full Restore on the lead",
        .input = {
            .variant = "full-restore",
            .items = {{"potion", 4}, {"full-restore", 12}},
        },
        .expectation = {.end = ItemUseEnd::USED, .used = {"full-restore"}},
    },
};

//  "use_item rare-candy" with its count, an integer as the plans give it.
StoryStep candy_step(int64_t count){
    StoryStep step = game_step_of("use_item rare-candy", ITEM);
    step.args["count"] = count;
    return step;
}

//  repel_renew_order(): the smallest Repel that lasts the steps left, or
//  the one used, then larger, then smaller.
UnitTestResult test_repel_renew_order(){
    struct Case{
        std::string_view used;
        std::optional<int> remaining;
        std::vector<std::string> order;
    };
    const Case cases[] = {
        {"repel", std::nullopt, {"repel", "super-repel", "max-repel"}},
        {"super-repel", std::nullopt, {"super-repel", "max-repel", "repel"}},
        {"max-repel", std::nullopt, {"max-repel", "super-repel", "repel"}},
        {"repel", 60, {"repel", "super-repel", "max-repel"}},
        {"repel", 150, {"super-repel", "max-repel", "repel"}},
        {"repel", 400, {"max-repel", "super-repel", "repel"}},
    };
    for (const Case& test : cases){
        if (repel_renew_order(test.used, test.remaining) != test.order){
            return std::format("{} with {} steps left: wrong order.", test.used, test.remaining.value_or(-1));
        }
    }
    return true;
}

UnitTestResult test_variants_and_bike(){
    if (parse_use_item_step(game_step_of("use_item potion", ITEM)) || parse_use_item_step(game_step_of("use_item", ITEM))
        || parse_use_item_step(game_step_of("use_item repel slot=1", ITEM))
        || !parse_use_item_step(game_step_of("use_item super-repel", ITEM))
        || parse_use_item_step(game_step_of("use_item rare-candy slot=1", ITEM))
        || parse_use_item_step(candy_step(0))
        || !parse_use_item_step(candy_step(2))
        || use_item_count(candy_step(2)) != 2
        || use_item_count(game_step_of("use_item rare-candy", ITEM)) != 1
        || parse_use_item_step(game_step_of("use_item elixir", ITEM))
        || parse_use_item_step(game_step_of("use_item full-restore", ITEM))
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

struct StepDoneInput{
    ItemUseEnd end;
    //  The step's "if_any" arg.
    bool if_any = false;
};
struct StepDoneExpectation{
    bool done;
};
struct StepDoneCase{
    std::string name;
    StepDoneInput input;
    StepDoneExpectation expectation;
};

class Test_ItemUseStepDone : public UnitTest{
public:
    explicit Test_ItemUseStepDone(StepDoneCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryItemUse - step done: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (item_use_step_done(m_case.input.end, m_case.input.if_any) != m_case.expectation.done){
            return std::format("{}: expected done = {}.", item_use_end_name(m_case.input.end), m_case.expectation.done);
        }
        return true;
    }

private:
    StepDoneCase m_case;
};

const StepDoneCase STEP_DONE_CASES[] = {
    {.name = "used", .input = {.end = ItemUseEnd::USED}, .expectation = {.done = true}},
    {.name = "a Repel lingered", .input = {.end = ItemUseEnd::LINGERED}, .expectation = {.done = true}},
    {.name = "none left", .input = {.end = ItemUseEnd::NONE_LEFT}, .expectation = {.done = false}},
    {.name = "none left, if_any", .input = {.end = ItemUseEnd::NONE_LEFT, .if_any = true}, .expectation = {.done = true}},
    {.name = "none left, if_any false", .input = {.end = ItemUseEnd::NONE_LEFT, .if_any = false}, .expectation = {.done = false}},
    {.name = "no bag, if_any", .input = {.end = ItemUseEnd::NO_BAG, .if_any = true}, .expectation = {.done = false}},
};

//  A Super Repel step of the story table, PokeGuy's (FRLG_POKEGUY_15934_VIDEO_NOTES.md).
struct SuperRepelInput{
    std::string_view checkpoint;
    std::string_view step;
};
//  The steps around it ("" for none: the checkpoint's first or last), and
//  the Super Repels in the bag before it: those the Mart steps bought less
//  those the use_item steps used, in story order. Repels renewed in a
//  battle aren't counted, so the step skips if none is left (if_any).
struct SuperRepelExpectation{
    std::string_view previous;
    std::string_view next;
    size_t in_bag;
};
struct SuperRepelCase{
    std::string name;
    SuperRepelInput input;
    SuperRepelExpectation expectation;
};

class Test_SuperRepelStep : public UnitTest{
public:
    explicit Test_SuperRepelStep(SuperRepelCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryItemUse - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        size_t in_bag = 0;
        for (const StoryCheckpoint& row : story_checkpoints()){
            for (size_t c = 0; c < row.steps.size(); c++){
                const StoryStep& step = row.steps[c];
                if (row.id == m_case.input.checkpoint && step.name == m_case.input.step){
                    return check(row, c, in_bag);
                }
                in_bag = super_repels_after(step, in_bag);
            }
        }
        return std::format("No step {} in {}.", m_case.input.step, m_case.input.checkpoint);
    }

private:
    static size_t super_repels_after(const StoryStep& step, size_t in_bag){
        if (step.action == MART_ACTION){
            for (const MartLine& line : mart_plan_of(step.args).value_or(MartPlan()).lines){
                in_bag += line.item == "super-repel" ? line.quantity : 0;
            }
        }
        if (parse_use_item_step(step) == "super-repel" && in_bag > 0){
            in_bag--;
        }
        return in_bag;
    }
    UnitTestResult check(const StoryCheckpoint& row, size_t index, size_t in_bag) const{
        const StoryStep& step = row.steps[index];
        const std::string_view previous = index == 0 ? "" : std::string_view(row.steps[index - 1].name);
        const std::string_view next = index + 1 == row.steps.size() ? "" : std::string_view(row.steps[index + 1].name);
        const bool* if_any = game_arg<bool>(step.args, "if_any");
        if (parse_use_item_step(step) != "super-repel" || if_any == nullptr || !*if_any){
            return std::format("{}/{}: expected a use_item super-repel step with if_any.", row.id, step.name);
        }
        if (previous != m_case.expectation.previous || next != m_case.expectation.next){
            return std::format("{}/{}: between \"{}\" and \"{}\".", row.id, step.name, previous, next);
        }
        if (in_bag != m_case.expectation.in_bag){
            return std::format("{}/{}: {} Super Repels in the bag before it.", row.id, step.name, in_bag);
        }
        return true;
    }

    SuperRepelCase m_case;
};

const SuperRepelCase SUPER_REPEL_CASES[] = {
    {
        .name = "12.03's Super Repel before the Fly to Pallet",
        .input = {.checkpoint = "12.03", .step = "super_repel_fuchsia"},
        .expectation = {.previous = "", .next = "outside_fuchsia_center", .in_bag = 5},
    },
    {
        .name = "13.01's Super Repel after 3F's statue",
        .input = {.checkpoint = "13.01", .step = "super_repel_mansion"},
        .expectation = {.previous = "mansion_holes", .next = "", .in_bag = 4},
    },
};

struct AutoStoryItemUseBodyInput{
    UnitTestResult (*body)();
};
struct AutoStoryItemUseBodyExpectation{
    std::string description;
};
struct AutoStoryItemUseBodyCase{
    std::string name;
    AutoStoryItemUseBodyInput input;
    AutoStoryItemUseBodyExpectation expectation;
};

class Test_AutoStoryItemUseBody : public UnitTest{
public:
    explicit Test_AutoStoryItemUseBody(AutoStoryItemUseBodyCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryItemUse - " + test_case.name)
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
        .name = "variants and the bike's pace",
        .input = {.body = test_variants_and_bike},
        .expectation = {.description = "the story's bag items, ride_bike, and the bike's pace outdoors only"},
    },
    {
        .name = "the Repels tried after one wore off",
        .input = {.body = test_repel_renew_order},
        .expectation = {.description = "the smallest that lasts the steps left, or the one used, then larger, then smaller"},
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
    for (const StepDoneCase& item : STEP_DONE_CASES){
        database.add<Test_ItemUseStepDone>(item);
    }
    for (const SuperRepelCase& item : SUPER_REPEL_CASES){
        database.add<Test_SuperRepelStep>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  AutoStory Battle Bag Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  use_battle_item() on a made-up bag in battle, and the battle's supply
 *  line.
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattleBag.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  19.01b's Items pocket at the Route 22 rival (console B, 2026-11-16).
const std::vector<BagRow> ROUTE22_RIVAL_POCKET = {
    {"moon-stone", 2}, {"potion", 3}, {"repel", 2}, {"escape-rope", 4}, {"super-repel", 4},
    {"rare-candy", 1}, {"x-special", 14}, {"x-speed", 6}, {"x-accuracy", 3}, {"guard-spec", 2},
    {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6}, {"antidote", 1}, {"parlyz-heal", 2},
};

//  `rows` without `item`'s row: the pocket as counted before the item came
//  or after the last one went, the rows below it one higher.
std::vector<BagRow> rows_without(const std::vector<BagRow>& rows, std::string_view item){
    std::vector<BagRow> kept;
    for (const BagRow& row : rows){
        if (row.item != item){
            kept.push_back(row);
        }
    }
    return kept;
}

//  A bag in battle on the sim: its Items pocket, a row used if it holds
//  the item asked for.
class SimBattleBag : public BattleBag{
public:
    explicit SimBattleBag(std::vector<BagRow> rows)
        : m_rows(std::move(rows))
    {}
    virtual std::vector<BagRow> read_pocket(const Supplies&) override{
        m_reads++;
        return m_rows;
    }
    virtual bool use_row(const Supplies&, int row, int, const std::string& item) override{
        m_tried.push_back(row);
        return row >= 0 && static_cast<size_t>(row) < m_rows.size() && m_rows[static_cast<size_t>(row)].item == item;
    }
    int reads() const{
        return m_reads;
    }
    const std::vector<int>& tried() const{
        return m_tried;
    }

private:
    std::vector<BagRow> m_rows;
    int m_reads = 0;
    std::vector<int> m_tried;
};

struct BattleItemInput{
    //  The pocket as the story's counts have it.
    std::vector<BagRow> counted;
    //  The pocket the bag really holds.
    std::vector<BagRow> bag;
    std::string item;
};
struct BattleItemExpectation{
    bool used;
    //  The rows tried, from 0, in turn.
    std::vector<int> tried;
    int reads;
};
struct BattleItemCase{
    std::string name;
    BattleItemInput input;
    BattleItemExpectation expectation;
};

class Test_UseBattleItem : public UnitTest{
public:
    explicit Test_UseBattleItem(BattleItemCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryBattleBag - battle item: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        Supplies supplies;
        set_from_bag(supplies, m_case.input.counted);
        SimBattleBag bag(m_case.input.bag);
        const bool used = use_battle_item(m_case.input.item, logger, bag, supplies);
        const BattleItemExpectation& expected = m_case.expectation;
        if (used != expected.used || bag.tried() != expected.tried || bag.reads() != expected.reads){
            return std::format(
                "Used {} after {} rows tried and {} reads; expected {} after {} and {}.",
                used, bag.tried().size(), bag.reads(), expected.used, expected.tried.size(), expected.reads
            );
        }
        return true;
    }

private:
    BattleItemCase m_case;
};

const BattleItemCase BATTLE_ITEM_CASES[] = {
    {
        .name = "its row as counted: used, no read",
        .input = {.counted = ROUTE22_RIVAL_POCKET, .bag = ROUTE22_RIVAL_POCKET, .item = "x-special"},
        .expectation = {.used = true, .tried = {6}, .reads = 0},
    },
    {
        .name = "stale counts, the X Special a row lower: read again, then used",
        .input = {.counted = rows_without(ROUTE22_RIVAL_POCKET, "escape-rope"), .bag = ROUTE22_RIVAL_POCKET, .item = "x-special"},
        .expectation = {.used = true, .tried = {5, 6}, .reads = 1},
    },
    {
        .name = "none in the bag after the read: not used",
        .input = {.counted = ROUTE22_RIVAL_POCKET, .bag = rows_without(ROUTE22_RIVAL_POCKET, "x-special"), .item = "x-special"},
        .expectation = {.used = false, .tried = {6}, .reads = 1},
    },
};

struct SupplyLineInput{
    std::vector<BagRow> pocket;
};
struct SupplyLineExpectation{
    std::string line;
};
struct SupplyLineCase{
    std::string name;
    SupplyLineInput input;
    SupplyLineExpectation expectation;
};

class Test_SupplyLine : public UnitTest{
public:
    explicit Test_SupplyLine(SupplyLineCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryBattleBag - supply line: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        Supplies supplies;
        set_from_bag(supplies, m_case.input.pocket);
        const std::string line = supply_line(supplies);
        if (line != m_case.expectation.line){
            return std::format("\"{}\"; expected \"{}\".", line, m_case.expectation.line);
        }
        return true;
    }

private:
    SupplyLineCase m_case;
};

const SupplyLineCase SUPPLY_LINE_CASES[] = {
    {
        .name = "the Route 22 rival's bag lists its Super Potions",
        .input = {.pocket = ROUTE22_RIVAL_POCKET},
        .expectation = {.line = "3 Potions, 6 Super Potions, 1 Antidotes, 2 Parlyz Heals, 0 Awakenings, 0 Full Heals"},
    },
    {
        .name = "a Hyper Potion read from the bag is listed",
        .input = {.pocket = {{"potion", 2}, {"hyper-potion", 4}, {"x-special", 3}}},
        .expectation = {.line = "2 Potions, 0 Super Potions, 4 hyper-potion, 0 Antidotes, 0 Parlyz Heals, 0 Awakenings, 0 Full Heals"},
    },
};

} // namespace


void add_tests_AutoStoryBattleBag(UnitTestDatabase& database){
    for (const BattleItemCase& test_case : BATTLE_ITEM_CASES){
        database.add<Test_UseBattleItem>(test_case);
    }
    for (const SupplyLineCase& test_case : SUPPLY_LINE_CASES){
        database.add<Test_SupplyLine>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

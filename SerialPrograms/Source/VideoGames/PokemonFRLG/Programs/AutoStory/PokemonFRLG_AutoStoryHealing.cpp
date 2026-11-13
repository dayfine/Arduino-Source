/*  AutoStory Healing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

namespace{

constexpr int POTION_HP = 20;
constexpr int SUPER_POTION_HP = 50;
//  The player's HP bar's width in pixels: its fill is read to one.
constexpr int HP_BAR_PIXELS = 48;
//  The most items heal_to_full() uses: more than a full bar takes.
constexpr int MAX_HEAL_ITEMS = 12;

} // namespace


int healing_hp(const std::string& item){
    if (item == "potion"){
        return POTION_HP;
    }
    if (item == "super-potion"){
        return SUPER_POTION_HP;
    }
    return 0;
}

void measured_hp(Supplies& supplies, double fill){
    supplies.measured_fill = fill;
    supplies.healed_since_measured = 0;
}

void took_recovery_item(Supplies& supplies, const std::string& item){
    spend_item(supplies, item);
    if (const int healed = healing_hp(item); healed > 0){
        //  One Potion (20 HP) takes a starter of the early levels from red
        //  to green; the bar can't be read again on the overworld. The HP
        //  is counted against the last measured bar (missing_hp()).
        supplies.hurt = false;
        supplies.hp_red = false;
        supplies.hp_fill = 1.0;
        supplies.healed_since_measured += healed;
    }else{
        supplies.status = StatusCondition::NONE;
    }
}

std::optional<int> missing_hp(const Supplies& supplies, int fallback_max_hp){
    if (!supplies.measured_fill){
        return std::nullopt;
    }
    const int max_hp = supplies.lead.stats && supplies.lead.stats->hp > 0 ? supplies.lead.stats->hp : fallback_max_hp;
    const double fill = *supplies.measured_fill;
    const int missing = fill >= 1.0 ? 0 : (int)std::ceil((1.0 - fill) * max_hp + (double)max_hp / HP_BAR_PIXELS);
    return missing - supplies.healed_since_measured;
}

std::optional<int> heal_to_full(
    Supplies& supplies, int fallback_max_hp, const std::function<bool(const std::string& item)>& use
){
    for (int c = 0; c < MAX_HEAL_ITEMS; c++){
        const std::optional<int> missing = missing_hp(supplies, fallback_max_hp);
        if (!missing || *missing <= 0){
            return missing;
        }
        std::string item;
        if (*missing > POTION_HP && supplies.super_potions > 0){
            item = "super-potion";
        }else if (supplies.potions > 0){
            item = "potion";
        }else if (supplies.super_potions > 0){
            item = "super-potion";
        }else{
            return missing;
        }
        if (!use(item)){
            break;
        }
    }
    return missing_hp(supplies, fallback_max_hp);
}

int hp_cover_shortfall(const Supplies& supplies, int cover_hp, const std::string& item){
    const int held = supplies.potions * POTION_HP + supplies.super_potions * SUPER_POTION_HP;
    const int each = healing_hp(item);
    if (each <= 0 || held >= cover_hp){
        return 0;
    }
    return (cover_hp - held + each - 1) / each;
}

int fill_shortfall(const Supplies& supplies, int target, const std::string& item){
    return std::max(0, target - item_count(supplies, item));
}

bool bag_holds(const Supplies& supplies, const std::vector<Pokemon::StoryItem>& needs, bool any){
    for (const Pokemon::StoryItem& need : needs){
        const bool held = bag_count(supplies, need.item) >= static_cast<int>(need.quantity);
        if (held == any){
            return held;
        }
    }
    return !any;
}


namespace{

//  Blastoise at Lv39 (pokeguy's lead into Koga), its maximum HP known.
constexpr int LEAD_MAX_HP = 122;

Supplies bag(int potions, int super_potions){
    Supplies supplies;
    supplies.potions = potions;
    supplies.super_potions = super_potions;
    supplies.pocket = {"potion", "super-potion"};
    return supplies;
}

//  The fake bag use: every item is used, as the console's
//  use_recovery_item() takes it into the supplies.
bool use_from_bag(Supplies& supplies, const std::string& item){
    took_recovery_item(supplies, item);
    return true;
}

struct HealingCase{
    std::string name;
    std::function<UnitTestResult()> body;
};

class Test_AutoStoryHealing : public UnitTest{
public:
    explicit Test_AutoStoryHealing(HealingCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryHealing - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.body();
    }

private:
    HealingCase m_case;
};

//  Kayden's Hypno leaves the bar at 3/10: Super Potions and a Potion heal
//  it to full (with a bar pixel over), and the check before Koga passes.
UnitTestResult test_hard_hit_healed(){
    Supplies supplies = bag(2, 3);
    measured_hp(supplies, 0.30);
    const std::optional<int> before = missing_hp(supplies, LEAD_MAX_HP);
    if (before != 88){
        return std::format("Missing {} HP at 3/10 of 122, expected 88 (85.4 and a bar pixel).", before.value_or(-1));
    }
    const std::optional<int> after = heal_to_full(supplies, LEAD_MAX_HP, [&](const std::string& item){
        return use_from_bag(supplies, item);
    });
    if (!after || *after > 0){
        return std::format("Still missing {} HP after healing.", after.value_or(-1));
    }
    if (supplies.super_potions != 1 || supplies.potions != 2){
        return std::format("Used {} Super Potions and {} Potions, expected 2 and none.", 3 - supplies.super_potions, 2 - supplies.potions);
    }
    return true;
}

//  Out of Potions and Super Potions: the HP stays missing, which the check
//  before Koga fails on.
UnitTestResult test_out_of_potions(){
    Supplies supplies = bag(1, 0);
    measured_hp(supplies, 0.50);
    const std::optional<int> after = heal_to_full(supplies, LEAD_MAX_HP, [&](const std::string& item){
        return use_from_bag(supplies, item);
    });
    if (!after || *after <= 0){
        return "Healed to full with one Potion from half of 122 HP.";
    }
    return supplies.potions == 0 ? UnitTestResult(true) : UnitTestResult("The Potion wasn't used.");
}

//  A full bar needs nothing; no measurement since the load is unknown; the
//  summary's maximum HP wins over the plan's.
UnitTestResult test_missing_hp_edges(){
    Supplies supplies = bag(0, 0);
    if (missing_hp(supplies, LEAD_MAX_HP)){
        return "A bar never measured gave a missing HP.";
    }
    measured_hp(supplies, 1.0);
    if (missing_hp(supplies, LEAD_MAX_HP) != 0){
        return "A full bar misses HP.";
    }
    measured_hp(supplies, 0.5);
    supplies.lead.stats = Stats{.hp = 100};
    if (missing_hp(supplies, LEAD_MAX_HP) != 53){
        return std::format("Half of a 100 HP bar: {} missing, expected 53.", missing_hp(supplies, LEAD_MAX_HP).value_or(-1));
    }
    return true;
}

//  The Fuchsia Mart buys Super Potions so the bag covers a full bar.
UnitTestResult test_cover_shortfall(){
    struct Case{
        int potions;
        int super_potions;
        int expected;
    };
    const Case cases[] = {
        {0, 0, 3},
        {2, 0, 2},
        {5, 1, 0},
        {0, 3, 0},
    };
    for (const Case& test : cases){
        const int buy = hp_cover_shortfall(bag(test.potions, test.super_potions), LEAD_MAX_HP, "super-potion");
        if (buy != test.expected){
            return std::format("{} Potions, {} Super Potions: buy {}, expected {}.", test.potions, test.super_potions, buy, test.expected);
        }
    }
    return true;
}

} // namespace

void add_tests_AutoStoryHealing(UnitTestDatabase& database){
    database.add<Test_AutoStoryHealing>(HealingCase{"Kayden hits hard: healed to full before Koga", test_hard_hit_healed});
    database.add<Test_AutoStoryHealing>(HealingCase{"out of Potions: the HP stays missing", test_out_of_potions});
    database.add<Test_AutoStoryHealing>(HealingCase{"missing HP: unknown, full, the summary's maximum", test_missing_hp_edges});
    database.add<Test_AutoStoryHealing>(HealingCase{"the Mart buys the shortfall for a full bar", test_cover_shortfall});
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

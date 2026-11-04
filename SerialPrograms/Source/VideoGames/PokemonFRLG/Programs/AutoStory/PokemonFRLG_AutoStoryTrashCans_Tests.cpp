/*  AutoStory Trash Cans Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The trash can search against a fake Vermilion Gym that draws its
 *  switches as SetVermilionTrashCans() does and answers each can with the
 *  pages its message has (PokemonFRLG_AutoStoryTrashCans.h).
 *
 */

#include <algorithm>
#include <format>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

class Test_AutoStoryTrashCans : public UnitTest{
public:
    Test_AutoStoryTrashCans(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStoryTrashCans - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_body();
    }

private:
    std::function<UnitTestResult()> m_body;
};

using Switches = std::pair<int, int>;

//  SetVermilionTrashCans()'s second switch for each first one, as its
//  cases add to the first: 1 +1 or +5; 2-4 +1, +5 or -1; 5 +5 or -1; 6 -5,
//  +1 or +5; 7-9 -5, +1, +5 or -1; 10 -5, +5 or -1; 11 -5 or +1; 12-14 -5,
//  +1 or -1; 15 -5 or -1.
std::vector<int> decomp_offsets(int first){
    switch (first){
    case 1:                     return {+1, +5};
    case 2: case 3: case 4:     return {+1, +5, -1};
    case 5:                     return {+5, -1};
    case 6:                     return {-5, +1, +5};
    case 7: case 8: case 9:     return {-5, +1, +5, -1};
    case 10:                    return {-5, +5, -1};
    case 11:                    return {-5, +1};
    case 12: case 13: case 14:  return {-5, +1, -1};
    default:                    return {-5, -1};
    }
}

Switches draw(std::mt19937& rng){
    const int first = 1 + static_cast<int>(rng() % 15);
    const std::vector<int> offsets = decomp_offsets(first);
    return {first, first + offsets[rng() % offsets.size()]};
}

//  The gym's cans as its scripts answer them.
class FakeGym{
public:
    explicit FakeGym(std::function<Switches()> draw)
        : m_draw(std::move(draw))
        , m_switches(m_draw())
    {}

    CanPages check(int can){
        checked.push_back(can);
        if (can == fail_at){
            return CanPages::FAILED;
        }
        if (open){
            return CanPages::ONE;
        }
        if (!m_first_found){
            m_first_found = can == m_switches.first;
            return m_first_found ? CanPages::TWO : CanPages::ONE;
        }
        if (can == m_switches.second){
            open = true;
            return CanPages::ONE;
        }
        m_first_found = false;
        m_switches = m_draw();
        return CanPages::TWO;
    }

    std::vector<int> checked;
    bool open = false;
    int fail_at = 0;

private:
    std::function<Switches()> m_draw;
    Switches m_switches;
    bool m_first_found = false;
};

//  Draws `script` in order, then the game's draws.
std::function<Switches()> scripted(std::vector<Switches> script, uint32_t seed = 1){
    auto rng = std::make_shared<std::mt19937>(seed);
    auto next = std::make_shared<size_t>(0);
    return [script = std::move(script), rng, next]{
        return *next < script.size() ? script[(*next)++] : draw(*rng);
    };
}

UnitTestResult test_cans_and_neighbors(){
    for (int can = 1; can <= TRASH_CAN_COUNT; can++){
        std::set<int> expected;
        for (int offset : decomp_offsets(can)){
            expected.insert(can + offset);
        }
        const std::vector<int> beside = trash_can_neighbors(can);
        if (std::set<int>(beside.begin(), beside.end()) != expected){
            return std::format("Can {}: the cans beside it differ from SetVermilionTrashCans().", can);
        }
        //  Beside: one tile apart on the map but for the can between.
        const Pokemon::TilePos tile = trash_can_tile(can);
        for (int other : beside){
            const Pokemon::TilePos there = trash_can_tile(other);
            if (std::abs(there.x - tile.x) + std::abs(there.y - tile.y) != 2){
                return std::format("Cans {} and {} aren't two tiles apart.", can, other);
            }
        }
        const TrashCanStand stand = trash_can_stand(can);
        if (stand.tile.next(stand.facing) != tile){
            return std::format("Can {}'s stand doesn't face it.", can);
        }
    }
    if (!(trash_can_tile(1) == Pokemon::TilePos{1, 10}) || !(trash_can_tile(15) == Pokemon::TilePos{9, 14})){
        return "Cans 1 and 15 aren't at (1, 10) and (9, 14), the gym's TrashCan1 and TrashCan15.";
    }
    std::vector<int> order = trash_can_order();
    std::sort(order.begin(), order.end());
    for (int can = 1; can <= TRASH_CAN_COUNT; can++){
        if (order[can - 1] != can){
            return "The search order isn't every can once.";
        }
    }
    return true;
}

//  The guess beside the first switch is right: the search ends there.
UnitTestResult test_right_guess(){
    QuietLogger logger;
    FakeGym gym(scripted({{3, 4}}));
    const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
    const std::vector<int> expected{1, 2, 3, 4};
    if (result.end != TrashCanEnd::OPENED || gym.checked != expected || result.resets != 0 || result.last_can != 4 || !gym.open){
        return std::format("Expected 1, 2, 3 then 4, open, got {} after {} checks.", trash_can_end_name(result.end), result.checks);
    }
    return true;
}

//  The guess is wrong: the locks reset and both switches are drawn again;
//  the search goes on from the can the player stands at, finds the new
//  first switch and guesses again.
UnitTestResult test_wrong_guess_resets(){
    QuietLogger logger;
    //  First under 3, second under 8: the guess, 4, is wrong. Then first
    //  under 4 (where the player stands), second under 5.
    FakeGym gym(scripted({{3, 8}, {4, 5}}));
    const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
    const std::vector<int> expected{1, 2, 3, 4, 4, 5};
    if (result.end != TrashCanEnd::OPENED || gym.checked != expected || result.resets != 1){
        std::string text;
        for (int can : gym.checked){
            text += " " + std::to_string(can);
        }
        return std::format("Expected 1 2 3 4 4 5 with a reset, got{} ({}).", text, trash_can_end_name(result.end));
    }
    //  The new first switch further on: the search walks on from there.
    FakeGym further(scripted({{3, 8}, {13, 14}}));
    const TrashCanResult on = run_trash_cans(logger, [&](int can){ return further.check(can); });
    const std::vector<int> walked{1, 2, 3, 4, 4, 5, 10, 9, 8, 7, 6, 11, 12, 13, 14};
    if (on.end != TrashCanEnd::OPENED || further.checked != walked){
        return "Expected the search to go on from can 4 to 13, then 14.";
    }
    return true;
}

//  Every first switch, with every second beside it: the door opens, the
//  wrong guesses' draws as the game makes them.
UnitTestResult test_every_pair_opens(){
    QuietLogger logger;
    uint32_t seed = 1;
    for (int first = 1; first <= TRASH_CAN_COUNT; first++){
        for (int offset : decomp_offsets(first)){
            FakeGym gym(scripted({{first, first + offset}}, seed++));
            const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
            if (result.end != TrashCanEnd::OPENED || !gym.open){
                return std::format("First {}, second {}: {} after {} checks.", first, first + offset, trash_can_end_name(result.end), result.checks);
            }
        }
    }
    return true;
}

//  10000 gyms as the game draws them: the checks it takes, and how often
//  TRASH_CAN_MAX_CHECKS isn't enough.
UnitTestResult test_many_gyms(){
    QuietLogger logger;
    std::mt19937 rng(7);
    size_t total = 0;
    size_t gave_up = 0;
    size_t most = 0;
    constexpr size_t GYMS = 10000;
    for (size_t c = 0; c < GYMS; c++){
        FakeGym gym([&rng]{ return draw(rng); });
        const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
        gave_up += result.end != TrashCanEnd::OPENED;
        total += result.checks;
        most = std::max(most, result.checks);
    }
    const double mean = static_cast<double>(total) / GYMS;
    if (gave_up > 2 || mean < 15 || mean > 35){
        return std::format("Expected about 25 checks a gym and next to no give-ups, got {:.1f} checks, {} give-ups, at most {}.", mean, gave_up, most);
    }
    return true;
}

UnitTestResult test_failed_check_stops(){
    QuietLogger logger;
    FakeGym gym(scripted({{9, 14}}));
    gym.fail_at = 3;
    const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
    if (result.end != TrashCanEnd::FAILED || result.checks != 3 || gym.checked.back() != 3){
        return "Expected the search to stop at the failed check of can 3.";
    }
    return true;
}

}


void add_tests_AutoStoryTrashCans(UnitTestDatabase& database){
    database.add<Test_AutoStoryTrashCans>("cans, the cans beside each and their stands match the gym", test_cans_and_neighbors);
    database.add<Test_AutoStoryTrashCans>("a right guess opens the door", test_right_guess);
    database.add<Test_AutoStoryTrashCans>("a wrong guess resets, the search goes on from there", test_wrong_guess_resets);
    database.add<Test_AutoStoryTrashCans>("every first and second switch opens the door", test_every_pair_opens);
    database.add<Test_AutoStoryTrashCans>("10000 gyms as the game draws them", test_many_gyms);
    database.add<Test_AutoStoryTrashCans>("a failed check stops the search", test_failed_check_stops);
}



}
}
}

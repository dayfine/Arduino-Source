/*  BDSP AutoStory - Battle Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The battle actions' tests: the move picked from the menu as read, the
 *  Potions, who a gym battle was against and the Trainer Card's badge check.
 *  Screenshots are in the unit test resources (the CommandLineTests
 *  checkout); a test whose screenshot is missing is skipped.
 *
 */

#include <chrono>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "Pokemon/AutoStory/Types/Pokemon_BadgeCheck.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_BagScreen.h"
#include "PokemonBDSP_AutoStory_Battle.h"
#include "PokemonBDSP_AutoStory_BattleMoves.h"
#include "PokemonBDSP_AutoStory_GymOpponent.h"
#include "PokemonBDSP_AutoStory_TalkOver.h"
#include "PokemonBDSP_AutoStory_BattleActions_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;

namespace{


//  The overworld after a won battle, replayed look by look through the
//  talk-over's judge as the console runs it; returns the looks with a box.
size_t replay_after_battle(const std::vector<Pokemon::ScreenEvents>& looks){
    using Pokemon::ScreenEvent;
    TalkOverTally judge;
    WallClock now = WallClock{} + std::chrono::hours(1);
    for (const Pokemon::ScreenEvents& look : looks){
        if (judge.look(look, now) == SceneMove::DONE){
            return judge.box_looks();
        }
        now += TALK_OVER_HOLD + TALK_OVER_RELEASE;
    }
    const Pokemon::ScreenEvents quiet{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL};
    for (size_t i = 0; i < 20; i++){
        if (judge.look(quiet, now) == SceneMove::DONE){
            return judge.box_looks();
        }
        now += TALK_OVER_HOLD + TALK_OVER_RELEASE;
    }
    return judge.box_looks();
}

//  01.04 (lab-B, 2026-11-13 01:17, French): Roark is told from a gym trainer
//  by what happens after the battle, not by reading its intro text. A
//  trainer's battle ends straight on a quiet overworld (22:49, the cleanup
//  over in 7.8 s); Roark talks there for about 40 s (01:16:43 to 01:17:25),
//  with a pause for the badge fanfare.
UnitTestResult test_gym_opponent_from_the_talk_after(Logger&){
    using Pokemon::ScreenEvent;
    using Pokemon::ScreenEvents;
    const ScreenEvents box{ScreenEvent::DIALOG_BOX};
    const ScreenEvents fanfare{ScreenEvent::PLAYER_CAN_MOVE};
    struct Case{ std::string_view name; std::vector<ScreenEvents> looks; GymOpponent opponent; };
    std::vector<ScreenEvents> roark(20, box);
    roark.insert(roark.end(), 3, fanfare);
    roark.insert(roark.end(), 12, box);
    const Case cases[] = {
        {"a trainer: the overworld comes back quiet", {}, GymOpponent::TRAINER},
        {"a trainer: one stray look with a box", {box}, GymOpponent::TRAINER},
        {"Roark: the badge and TM speech, a fanfare pause between", roark, GymOpponent::LEADER},
    };
    for (const Case& c : cases){
        const size_t looks = replay_after_battle(c.looks);
        const GymOpponent opponent = gym_opponent_after_battle(looks);
        if (opponent != c.opponent){
            return std::format("{}: {} look(s) with a box read as {}, not {}.", c.name, looks,
                gym_opponent_name(opponent), gym_opponent_name(c.opponent));
        }
    }
    return true;
}

//  The column's time estimate is a hint only now: it must still count and
//  restart, and a negative walk adds nothing.
UnitTestResult test_gym_column_is_a_hint(Logger&){
    GymColumn column(6, 9, 240ms);
    if (column.leader_here() || column.tiles() != 0){
        return "At the entrance the hint is a trainer's tile.";
    }
    column.walked_north(240ms * 8);
    if (!column.leader_here()){
        return "Past the halfway point to Roark the hint says his tile.";
    }
    column.walked_north(-100ms);
    if (column.tiles() != 8){
        return "A negative walk must not move the hint.";
    }
    column.restart();
    if (column.leader_here()){
        return "After a restart the hint is a trainer's tile.";
    }
    return true;
}

//  Roark, lab-B 2026-11-12 23:49: four Potions at 26%, 26%, 29%, 29%. A Potion
//  that gained nothing ends the Potions of the battle.
UnitTestResult test_potion_ledger_stops_without_gain(Logger&){
    PotionLedger potions;
    potions.observe(0.26);
    if (!potions.may_use()){
        return "The first Potion must be allowed.";
    }
    potions.used(0.26);
    if (!potions.pending()){
        return "A Potion waits for its HP read.";
    }
    potions.observe(-1);
    if (!potions.pending() || potions.stalled()){
        return "An unread bar must not settle a Potion.";
    }
    potions.observe(0.26);
    if (!potions.stalled() || potions.may_use()){
        return "No gain (26% then 26%) must stop the Potions.";
    }
    //  The gain case: a Potion that raised HP allows the next while it is low.
    PotionLedger gaining;
    gaining.used(0.20);
    gaining.observe(0.45);
    if (gaining.stalled() || !gaining.may_use()){
        return "A Potion that gained HP must not stop the Potions.";
    }
    //  At most the cap, and none after the Bag runs out.
    for (size_t n = 1; n < PotionLedger::MAX_POTIONS; n++){
        gaining.used(0.20);
        gaining.observe(0.20 + 0.01 * (double)n);
    }
    if (gaining.may_use()){
        return "No more than the cap of Potions in a battle.";
    }
    PotionLedger empty;
    empty.out_of_potions();
    if (empty.may_use()){
        return "No Potion once the Bag has none.";
    }
    return true;
}

//  The Trainer Card check runs for the levels that check everything and fails
//  on a missing badge and on a card the reader cannot read.
UnitTestResult test_badge_check(Logger&){
    using Pokemon::BadgeReading;
    using Pokemon::StoryCheckLevel;
    for (StoryCheckLevel level : {StoryCheckLevel::TESTING, StoryCheckLevel::THOROUGH}){
        if (!Pokemon::badge_check_wanted(level)){
            return "A thorough run must check the badge.";
        }
    }
    for (StoryCheckLevel level : {StoryCheckLevel::DEFAULT, StoryCheckLevel::FAST, StoryCheckLevel::SPEEDRUN}){
        if (Pokemon::badge_check_wanted(level)){
            return "A run below thorough must not open the Trainer Card.";
        }
    }
    if (Pokemon::badge_check_failure(BadgeReading::MISSING, "Coal Badge").find("Coal Badge") == std::string::npos){
        return "A missing badge must fail and name the badge.";
    }
    if (!Pokemon::badge_check_failure(BadgeReading::PRESENT, "Coal Badge").empty()){
        return "A present badge must not fail.";
    }
    if (Pokemon::badge_check_failure(BadgeReading::UNREADABLE, "Coal Badge").find("Coal Badge") == std::string::npos){
        return "An unreadable card must fail and name the badge.";
    }
    return true;
}

//  A menu read on an empty image, or with buttons outside the frame, reads
//  nothing instead of crashing (SIGSEGV in gray_box, 2026-11-12).
UnitTestResult test_move_menu_unreadable(Logger&){
    BattleMoveMenuState moves;
    moves.move_count = 2;
    moves.buttons[0] = ImageFloatBox{0.5, 0.5, 0.4, 0.1};
    moves.buttons[1] = ImageFloatBox{1.5, 1.5, 0.4, 0.1};
    const ImageRGB32 frame(64, 36);
    const ImageViewRGB32 inputs[] = {ImageViewRGB32(), frame};
    for (const ImageViewRGB32& screen : inputs){
        const std::vector<MoveSlot> slots = read_move_menu(screen, moves);
        if (slots.size() != 2){
            return "read_move_menu returned " + std::to_string(slots.size()) + " slots, expected 2.";
        }
        if (!slots[1].slug.empty() || slots[1].pp != -1){
            return "a button outside the frame read a move.";
        }
    }
    return true;
}

//  The move picked from names and PP as read. lab-B 2026-11-09, French:
//  no name read, Piplup's Pound at 35 PP and Growl at 22 PP, and the old
//  "newest move that is not a status move" fallback used Growl until the
//  training gave up after 25 turns.
UnitTestResult test_choose_move(Logger&){
    struct Case{ std::string_view name; std::vector<MoveSlot> slots; size_t best; };
    const Case cases[] = {
        {"no name read, PP 35 and 22", {{"", 35}, {"", 22}}, 1},
        {"no name read, PP 35 and 30", {{"", 35}, {"", 30}}, 1},
        {"no name or PP read", {{"", -1}, {"", -1}}, 1},
        {"Scratch and Leer", {{"scratch", 35}, {"leer", 30}}, 1},
        {"Pound, Growl, Water Gun and Charm", {{"pound", 35}, {"growl", 40}, {"water-gun", 10}, {"charm", 20}}, 3},
        {"Water Gun out of PP", {{"pound", 35}, {"growl", 40}, {"water-gun", 0}, {"charm", 20}}, 1},
        {"slot 1 unread at 38 PP: a status move", {{"", 38}, {"", 40}, {"", 20}}, 3},
        {"Growl unread, Tackle read", {{"", 40}, {"tackle", 35}}, 2},
    };
    for (const Case& c : cases){
        const size_t best = choose_move(c.slots);
        if (best != c.best){
            return std::string(c.name) + ": picked move " + std::to_string(best) + ", expected " + std::to_string(c.best) + ".";
        }
    }
    return true;
}

//  Move menus saved on lab-B (SCREENSHOTS_PATH()/AutoStory/moves/), read in
//  the game's language. The French ones read nothing before the names came
//  from the move name dictionary.
UnitTestResult test_move_menu_screenshots(Logger& logger){
    struct Case{
        std::string file;
        Language language;
        std::vector<MoveSlot> slots;
        size_t best;
    };
    const Case cases[] = {
        {"PokemonBDSP/AutoStory/MoveMenuFrenchPoundGrowl-20261109-192734.png", Language::French,
            {{"pound", 35}, {"growl", 40}}, 1},
        {"PokemonBDSP/AutoStory/MoveMenuFrenchPoundGrowl16-20261109-193035.png", Language::French,
            {{"pound", 35}, {"growl", 16}}, 1},
        {"PokemonBDSP/AutoStory/MoveMenuEnglishPiplup-20261108-181435.png", Language::English,
            {{"pound", 35}, {"growl", 40}, {"water-gun", 10}, {"charm", 20}}, 3},
    };
    const Language story = story_language();
    for (const Case& c : cases){
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
        }
        const ImageRGB32 screen(path);
        set_story_language(c.language);
        const std::vector<MoveSlot> slots = read_move_menu(screen, detect_battle_move_menu(screen));
        set_story_language(story);
        std::string read;
        for (size_t i = 0; i < slots.size(); i++){
            read += " [" + slots[i].describe(i) + "]";
        }
        logger.log(std::string(c.file) + ":" + read);
        if (slots.size() != c.slots.size()){
            return std::string(c.file) + ": " + std::to_string(slots.size()) + " moves, expected " + std::to_string(c.slots.size()) + ".";
        }
        for (size_t i = 0; i < slots.size(); i++){
            if (slots[i].slug != c.slots[i].slug || slots[i].pp != c.slots[i].pp){
                return std::string(c.file) + ": move " + std::to_string(i + 1) + " read as" + read + ".";
            }
        }
        if (choose_move(slots) != c.best){
            return std::string(c.file) + ": picked move " + std::to_string(choose_move(slots)) + ", expected " + std::to_string(c.best) + ".";
        }
    }
    return true;
}


//  The Potion after a fight reads the lead's HP when the Bag opens and goes
//  on to the pocket and the list only if a dose is needed (lab-B 2026-11-12:
//  it found the Potion, opened the party, read 80% and "no Potion needed").
UnitTestResult test_potion_need(Logger&){
    struct Case{ std::string_view name; double hp; size_t dose; PotionNeed need; };
    const Case cases[] = {
        {"full HP", 1.0, 0, PotionNeed::FULL},
        {"80% on the first dose with the check at 80%", 0.80, 0, PotionNeed::ENOUGH},
        {"a bit over the check", 0.90, 0, PotionNeed::ENOUGH},
        {"under the check", 0.55, 0, PotionNeed::NEEDED},
        {"the second dose heals on until full", 0.85, 1, PotionNeed::NEEDED},
        {"fainted", 0.0, 0, PotionNeed::FAINTED},
    };
    for (const Case& c : cases){
        if (potion_need(c.hp, c.dose, 0.8) != c.need){
            return std::string(c.name) + ": wrong verdict.";
        }
    }
    return true;
}

struct BattleActionsInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct BattleActionsExpectation{
};

struct BattleActionsCase{
    BattleActionsInput input;
    BattleActionsExpectation expectation;
};

class Test_AutoStoryBattleActions : public UnitTest{
public:
    explicit Test_AutoStoryBattleActions(BattleActionsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    BattleActionsCase m_case;
};

} // namespace


const BattleActionsCase BATTLE_ACTIONS_CASES[] = {
    {
        .input = {"Potion after a fight: the lead's HP decides before the Bag is searched", test_potion_need},
        .expectation = {},
    },
    {
        .input = {"battle moves: the pick from names and PP", test_choose_move},
        .expectation = {},
    },
    {
        .input = {"battle moves: unreadable menu reads nothing", test_move_menu_unreadable},
        .expectation = {},
    },
    {
        .input = {"01.04 gym: Roark is told by the talk on the overworld after the battle, in any language (lab-B 2026-11-13)", test_gym_opponent_from_the_talk_after},
        .expectation = {},
    },
    {
        .input = {"01.04 gym: the column's tile estimate is a hint only", test_gym_column_is_a_hint},
        .expectation = {},
    },
    {
        .input = {"Battle: a Potion that gained no HP ends the Potions of the battle (Roark, lab-B 2026-11-12 23:49)", test_potion_ledger_stops_without_gain},
        .expectation = {},
    },
    {
        .input = {"badge check: thorough levels open the Trainer Card and a missing badge fails", test_badge_check},
        .expectation = {},
    },
    {
        .input = {"battle moves: move menu screenshots in each language", test_move_menu_screenshots},
        .expectation = {},
    },
};

void add_tests_AutoStory_BattleActions(UnitTestDatabase& database){
    for (const auto& item : BATTLE_ACTIONS_CASES){
        database.add<Test_AutoStoryBattleActions>(item);
    }
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

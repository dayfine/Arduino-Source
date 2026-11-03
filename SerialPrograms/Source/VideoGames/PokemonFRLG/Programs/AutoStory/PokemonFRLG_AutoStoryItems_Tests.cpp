/*  AutoStory Items Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart cursor's moves, and the bag's rows as the story counts them.
 *
 */

#include <cmath>
#include <format>
#include <functional>
#include <string_view>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Language.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SaveCardReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class Test_AutoStoryItems : public UnitTest{
public:
    Test_AutoStoryItems(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonFRLG::AutoStoryItems - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};


//  The orders the story gives the Viridian Mart: rows 1, 2, 3 in turn. The
//  cursor stays on the row bought, so each is one row down from the last.
UnitTestResult test_mart_cursor(Logger&){
    if (mart_cursor_moves(0, 1) != 1 || mart_cursor_moves(1, 2) != 1 || mart_cursor_moves(2, 3) != 1){
        return "Expected one press down between the story's orders.";
    }
    if (mart_cursor_moves(3, 1) != -2 || mart_cursor_moves(2, 2) != 0){
        return "Expected presses up to a row above, none to the same row.";
    }
    int cursor = 0;
    std::vector<int> bought;
    for (int row : {1, 2, 3}){
        cursor += mart_cursor_moves(cursor, row);
        bought.push_back(cursor);
    }
    if (bought != std::vector<int>{1, 2, 3}){
        return "The cursor didn't land on rows 1, 2, 3.";
    }
    return true;
}

//  Rows skip items with none left; the order read from the bag replaces
//  the order bought.
UnitTestResult test_pocket_rows(Logger&){
    Supplies supplies;
    supplies.potions = 5;
    supplies.antidotes = 3;
    supplies.parlyz_heals = 1;
    if (item_row(supplies, "potion") != 0 || item_row(supplies, "antidote") != 1 || item_row(supplies, "parlyz-heal") != 2){
        return "Wrong rows for the order bought.";
    }
    supplies.potions = 0;
    if (item_row(supplies, "potion") != -1 || item_row(supplies, "antidote") != 0){
        return "An item with none left still takes a row.";
    }
    set_from_bag(supplies, {{"potion", 4}, {"parlyz-heal", 3}});
    if (supplies.potions != 4 || supplies.antidotes != 0 || supplies.parlyz_heals != 3){
        return "Counts not taken from the bag.";
    }
    if (item_row(supplies, "parlyz-heal") != 1 || item_row(supplies, "antidote") != -1){
        return "Rows not taken from the bag.";
    }
    return true;
}

//  #211: a row loaded from a save counted no Potions, so its wild battles
//  were all run from. The counts come from the bag as read, both for the
//  run and for a reload; with nothing read, the row's are kept.
UnitTestResult test_loaded_bag(Logger&){
    Supplies row;
    Supplies saved = row;
    const std::vector<std::string> unread = set_from_loaded_bag(row, saved, {{"potion", 4}, {"antidote", 2}});
    if (row.potions != 4 || saved.potions != 4 || row.antidotes != 2 || saved.antidotes != 2 || !unread.empty()){
        return std::format(
            "Expected 4 Potions and 2 Antidotes from the bag, now and for a reload; got {} and {}, {} and {}.",
            row.potions, saved.potions, row.antidotes, saved.antidotes
        );
    }
    if (item_row(saved, "antidote") != 1){
        return "Expected the reload's pocket order from the bag.";
    }

    Supplies known;
    known.potions = 2;
    Supplies known_saved = known;
    set_from_loaded_bag(known, known_saved, {});
    if (known.potions != 2 || known_saved.potions != 2){
        return "Expected the row's 2 Potions kept with nothing read.";
    }
    return true;
}

//  A count the bag reader couldn't read (-1) is never taken for none: the
//  row still holds some, so it keeps the count bought, or 1 with none
//  known, and the battle's checks (a count above 0, a row) still use it.
UnitTestResult test_unread_counts(Logger&){
    Supplies bought;
    bought.potions = 5;
    bought.antidotes = 3;
    bought.parlyz_heals = 1;
    const std::vector<BagRow> rows{{"potion", -1}, {"antidote", -1}, {"parlyz-heal", 1}};
    const std::vector<std::string> unread = set_from_bag(bought, rows);
    if (unread != std::vector<std::string>{"potion", "antidote"}){
        return "Expected the Potion and Antidote counts reported unread.";
    }
    if (bought.potions != 5 || bought.antidotes != 3 || bought.parlyz_heals != 1){
        return std::format(
            "Expected 5 Potions, 3 Antidotes, 1 Parlyz Heal kept from the order, got {}, {}, {}.",
            bought.potions, bought.antidotes, bought.parlyz_heals
        );
    }

    Supplies unknown;
    unknown.potions = 0;
    unknown.antidotes = 0;
    unknown.parlyz_heals = 0;
    set_from_bag(unknown, rows);
    if (item_count(unknown, "potion") <= 0 || item_count(unknown, "antidote") <= 0){
        return std::format(
            "Expected unread counts with none known to count as some, got {} Potions, {} Antidotes.",
            unknown.potions, unknown.antidotes
        );
    }
    if (item_row(unknown, "potion") != 0 || item_row(unknown, "antidote") != 1 || item_row(unknown, "parlyz-heal") != 2){
        return std::format(
            "Expected rows 0, 1, 2 with unread counts, got {}, {}, {}.",
            item_row(unknown, "potion"), item_row(unknown, "antidote"), item_row(unknown, "parlyz-heal")
        );
    }
    return true;
}

//  In a language the reader has no names for, nothing is read, so the
//  checks fall back to positions instead of failing.
UnitTestResult test_unread_language(Logger& logger){
    const ImageRGB32 screen(1920, 1080);
    if (!read_item_list_name(logger, Language::Japanese, screen, 0).empty()){
        return "Read an item name in Japanese.";
    }
    if (!read_bag_pocket(logger, Language::Japanese, screen).empty()){
        return "Read bag rows in Japanese.";
    }
    if (!read_save_card_location(logger, Language::Japanese, screen).empty()){
        return "Read a save card's place in Japanese.";
    }
    return true;
}


//  battle_shows(), the battle loop's look at whether the battle is still on
//  screen, on console B's captures.
class Test_BattleShows : public ImageUnitTest{
public:
    Test_BattleShows(const std::string& image, bool expected)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
        , m_expected(expected)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        return battle_shows(images[0]) == m_expected;
    }

private:
    bool m_expected;
};

//  read_party_status() on console B's captures: the bar's color, and its
//  colored share within a pixel of the 48-pixel bar of `hp` / `max_hp`.
//  Console B, 2026-11-03, before Miguel: 17/55 HP read as 15% of the bar,
//  then 2% with the box a pixel lower.
class Test_PartyHp : public ImageUnitTest{
public:
    Test_PartyHp(const std::string& image, std::string_view color, int hp, int max_hp)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
        , m_color(color)
        , m_share(double(hp) / max_hp)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const PartyStatus status = read_party_status(images[0]);
        const std::string_view color = !status.hp_seen ? "none" : status.hp_red ? "red" : status.hp_yellow ? "yellow" : "green";
        if (color != m_color){
            return std::format("Expected an HP bar read as {}, got {}.", m_color, color);
        }
        if (std::abs(status.hp_fill - m_share) > 1.0 / 48){
            return std::format("Expected {:.0f}% of the bar, got {:.0f}%.", 100 * m_share, 100 * status.hp_fill);
        }
        return true;
    }

private:
    std::string_view m_color;
    double m_share;
};


}


void add_tests_AutoStoryItems(UnitTestDatabase& database){
    database.add<Test_AutoStoryItems>("Mart cursor stays on the row bought", test_mart_cursor);
    database.add<Test_AutoStoryItems>("Items pocket rows", test_pocket_rows);
    database.add<Test_AutoStoryItems>("a loaded save's counts come from the bag", test_loaded_bag);
    database.add<Test_AutoStoryItems>("unread bag counts count as some", test_unread_counts);
    database.add<Test_AutoStoryItems>("no item names read in a language without them", test_unread_language);
    //  "Wild CHANSEY appeared!" with its HP box, and Mt. Moon B2F's corridor
    //  after "Got away safely!", whose black lower half reads as the battle's
    //  text box (console B, 2026-11-03: 9 minutes of A and B there).
    database.add<Test_BattleShows>("PokemonFRLG/WildEncounterReader/eng_chansey.jpg", true);
    database.add<Test_BattleShows>("PokemonFRLG/AutoStoryBattle/MtMoonB2F_AfterRun_False.png", false);
    database.add<Test_PartyHp>("PokemonFRLG/AutoStoryBattle/MtMoon1F_HpGreen_Full.png", "green", 45, 45);
    database.add<Test_PartyHp>("PokemonFRLG/AutoStoryBattle/MtMoon1F_HpGreen_27of45.png", "green", 27, 45);
    database.add<Test_PartyHp>("PokemonFRLG/AutoStoryBattle/MtMoonB2F_HpYellow_17of55.png", "yellow", 17, 55);
    database.add<Test_PartyHp>("PokemonFRLG/AutoStoryBattle/MtMoonB2F_HpYellow_17of55_BarLow.png", "yellow", 17, 55);
    //  The battle's choices, registered here to keep add_tests() within its size.
    add_tests_AutoStoryParty(database);
}



}
}
}

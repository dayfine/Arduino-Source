/*  AutoStory Items Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart cursor's moves, the bag's rows as the story counts them, and
 *  the bag read off a capture: the one that showed the Mart had bought
 *  Parlyz Heals instead of Antidotes (console B, 2026-11-01).
 *
 */

#include <filesystem>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"

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


//  In the CommandLineTests repo, like the other image tests. Skipped while
//  it isn't there.
const char* const BAG_CAPTURE = "PokemonFRLG/AutoStory/BagItems_Potion4_ParlyzHeal3.png";

UnitTestResult test_bag_capture(Logger& logger){
    const std::string path = UNIT_TEST_RESOURCE_PATH() + BAG_CAPTURE;
    if (!std::filesystem::exists(path)){
        return UnitTestResult(UnitTestResult::SKIPPED, "Capture not found: " + path);
    }
    const ImageRGB32 screen(path);
    const std::vector<BagRow> rows = read_bag_pocket(logger, screen);
    if (rows.size() != 2 || rows[0].item != "potion" || rows[0].count != 4 || rows[1].item != "parlyz-heal" || rows[1].count != 3){
        std::string text;
        for (const BagRow& row : rows){
            text += std::format(" {} x{};", row.item, row.count);
        }
        return "Expected potion x4; parlyz-heal x3, read:" + text;
    }
    //  The name check before using an item: row 1 passes as a Potion, row 2
    //  fails as an Antidote.
    if (read_item_list_name(logger, screen, 0) != "potion"){
        return "Row 1 doesn't read as a Potion.";
    }
    if (read_item_list_name(logger, screen, 1) == "antidote"){
        return "Row 2 reads as an Antidote.";
    }
    if (read_item_list_name(logger, screen, 2) != "cancel"){
        return "Row 3 doesn't read as CANCEL.";
    }
    return true;
}

}


void add_tests_AutoStoryItems(UnitTestDatabase& database){
    database.add<Test_AutoStoryItems>("Mart cursor stays on the row bought", test_mart_cursor);
    database.add<Test_AutoStoryItems>("Items pocket rows", test_pocket_rows);
    database.add<Test_AutoStoryItems>("Items pocket read off a capture", test_bag_capture);
}



}
}
}

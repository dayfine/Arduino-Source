/*  AutoStory Items Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Mart cursor's moves, the bag's rows as the story counts them, and
 *  where the story's wild battle seen late places a stopped leg.
 *
 */

#include <cctype>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Language.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleHpBarReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SaveCardReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleDecisions.h"
#include "PokemonFRLG_AutoStoryItemUse.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryNewMembers.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryViridianMart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryItemsInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct AutoStoryItemsExpectation{
    std::string_view description;
};

struct AutoStoryItemsTestCase{
    AutoStoryItemsInput input;
    AutoStoryItemsExpectation expectation;
};

class Test_AutoStoryItems : public UnitTest{
public:
    explicit Test_AutoStoryItems(AutoStoryItemsTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStoryItems - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_scenario.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryItemsTestCase m_scenario;
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

//  Console B, 2026-11-03: a save from Mt. Moon has a MOON STONE above the
//  PARLYZ HEAL and 15 POTIONs. An item the story doesn't count still takes
//  its row, so the battle knows all 15 Potions and finds them in row 2.
UnitTestResult test_loaded_bag_other_items(Logger&){
    Supplies row;
    row.potions = 0;
    row.parlyz_heals = 0;
    Supplies saved = row;
    set_from_loaded_bag(row, saved, {{"", 1}, {"parlyz-heal", 1}, {"potion", 15}});
    if (row.potions != 15 || saved.potions != 15 || row.parlyz_heals != 1){
        return std::format(
            "Expected 15 Potions and 1 Parlyz Heal from the bag, now and for a reload; got {}, {} and {}.",
            row.potions, saved.potions, row.parlyz_heals
        );
    }
    if (item_row(row, "parlyz-heal") != 1 || item_row(row, "potion") != 2 || item_row(saved, "potion") != 2){
        return std::format(
            "Expected the Parlyz Heal in row 1 and the Potions in row 2 below the Moon Stone, got {} and {}.",
            item_row(row, "parlyz-heal"), item_row(row, "potion")
        );
    }
    return true;
}

//  Awakenings, bought at Cerulean after a MOON STONE took its row: counted
//  from the bag and found in their row.
UnitTestResult test_loaded_bag_awakenings(Logger&){
    Supplies row;
    Supplies saved = row;
    set_from_loaded_bag(row, saved, {{"", 1}, {"parlyz-heal", 1}, {"potion", 15}, {"awakening", 3}});
    if (row.awakenings != 3 || saved.awakenings != 3 || item_count(row, "awakening") != 3 || item_row(row, "awakening") != 3){
        return std::format(
            "Expected 3 Awakenings in row 3, now and for a reload; got {} and {}, row {}.",
            row.awakenings, saved.awakenings, item_row(row, "awakening")
        );
    }
    return true;
}

//  The status badge's mean color on console B's captures: none and PSN
//  (2026-11-01), and SLP after the Cerulean rival's Sleep Powder
//  (2026-11-03, m56 run 2, turns 6 and 7 at 34/65 and 13/65 HP), which
//  read as "a status".
UnitTestResult test_status_badge(Logger&){
    const std::vector<std::pair<uint32_t, StatusCondition>> cases = {
        {combine_rgb(255, 255, 217), StatusCondition::NONE},
        {combine_rgb(228, 168, 212), StatusCondition::POISON},
        {combine_rgb(202, 203, 168), StatusCondition::SLEEP},
    };
    for (const auto& [color, expected] : cases){
        ImageRGB32 badge(12, 4);
        badge.fill(color);
        const StatusCondition read = read_status_badge(badge);
        if (read != expected){
            return std::format("Badge 0x{:08x}: expected {}, got {}.", color, status_name(expected), status_name(read));
        }
    }
    return true;
}

//  The pocket's rows as read: an unread name keeps its row, so the rows
//  below it are still read; the rows end at CANCEL, or at the last name read.
UnitTestResult test_pocket_rows_unread(Logger&){
    const std::vector<BagRow> rows = pocket_rows({{"", 1}, {"parlyz-heal", 1}, {"potion", 15}, {"cancel", -1}, {"", -1}});
    if (rows.size() != 3 || !rows[0].item.empty() || rows[2].item != "potion" || rows[2].count != 15){
        return std::format("Expected 3 rows, the Potions' last with 15, got {}.", rows.size());
    }
    if (pocket_rows({{"potion", 3}, {"", -1}, {"", -1}}).size() != 1){
        return "Expected the unread rows after the last name dropped.";
    }
    if (!pocket_rows({{"", 1}, {"", 2}}).empty()){
        return "Expected nothing with no name read.";
    }
    return true;
}

//  Rows past the first six show where the list has scrolled to: the
//  cursor stops at the fourth row shown, until the CANCEL shows. The
//  Tower's bag (console B, 2026-11-12): its X Specials 7th of 8 rows and
//  the CANCEL; then the Potions run out and take no row.
UnitTestResult test_item_shown_row(Logger&){
    Supplies supplies;
    set_from_bag(supplies, {
        {"moon-stone", 2}, {"potion", 1}, {"nugget", 1}, {"repel", 5},
        {"escape-rope", 5}, {"super-repel", 7}, {"x-special", 2}, {"x-speed", 1},
    });
    const std::vector<std::pair<int, int>> full = {{2, 2}, {3, 3}, {4, 3}, {6, 3}, {7, 4}, {8, 5}};
    for (const auto& [row, shown] : full){
        if (item_shown_row(supplies, row) != shown){
            return std::format("Row {} of 9: expected shown in row {}, got {}.", row, shown, item_shown_row(supplies, row));
        }
    }
    spend_item(supplies, "potion");
    if (item_row(supplies, "x-special") != 5 || item_shown_row(supplies, 5) != 3 || item_shown_row(supplies, 7) != 5){
        return "Expected the X Specials in row 6 of 8 shown 4th, the CANCEL last, with no Potions left.";
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

//  The step's arg `key`, if it has one of type `Type`.
template <typename Type>
const Type* step_arg(const StoryStep& step, std::string_view key){
    const auto found = step.args.find(key);
    return found == step.args.end() ? nullptr : std::get_if<Type>(&found->second);
}

//  The items a step finds in the bag or in a Mart's list by name: those it
//  buys, uses or counts. Nothing for a step that reads no item's name.
std::vector<std::string> items_read_by(const StoryStep& step){
    std::vector<std::string> items;
    if (step.kind == StoryStepKind::SHOP){
        if (const auto* lines = step_arg<std::vector<Pokemon::StoryItem>>(step, "items")){
            for (const MartOrder& order : viridian_mart_orders(*lines).value_or(std::vector<MartOrder>())){
                items.emplace_back(order.item);
            }
        }
    }else if (step.action == MART_ACTION){
        if (const std::optional<MartPlan> plan = mart_plan_of(step.args)){
            for (const MartLine& line : plan->lines){
                items.push_back(line.item);
            }
        }
    }else if (step.action == USE_ITEM_VARIANT){
        if (const auto* item = step_arg<std::string>(step, "item")){
            items.push_back(*item);
        }
    }else if (step.action == COUNT_ITEMS_VARIANT){
        if (const auto* counted = step_arg<std::vector<std::string>>(step, "items")){
            items.insert(items.end(), counted->begin(), counted->end());
        }
    }
    if (const auto* used = step_arg<std::vector<Pokemon::StoryItem>>(step, "use_items")){
        for (const Pokemon::StoryItem& item : *used){
            items.push_back(item.item);
        }
    }
    return items;
}

//  11.03 counted its FULL RESTOREs with a reader that had no name for
//  them: the row read as nothing, and the count was 0 after the Safari
//  Zone's ball was picked up (console B, 2026-11-12). Every item a plan
//  buys, uses or counts has a name in the reader, and so do the MOON STONE
//  and the NUGGET, which sit above the story's items in the pocket.
UnitTestResult test_plan_items_readable(Logger&){
    std::string missing;
    for (const std::string_view item : {"moon-stone", "nugget"}){
        if (!item_list_reader_knows(item)){
            missing += std::format("\n    the bag: {}", item);
        }
    }
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            for (const std::string& item : items_read_by(step)){
                if (!item_list_reader_knows(item)){
                    missing += std::format("\n    {}/{}: {}", checkpoint.id, step.name, item);
                }
            }
        }
    }
    if (!missing.empty()){
        return "The item list reader has no name for:" + missing;
    }
    return true;
}

//  The open leg through Route 1's grass on the way from Viridian Mart to
//  Oak's Lab ("south 20"), and the tile it starts from.
struct GrassLeg{
    MapPosition start;
    RouteStep step;
};
std::optional<GrassLeg> route1_grass_leg(const std::vector<MapData>& maps){
    const MapPosition mart{"ViridianCity_Mart", {4, 3}};
    const RouteGoal lab{MapPosition{"PalletTown_ProfessorOaksLab", {6, 11}}, std::nullopt};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(maps, mart, lab, default_route_costs(), Direction::WEST);
    if (!route){
        return std::nullopt;
    }
    MapPosition start = mart;
    for (const RouteStep& step : *route){
        if (step.action == RouteAction::WALK && step.wild && !step.to_wall && !step.warps && step.tiles >= 16){
            return GrassLeg{start, step};
        }
        start = step.end;
    }
    return std::nullopt;
}

//  How many tiles of `leg` the player walked to `at`, or -1 if `at` is off it.
int tiles_walked(const std::vector<MapData>& maps, const GrassLeg& leg, const MapPosition& at){
    const std::vector<MapPosition> tiles = leg_tiles(maps, leg.start, leg.step);
    for (size_t c = 0; c < tiles.size(); c++){
        if (tiles[c] == at){
            return (int)c;
        }
    }
    return -1;
}

//  r16 and r17 (console B, 2026-11-01), Route 1: a wild battle on the first
//  grass step of "south 20" whose flashes were missed shows "Wild <Pokemon>
//  appeared!" 3.7 s after the step's press. The story's interrupt for it
//  must place the stop where the battle stopped the player, as the walker's
//  does: with 2 s it put them about 6 tiles further on.
UnitTestResult test_late_battle_placed(Logger&){
    const std::vector<MapData> maps = overworld_maps();
    const std::optional<GrassLeg> leg = route1_grass_leg(maps);
    if (!leg){
        return "Expected an open leg of 16 tiles or more through Route 1's grass from Viridian Mart to Oak's Lab.";
    }
    SimScript script;
    script.encounters = {{.grass_step = 1, .seen_late = true}};
    OverworldSim sim(maps, leg->start, leg->step.direction, script);
    Supplies supplies;
    const OverworldInterruptSet story = story_interrupts(supplies, false, Pokemon::StoryCheckLevel::FAST, Language::English);
    OverworldSimWalker walker(sim, {}, story);
    const StepOutcome outcome = walker.calls().walk(leg->step, leg->step.direction);
    const int walked = tiles_walked(maps, *leg, sim.state().position);
    if (outcome.end != StepEnd::STOPPED || !outcome.walked || walked < outcome.walked->min || outcome.walked->max < walked){
        return std::format(
            "Expected \"{}\" from {} stopped and placed on tile {} ({}), where the battle stopped the player; placed on tiles {} to {}.",
            to_string(leg->step), to_string(leg->start), walked, to_string(sim.state().position),
            outcome.walked ? outcome.walked->min : -1, outcome.walked ? outcome.walked->max : -1
        );
    }
    return true;
}


//  battle_shows(), the battle loop's look at whether the battle is still on
//  screen, on console B's captures named <anything>_<True or False>.png.
//  An image whose name doesn't say is registered with its answer.
class Test_BattleShows : public ImageUnitTest{
public:
    Test_BattleShows(const std::string& image, std::optional<bool> expected = std::nullopt)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
        , m_expected(expected)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        bool expected = false;
        if (m_expected){
            expected = *m_expected;
        }else{
            const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
            if (words.empty() || !parse_bool(words.back(), expected)){
                return "Error: filename must be <anything>_<True or False>.";
            }
        }
        return battle_shows(images[0]) == expected;
    }

private:
    std::optional<bool> m_expected;
};

//  battle_yes_no_shows() on console B's captures named
//  <anything>_<True or False>.png.
class Test_BattleYesNo : public ImageUnitTest{
public:
    Test_BattleYesNo(const std::string& image)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        bool expected = false;
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        if (words.empty() || !parse_bool(words.back(), expected)){
            return "Error: filename must be <anything>_<True or False>.";
        }
        return battle_yes_no_shows(images[0]) == expected;
    }
};

//  read_opponent_hp_fill() on console B's captures named
//  <anything>_FoeHp_<pixels>of48.png: the bar's filled share within a pixel.
class Test_FoeHp : public ImageUnitTest{
public:
    Test_FoeHp(const std::string& image)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        int pixels = -1;
        if (words.empty() || words.back().find("of48") == std::string::npos || !parse_int(words.back().substr(0, words.back().find("of48")), pixels)){
            return "Error: filename must be <anything>_FoeHp_<pixels>of48.";
        }
        const std::optional<double> fill = read_opponent_hp_fill(images[0]);
        if (!fill || std::abs(*fill - pixels / 48.0) > 1.0 / 48){
            return std::format("Expected {} of 48 pixels, got {:.1f}.", pixels, fill ? 48 * *fill : -1.0);
        }
        return true;
    }
};

//  read_party_status() on console B's captures named
//  <anything>_Hp<Color>_<hp>of<max hp>[_<anything>].png: the bar's color,
//  and its colored share within a pixel of the 48-pixel bar of hp / max hp.
//  Console B, 2026-11-03, before Miguel: 17/55 HP read as 15% of the bar,
//  then 2% with the box a pixel lower.
class Test_PartyHp : public ImageUnitTest{
public:
    Test_PartyHp(const std::string& image)
        : ImageUnitTest("PokemonFRLG::AutoStoryBattle - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        std::string expected_color;
        int hp = 0;
        int max_hp = 0;
        for (const std::string& word : parse_words(Filesystem::Path(image_paths()[0]).stem().string())){
            const size_t of = word.find("of");
            if (word.starts_with("Hp") && word.size() > 2){
                expected_color = word.substr(2);
                expected_color[0] = char(std::tolower((unsigned char)expected_color[0]));
            }else if (of != std::string::npos){
                parse_int(word.substr(0, of), hp);
                parse_int(word.substr(of + 2), max_hp);
            }
        }
        if (expected_color.empty() || max_hp <= 0){
            return "Error: filename must be <anything>_Hp<Color>_<hp>of<max hp>[_<anything>].";
        }
        const double share = double(hp) / max_hp;
        const PartyStatus status = read_party_status(images[0]);
        const std::string_view color = !status.hp_seen ? "none" : status.hp_red ? "red" : status.hp_yellow ? "yellow" : "green";
        if (color != expected_color){
            return std::format("Expected an HP bar read as {}, got {}.", expected_color, color);
        }
        if (std::abs(status.hp_fill - share) > 1.0 / 48){
            return std::format("Expected {:.0f}% of the bar, got {:.0f}%.", 100 * share, 100 * status.hp_fill);
        }
        return true;
    }
};


//  Regression, console B 2026-11-11, 09.02 (Rocket Hideout): a Grunt's
//  battle ended with the starter poisoned at 21% HP and no Antidote, and the
//  story walked on poisoned into the next battle. After a battle: an
//  Antidote, else a Full Heal; a Potion for HP not green, or for poison
//  nothing cured once the bar is under 3/4; the nurse for what's left.
UnitTestResult test_poison_cured_after_battle(Logger&){
    Supplies supplies;
    supplies.status = StatusCondition::POISON;
    supplies.hp_fill = 0.21;
    supplies.hurt = true;
    supplies.hp_red = true;
    supplies.potions = 14;
    supplies.antidotes = 1;
    supplies.full_heals = 1;
    if (recovery_items(supplies) != std::vector<std::string>{"antidote", "potion"}){
        return "Expected an Antidote, then a Potion.";
    }
    supplies.antidotes = 0;
    if (recovery_items(supplies) != std::vector<std::string>{"full-heal", "potion"}){
        return "Expected a Full Heal with no Antidote, then a Potion.";
    }
    supplies.full_heals = 0;
    if (recovery_items(supplies) != std::vector<std::string>{"potion"}){
        return "Expected a Potion with nothing to cure poison.";
    }
    supplies.hurt = false;
    supplies.hp_red = false;
    supplies.hp_fill = 0.7;
    if (recovery_items(supplies) != std::vector<std::string>{"potion"}){
        return "Expected a Potion for a poisoned bar under 3/4.";
    }
    supplies.hp_fill = 0.9;
    if (!recovery_items(supplies).empty() || !nurse_before_battle(supplies)){
        return "Expected no Potion for a poisoned bar near full, and the nurse for the poison.";
    }
    supplies.status = StatusCondition::PARALYSIS;
    supplies.full_heals = 1;
    if (recovery_items(supplies) != std::vector<std::string>{"full-heal"}){
        return "Expected a Full Heal for PAR with no Parlyz Heal.";
    }
    return true;
}

//  The Tower rival as PokeGuy fought him (10.03): two X Specials on the
//  first turns, then attacks. A Potion comes first for HP not green, red
//  HP or a wild battle uses none, and the last X Special takes its row.
UnitTestResult test_planned_battle_items(Logger&){
    const PartyStatus GREEN{.hp_seen = true, .hp_yellow = false, .hp_red = false, .status = StatusCondition::NONE};
    const PartyStatus RED{.hp_seen = true, .hp_yellow = false, .hp_red = true, .status = StatusCondition::NONE};
    const PartyStatus YELLOW{.hp_seen = true, .hp_yellow = true, .hp_red = false, .status = StatusCondition::NONE};
    Supplies supplies;
    supplies.lead = story_lead_after_brock();
    set_from_bag(supplies, {{"potion", 2}, {"x-special", 2}, {"x-speed", 6}});
    supplies.battle_items = {{.item = "x-special", .quantity = 3}};

    Supplies wild_supplies = supplies;
    BattleDecisions wild(wild_supplies, BattleKind::WILD_FIGHT);
    if (wild.turn(GREEN).choice == TurnChoice::BATTLE_ITEM){
        return "Expected no X Special in a wild battle.";
    }

    BattleDecisions decisions(supplies, BattleKind::TRAINER);
    if (decisions.turn(YELLOW).choice != TurnChoice::POTION){
        return "Expected a Potion before an X Special for yellow HP.";
    }
    decisions.turn_done(TurnChoice::POTION, TurnResult::DONE);
    for (int c = 0; c < 2; c++){
        const TurnAction action = decisions.turn(GREEN);
        if (action.choice != TurnChoice::BATTLE_ITEM || action.item != "x-special"){
            return std::format("Expected an X Special at turn {}.", c + 2);
        }
        decisions.turn_done(action.choice, TurnResult::DONE);
    }
    if (item_row(supplies, "x-special") != -1 || item_row(supplies, "x-speed") != 1){
        return "Expected the X Special's row gone with the last of the 2, X Speed a row up.";
    }
    if (decisions.turn(GREEN).choice != TurnChoice::MOVE){
        return "Expected an attack once the bag has no X Special, though the plan asked for 3.";
    }

    Supplies red_supplies;
    set_from_bag(red_supplies, {{"x-special", 5}});
    red_supplies.battle_items = {{.item = "x-special", .quantity = 1}};
    BattleDecisions red(red_supplies, BattleKind::TRAINER);
    if (red.turn(RED).choice != TurnChoice::MOVE){
        return "Expected an attack, not an X Special, at red HP with no Potion.";
    }
    const TurnAction other = red.turn(GREEN);
    red.turn_done(other.choice, TurnResult::NOT_DONE);
    if (other.choice != TurnChoice::BATTLE_ITEM || red.turn(GREEN).choice != TurnChoice::MOVE || red_supplies.other_counts.at("x-special") != 5){
        return "Expected no item after the bag's row read as another item.";
    }
    return true;
}


const AutoStoryItemsTestCase AUTO_STORY_ITEMS_CASES[] = {
    {
        .input = {.name = "Mart cursor stays on the row bought", .body = test_mart_cursor},
        .expectation = {.description = "verifies mart cursor remains on purchased item index"},
    },
    {
        .input = {.name = "Items pocket rows", .body = test_pocket_rows},
        .expectation = {.description = "parses item pocket rows from visual representation"},
    },
    {
        .input = {.name = "a loaded save's counts come from the bag", .body = test_loaded_bag},
        .expectation = {.description = "initializes bag counts when loading saved state"},
    },
    {
        .input = {.name = "an item the story doesn't count keeps its row", .body = test_loaded_bag_other_items},
        .expectation = {.description = "preserves row indexing for uncounted items"},
    },
    {
        .input = {.name = "Items pocket rows past the first page", .body = test_item_shown_row},
        .expectation = {.description = "places rows below the first six where the scrolled list shows them"},
    },
    {
        .input = {.name = "an unread item name keeps its row", .body = test_pocket_rows_unread},
        .expectation = {.description = "preserves placeholder row when item name is unread"},
    },
    {
        .input = {.name = "unread bag counts count as some", .body = test_unread_counts},
        .expectation = {.description = "treats unread quantities as holding at least one"},
    },
    {
        .input = {.name = "every item a plan reads has a name in the reader", .body = test_plan_items_readable},
        .expectation = {.description = "the item list reader names each item a plan buys, uses or counts"},
    },
    {
        .input = {.name = "no item names read in a language without them", .body = test_unread_language},
        .expectation = {.description = "handles unsupported languages gracefully"},
    },
    {
        .input = {.name = "a wild battle seen late is placed where it stopped the leg", .body = test_late_battle_placed},
        .expectation = {.description = "places late wild battle at interruption point"},
    },
    {
        .input = {.name = "Awakenings counted from the bag", .body = test_loaded_bag_awakenings},
        .expectation = {.description = "verifies awakenings are read and tracked from bag contents"},
    },
    {
        .input = {.name = "the status badge: none, PSN, SLP", .body = test_status_badge},
        .expectation = {.description = "detects status conditions accurately from status bar image"},
    },
    {
        .input = {.name = "poison after a battle: Antidote, else Full Heal, then a Potion", .body = test_poison_cured_after_battle},
        .expectation = {.description = "cures a status left by a battle before the story walks on"},
    },
    {
        .input = {.name = "battle items: the rival's X Specials first, Potions before them", .body = test_planned_battle_items},
        .expectation = {.description = "uses the plan's X items before attacking"},
    },
};

} // namespace


void add_tests_AutoStoryItems(UnitTestDatabase& database){
    for (const auto& scenario : AUTO_STORY_ITEMS_CASES){
        database.add<Test_AutoStoryItems>(scenario);
    }
    //  "Wild CHANSEY appeared!" with its HP box, and Mt. Moon B2F's corridor
    //  after "Got away safely!", whose black lower half reads as the battle's
    //  text box (console B, 2026-11-03: 9 minutes of A and B there).
    database.add<Test_BattleShows>("PokemonFRLG/WildEncounterReader/eng_chansey.jpg", true);
    database.add<Test_BattleShows>("PokemonFRLG/AutoStoryBattle/MtMoonB2F_AfterRun_False.png");
    for (std::string_view image : {
        "MtMoon1F_HpGreen_45of45.png",
        "MtMoon1F_HpGreen_27of45.png",
        "MtMoonB2F_HpYellow_17of55.png",
        "MtMoonB2F_HpYellow_17of55_BarLow.png",
    }){
        database.add<Test_PartyHp>(std::format("PokemonFRLG/AutoStoryBattle/{}", image));
    }
    //  The YES/NO to "Delete a move to make room for BITE?" (Miguel), and the
    //  S.S. Anne rival's and Misty's send-outs, which the move prompt's
    //  detector alone took for one (console B, 2026-11-03).
    for (std::string_view image : {
        "Miguel_DeleteMoveForBite_YesNo_True.png",
        "SSAnneRival_SentOutRaticate_YesNo_False.png",
        "SSAnneRival_SentOutKadabra_YesNo_False.png",
        "Misty_SentOutStarmie_YesNo_False.png",
    }){
        database.add<Test_BattleYesNo>(std::format("PokemonFRLG/AutoStoryBattle/{}", image));
    }
    //  The S.S. Anne rival's Pidgeotto at full HP, its Raticate low.
    database.add<Test_FoeHp>("PokemonFRLG/AutoStoryBattle/SSAnneRival_Pidgeotto_FoeHp_48of48.png");
    database.add<Test_FoeHp>("PokemonFRLG/AutoStoryBattle/SSAnneRival_Raticate_FoeHp_7of48.png");
    //  The battle's choices, registered here to keep add_tests() within its size.
    add_tests_AutoStoryParty(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

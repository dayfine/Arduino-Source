/*  Mail Glitch Activation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryMart.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryPacedRoute.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_MailGlitchActions.h"
#include "PokemonFRLG_MailGlitchBattle.h"
#include "PokemonFRLG_MailGlitchTable.h"
#include "PokemonFRLG_MailGlitch_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const MapData* find_map(const std::vector<MapData>& maps, std::string_view name){
    for (const MapData& map : maps){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

class Test_MailGlitchRoomsAndMart : public UnitTest{
public:
    Test_MailGlitchRoomsAndMart()
        : UnitTest("PokemonFRLG::MailGlitch - coded rooms, mart stock, and item OCR names")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<MapData> maps = overworld_maps({});
        for (const char* room : {
            "CeladonCity_Restaurant",
            "CeladonCity_GameCorner_PrizeRoom",
            "CeladonCity_DepartmentStore_4F",
            "VermilionCity_House2",
            "Route2_House",
        }){
            if (find_map(maps, room) == nullptr){
                return std::format("Missing coded room: {}", room);
            }
        }
        const std::vector<MartStockItem>& stock_4f = mart_stock("CeladonCity_DepartmentStore_4F");
        if (stock_4f.size() != 6 || stock_4f[1].item != "retro-mail" || stock_4f[1].price != 50){
            return "CeladonCity_DepartmentStore_4F mart stock does not match pokefirered (expected retro-mail at row 1 for $50).";
        }
        for (const char* item : {"poke-doll", "retro-mail", "berry-pouch"}){
            if (!item_list_reader_knows(item)){
                return std::format("ItemListReader does not know \"{}\".", item);
            }
        }
        return true;
    }
};

class Test_MailGlitchPlanLoad : public UnitTest{
public:
    Test_MailGlitchPlanLoad()
        : UnitTest("PokemonFRLG::MailGlitch - mg-01..mg-05 plan files load and chain cleanly")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<StoryCheckpoint>& rows = mail_glitch_checkpoints();
        const std::vector<std::string> expected{
            "mg-01", "mg-02", "mg-03", "mg-04a", "mg-04b", "mg-04c", "mg-05a", "mg-05",
        };
        if (rows.size() != expected.size()){
            return std::format("Expected {} checkpoints, got {}.", expected.size(), rows.size());
        }
        for (size_t i = 0; i < expected.size(); i++){
            if (rows[i].id != expected[i]){
                return std::format("Row {} has id {}, expected {}.", i, rows[i].id, expected[i]);
            }
            if (!rows[i].start){
                return std::format("Row {} has no start pose.", rows[i].id);
            }
        }
        const auto battle = std::find_if(rows.back().steps.begin(), rows.back().steps.end(), [](const StoryStep& step){
            return step.action == "mail_glitch_double_battle";
        });
        if (battle == rows.back().steps.end() || !battle->pose_after){
            return "mg-05 has no double battle end pose.";
        }
        const std::optional<StoryPose> after = story_pose(Pokemon::pose_of(*battle->pose_after));
        if (!after || after->map != "Route16" || after->tile.x != 30 || after->tile.y != 6){
            return "mg-05 must battle Lea & Jed on Route 16, avoiding the Route 8 Clefairy's Follow Me (mGBA, 2026-11-23).";
        }
        return true;
    }
};

class Test_MailGlitchOverworldSim : public UnitTest{
public:
    explicit Test_MailGlitchOverworldSim(std::string row_id)
        : UnitTest(std::format("PokemonFRLG::MailGlitch - OverworldSim walks {}", row_id))
        , m_row_id(std::move(row_id))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<StoryCheckpoint>& rows = mail_glitch_checkpoints();
        const auto it = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& r){
            return r.id == m_row_id;
        });
        if (it == rows.end()){
            return std::format("No Mail Glitch checkpoint {}.", m_row_id);
        }
        const StoryCheckpoint& row = *it;
        const std::optional<StoryPose> start = story_pose(Pokemon::pose_of(*row.start));
        if (!start){
            return std::format("{}: start is not a single tile.", row.id);
        }
        OverworldSim sim(story_maps(), MapPosition{start->map, start->tile}, start->facing.value_or(Direction::SOUTH));
        StoryPose at = *start;
        std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
        for (const StoryStep& step : row.steps){
            const std::string error = row_step_carrying_flags(sim, at, step, row.id, flags);
            if (!error.empty()){
                return error;
            }
        }
        return true;
    }

private:
    std::string m_row_id;
};

class Test_MailGlitchTower5FGrindLoop : public UnitTest{
public:
    Test_MailGlitchTower5FGrindLoop()
        : UnitTest("PokemonFRLG::MailGlitch - mg-04b idempotent Tower 5F grind loop and done flags")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<StoryCheckpoint>& rows = mail_glitch_checkpoints();
        const auto it = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& r){
            return r.id == MAIL_GLITCH_GRIND_CHECKPOINT;
        });
        if (it == rows.end() || !it->start){
            return "Missing mg-04b checkpoint or start pose.";
        }
        const StoryPose start = *story_pose(Pokemon::pose_of(*it->start));
        OverworldSim sim(story_maps(), MapPosition{start.map, start.tile}, *start.facing);
        StoryPose at = start;
        std::set<std::string> flags;
        for (int pass = 1; pass <= 2; pass++){
            for (const StoryStep& step : it->steps){
                const std::string err = row_step_carrying_flags(sim, at, step, it->id, flags);
                if (!err.empty()){
                    return std::format("Pass {}: {}", pass, err);
                }
            }
            if (!(sim.state().position == MapPosition{start.map, start.tile})){
                return std::format(
                    "Pass {}: expected return to {}, ended at {}.",
                    pass, to_string(MapPosition{start.map, start.tile}), to_string(sim.state().position)
                );
            }
        }
        if (mail_glitch_grind_done(flags)){
            return "mail_glitch_grind_done() should be false when no done flags are set.";
        }
        flags.insert(grind_done_flag("farfetchd"));
        if (mail_glitch_grind_done(flags)){
            return "mail_glitch_grind_done() should be false when only farfetchd is done.";
        }
        flags.insert(grind_done_flag("mr-mime"));
        if (!mail_glitch_grind_done(flags)){
            return "mail_glitch_grind_done() should be true once both farfetchd and mr-mime are done.";
        }
        return true;
    }
};

class Test_MailGlitchGrindLogic : public UnitTest{
public:
    Test_MailGlitchGrindLogic()
        : UnitTest("PokemonFRLG::MailGlitch - SweeperMovePool rotation and summary check cadence")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        SummaryRead blastoise;
        blastoise.moves = {
            {.move = "bite"},
            {.move = "surf"},
            {.move = "strength"},
            {.move = "blizzard"},
        };
        SweeperMovePool pool(blastoise);
        if (pool.pick_slot() != 1){
            return std::format("Expected surf (slot 1) first, got slot {}.", pool.pick_slot());
        }
        pool.mark_empty(1);
        if (pool.pick_slot() != 3){
            return std::format("Expected blizzard (slot 3) after surf, got slot {}.", pool.pick_slot());
        }
        pool.mark_empty(3);
        if (pool.pick_slot() != 0){
            return std::format("Expected bite (slot 0) before strength (Ghost-safe), got slot {}.", pool.pick_slot());
        }
        pool.mark_empty(0);
        if (pool.pick_slot() != 2){
            return std::format("Expected strength (slot 2) last, got slot {}.", pool.pick_slot());
        }
        pool.restore_all();
        if (pool.pick_slot() != 1 || pool.low_pp()){
            return "restore_all() did not restore surf as primary with full PP.";
        }
        if (should_check_grind_summary(10, 33, 5, false) || !should_check_grind_summary(10, 33, 12, false)){
            return "Early-grind cadence should check every 12 battles.";
        }
        if (should_check_grind_summary(28, 33, 4, false) || !should_check_grind_summary(28, 33, 5, false)){
            return "Mid-target cadence (within 6 levels) should check every 5 battles.";
        }
        if (!should_check_grind_summary(31, 33, 1, false) || !should_check_grind_summary(10, 33, 1, true)){
            return "Near-target (within 2 levels) or move prompt should check immediately.";
        }
        return true;
    }
};

} // namespace


void add_tests_MailGlitch(UnitTestDatabase& database){
    database.add<Test_MailGlitchRoomsAndMart>();
    database.add<Test_MailGlitchPlanLoad>();
    for (const char* id : {"mg-01", "mg-02", "mg-03", "mg-04a", "mg-04b", "mg-04c", "mg-05a", "mg-05"}){
        database.add<Test_MailGlitchOverworldSim>(id);
    }
    database.add<Test_MailGlitchTower5FGrindLoop>();
    database.add<Test_MailGlitchGrindLogic>();
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

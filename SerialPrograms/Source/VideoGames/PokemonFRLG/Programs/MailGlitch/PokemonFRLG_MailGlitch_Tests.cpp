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
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_MailGlitchActions.h"
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
        for (const char* item : {"poke-doll", "retro-mail", "berry-pouch", "oran-berry", "sitrus-berry"}){
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
        const std::vector<std::string> expected{"mg-01", "mg-02", "mg-03", "mg-04", "mg-05"};
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

} // namespace


void add_tests_MailGlitch(UnitTestDatabase& database){
    database.add<Test_MailGlitchRoomsAndMart>();
    database.add<Test_MailGlitchPlanLoad>();
    for (const char* id : {"mg-01", "mg-02", "mg-03", "mg-04", "mg-05"}){
        database.add<Test_MailGlitchOverworldSim>(id);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  BDSP AutoStory Story World Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  StoryWorld on small synthetic maps: what each tile does to a step, maps
 *  joined at offsets, and tiles blocked or taken off the maps.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_StoryWorld_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::MapData;
using Pokemon::TilePos;

//  A map of these rows, with a person standing still on each `standing`
//  tile and one wandering on each `wandering` tile.
MapData make_map(
    const std::vector<std::string>& rows,
    const std::vector<TilePos>& standing = {},
    const std::vector<TilePos>& wandering = {}
){
    MapData map;
    map.name = "Test";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{(int)rows[0].size(), (int)rows.size(), {}};
    for (const std::string& row : rows){
        for (char glyph : row){
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(glyph));
        }
    }
    for (const TilePos& pos : standing){
        map.objects.push_back({
            .name = "standing", .kind = Pokemon::ObjectKind::NPC, .x = pos.x, .y = pos.y,
            .facing = std::nullopt, .wander = std::nullopt,
        });
    }
    for (const TilePos& pos : wandering){
        map.objects.push_back({
            .name = "wandering", .kind = Pokemon::ObjectKind::NPC, .x = pos.x, .y = pos.y,
            .facing = std::nullopt, .wander = Pokemon::WanderBox{pos.x, pos.y, pos.x, pos.y},
        });
    }
    return map;
}

StoryWorld make_world(const MapData& map){
    StoryWorld world;
    world.add("Test", map, {0, 0});
    return world;
}


//  One step from (1, 1) of a 3 x 3 map whose middle tile (the step's
//  target) is `target`, and whether the player is held at a wall there.
struct StepCase{
    std::string name;
    std::vector<std::string> rows;
    std::vector<TilePos> standing = {};
    std::vector<TilePos> wandering = {};
    TilePos from = {};
    Direction direction = Direction::NORTH;
    std::optional<TilePos> lands = std::nullopt;
    bool at_wall = false;
};

class Test_StoryWorldSteps : public UnitTest{
public:
    Test_StoryWorldSteps()
        : UnitTest("PokemonBDSP::AutoStory - Story world: what each tile does to a step")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<StepCase> cases{
            {.name = "open ground", .rows = {".", "."}, .from = {0, 1}, .lands = TilePos{0, 0}},
            {.name = "tall grass", .rows = {"~", "."}, .from = {0, 1}, .lands = TilePos{0, 0}},
            {.name = "a wall", .rows = {"#", "."}, .from = {0, 1}, .at_wall = true},
            {.name = "water", .rows = {"w", "."}, .from = {0, 1}, .at_wall = true},
            {.name = "a sign", .rows = {"S", "."}, .from = {0, 1}, .at_wall = true},
            {.name = "a door is not a wall", .rows = {"D", "."}, .from = {0, 1}},
            {.name = "off the map is not a wall", .rows = {"."}, .from = {0, 0}},
            {.name = "a south ledge hops south", .rows = {".", "v", "."}, .direction = Direction::SOUTH, .lands = TilePos{0, 2}},
            {.name = "a south ledge stops north", .rows = {".", "v", "."}, .from = {0, 2}, .at_wall = true},
            {.name = "a west ledge hops west", .rows = {".<."}, .from = {2, 0}, .direction = Direction::WEST, .lands = TilePos{0, 0}},
            {.name = "a west ledge stops east", .rows = {".<."}, .direction = Direction::EAST, .at_wall = true},
            {.name = "a ledge onto a wall stops", .rows = {".", "v", "#"}, .direction = Direction::SOUTH, .at_wall = true},
            {.name = "a person standing still", .rows = {".", "."}, .standing = {{0, 0}}, .from = {0, 1}, .at_wall = true},
            {.name = "a wanderer does not block", .rows = {".", "."}, .wandering = {{0, 0}}, .from = {0, 1}, .lands = TilePos{0, 0}},
        };
        for (const StepCase& item : cases){
            const StoryWorld world = make_world(make_map(item.rows, item.standing, item.wandering));
            const std::optional<TilePos> lands = world.step(item.from, item.direction);
            if (lands != item.lands){
                return std::format("{}: lands on {}, expected {}.", item.name,
                    lands ? Pokemon::to_string(*lands) : "nothing", item.lands ? Pokemon::to_string(*item.lands) : "nothing");
            }
            if (world.at_wall(item.from, item.direction) != item.at_wall){
                return std::format("{}: at_wall() is {}, expected {}.", item.name, !item.at_wall, item.at_wall);
            }
        }
        return true;
    }
};


//  Two maps joined: tiles of each at their offsets, the gap between them
//  off the maps, and a shared wall border allowed where nothing else is.
class Test_StoryWorldJoin : public UnitTest{
public:
    Test_StoryWorldJoin()
        : UnitTest("PokemonBDSP::AutoStory - Story world: maps joined at offsets")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryWorld world;
        world.add("West", make_map({"##", ".#"}), {0, 0});
        world.add("East", make_map({"#.", "#~"}), {1, -1});
        const std::vector<std::pair<TilePos, std::string>> described{
            {{0, 2}, "West (0, 1)"},
            {{2, 0}, "East (1, 0)"},
            {{1, 1}, "West (1, 0)"},    //  the shared wall: the first map holds it
            {{2, 2}, "world (2, 2)"},
        };
        for (const auto& [pos, text] : described){
            if (world.describe(pos) != text){
                return std::format("{}: described as \"{}\", expected \"{}\".", Pokemon::to_string(pos), world.describe(pos), text);
            }
        }
        if (world.width() != 3 || world.height() != 3 || world.covered({2, 2}) || !world.standable({2, 1})){
            return std::string("The joined world is not 3 x 3 with East's grass at (2, 1) and nothing at (2, 2).");
        }
        if (!(world.to_world("East", {0, 0}) == TilePos{1, 0})){
            return "East (0, 0) is world " + Pokemon::to_string(world.to_world("East", {0, 0})) + ", expected (1, 0).";
        }
        try{
            world.add("Overlap", make_map({"."}), {0, 1});
            return std::string("Adding a map over open ground did not throw.");
        }catch (InternalProgramError&){}
        try{
            world.to_world("North", {0, 0});
            return std::string("to_world() of an unknown map did not throw.");
        }catch (InternalProgramError&){}
        return true;
    }
};


//  block() and uncover() change what a tile does, and survive a map added
//  later that moves the world's origin.
class Test_StoryWorldBlockUncover : public UnitTest{
public:
    Test_StoryWorldBlockUncover()
        : UnitTest("PokemonBDSP::AutoStory - Story world: blocked and uncovered tiles")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryWorld world;
        world.add("Road", make_map({"...."}), {0, 0});
        world.block({1, 0});
        world.uncover({2, 0});
        if (world.step({0, 0}, Direction::EAST) || !world.at_wall({0, 0}, Direction::EAST)){
            return std::string("A blocked tile is not a wall.");
        }
        if (world.covered({2, 0}) || world.at_wall({3, 0}, Direction::WEST)){
            return std::string("An uncovered tile is still on the map, or a wall.");
        }
        world.add("North", make_map({"#"}), {0, -1});
        const TilePos blocked = world.to_world("Road", {1, 0});
        const TilePos uncovered = world.to_world("Road", {2, 0});
        if (!world.occupied(blocked) || world.covered(uncovered) || !world.standable(world.to_world("Road", {3, 0}))){
            return std::string("Adding a map lost the blocked or uncovered tile.");
        }
        return true;
    }
};

}


void add_tests_AutoStory_StoryWorld(UnitTestDatabase& database){
    database.add<Test_StoryWorldSteps>();
    database.add<Test_StoryWorldJoin>();
    database.add<Test_StoryWorldBlockUncover>();
}


}
}
}

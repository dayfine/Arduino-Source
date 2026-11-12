/*  AutoStory Slope Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 11.02 on the sims: Route 16 and Route 18 to Fuchsia City on the
 *  overworld sim, and Route 17's slope, which has no runtime map, on a
 *  model of it here, from pokefirered's Route17 layout (map.bin, the
 *  celadon_city tileset's behaviors) and map.json: its walls, water,
 *  signs, ledges, hidden items and Bikers. The model rides the presses the
 *  console sends (slope_move_presses()) through ride_slope().
 *
 */

#include <algorithm>
#include <chrono>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStorySlope.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::Direction;

//  Rows of the model: Route 17's 0..159, then Route 18's from 160. Row -1
//  is Route 16's (14, 19), above the slope.
constexpr int ROUTE18_ROW = 160;

struct Biker{
    int x;
    int y;
    std::vector<std::pair<std::pair<int, int>, Direction>> looks;
    int sight;
};

//  pokefirered Route17 map.json's Bikers, with the tiles they stand on
//  and the ways they look: walkers a tile either way of their start.
std::vector<Biker> route17_bikers(){
    const auto walker = [](int x, int y){
        Biker biker{x, y, {}, 1};
        for (int dy = -1; dy <= 1; dy++){
            biker.looks.push_back({{x, y + dy}, Direction::NORTH});
            biker.looks.push_back({{x, y + dy}, Direction::SOUTH});
        }
        return biker;
    };
    return {
        walker(4, 18), walker(11, 15), walker(18, 41), walker(18, 94), walker(4, 116),
        Biker{15, 24, {{{15, 24}, Direction::EAST}, {{16, 24}, Direction::SOUTH}, {{16, 25}, Direction::WEST}, {{15, 25}, Direction::NORTH}}, 1},
        Biker{7, 38, {{{7, 38}, Direction::WEST}, {{7, 38}, Direction::EAST}}, 5},
        Biker{2, 61, {{{2, 61}, Direction::EAST}}, 4},
        Biker{21, 61, {{{21, 61}, Direction::WEST}}, 3},
        Biker{16, 134, {{{15, 134}, Direction::WEST}, {{15, 135}, Direction::SOUTH}, {{16, 135}, Direction::EAST}, {{16, 134}, Direction::NORTH}}, 1},
    };
}

bool in_box(int x, int y, int x0, int x1, int y0, int y1){
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

//  Route 17's walls, water and signs (and Route 18's west end).
bool blocked(int x, int y){
    if (y < 0){
        return x != 14;
    }
    if (y >= ROUTE18_ROW){
        return !in_box(x, y, 8, 16, ROUTE18_ROW, ROUTE18_ROW + 10);
    }
    if (x < 2 || x > 21 || (y <= 9 && x >= 15)){
        return true;
    }
    if (in_box(x, y, 13, 16, 30, 97) || in_box(x, y, 6, 9, 44, 93)
        || in_box(x, y, 8, 16, 98, 121) || in_box(x, y, 8, 16, 126, 132)
    ){
        return true;
    }
    for (const auto& [sx, sy] : std::vector<std::pair<int, int>>{{12, 53}, {12, 68}, {12, 83}, {12, 97}, {12, 125}, {11, 157}}){
        if (x == sx && y == sy){
            return true;
        }
    }
    if (y == 137){
        return !in_box(x, y, 8, 10, 137, 137) && !in_box(x, y, 14, 16, 137, 137);
    }
    return y >= 138 && (x <= 7 || x >= 17);
}

bool ledge(int x, int y){
    return y == 158 && x >= 8 && x <= 16 && x != 12;
}

bool slope(int x, int y){
    return y >= 0 && y <= 158 && !blocked(x, y) && !ledge(x, y);
}

int dx_of(Direction direction){
    return direction == Direction::EAST ? 1 : direction == Direction::WEST ? -1 : 0;
}
int dy_of(Direction direction){
    return direction == Direction::SOUTH ? 1 : direction == Direction::NORTH ? -1 : 0;
}

//  Route 17 with the player on the Bicycle, a frame at a time as
//  pokefirered's bike.c reads the controller: only between movements.
//  On a pull-down tile, with B released the player rolls south
//  (PlayerWalkFaster, 4 frames), turning to face south between tiles; with
//  B held a D-pad press is a walking step (PlayerWalkNormal, 16 frames), and
//  B alone stops the player. A press that comes while a movement is under
//  way is lost, and so is an A pressed then.
class SlopeModel{
public:
    SlopeModel(int x, int y)
        : m_x(x), m_y(y)
        , m_items{{{18, 83}, "rare-candy"}, {{14, 157}, "max-elixir"}}
        , m_bikers(route17_bikers())
    {
        for (const Biker& biker : m_bikers){
            m_biker_tiles.insert({biker.x, biker.y});
        }
    }

    SlopeSendResult send(const std::vector<SlopePress>& presses, bool message){
        for (size_t c = 0; c < presses.size(); c++){
            const SlopePress& press = presses[c];
            const double end = m_clock + std::chrono::duration<double, std::milli>(press.hold).count();
            bool first = true;
            for (; m_clock < end; m_clock += FRAME_MS){
                const bool a = press.a && first;
                first = false;
                if (m_busy > 0){
                    m_busy--;
                    continue;
                }
                frame(press);
                if (m_caught){
                    m_caught = false;
                    return SlopeSendResult{.sent = SlopeSent::BATTLE, .press = c};
                }
                if (a && message && face_item()){
                    return SlopeSendResult{.sent = SlopeSent::MESSAGE};
                }
            }
        }
        return SlopeSendResult{};
    }

    //  A Biker catches the player the first time they come onto (x, y):
    //  the presses stop there, as the battle's start cuts the batch short.
    void catch_on(int x, int y){
        m_catch = std::pair<int, int>{x, y};
    }

    int x() const{ return m_x; }
    int y() const{ return m_y; }
    const std::vector<std::string>& seen() const{ return m_seen; }

private:
    static constexpr double FRAME_MS = 1000.0 / 59.7275;
    static constexpr int WALK_FRAMES = 16;
    static constexpr int ROLL_FRAMES = 4;
    static constexpr int FLAT_FRAMES = 6;
    static constexpr int JUMP_FRAMES = 32;

    enum class State{
        NORMAL,
        SLOPE,
    };

    //  bike.c's input handlers, for a frame with no movement under way.
    void frame(const SlopePress& press){
        if (!slope(m_x, m_y)){
            m_state = State::NORMAL;
            if (press.dpad){
                step(*press.dpad, FLAT_FRAMES);
            }
            return;
        }
        if (m_state == State::SLOPE && press.dpad != std::optional<Direction>(m_moving)){
            //  BikeInputHandler_Slope to _Turning: faces the D-pad's way,
            //  south with none, for this frame. A roll's tiles take 6
            //  frames with it.
            m_state = State::NORMAL;
            m_moving = press.dpad.value_or(Direction::SOUTH);
            m_facing = m_moving;
            return;
        }
        if (m_state == State::SLOPE || !press.b){
            m_state = State::SLOPE;
            if (!press.dpad || *press.dpad == Direction::SOUTH){
                step(Direction::SOUTH, ROLL_FRAMES);
            }else{
                step(*press.dpad, WALK_FRAMES);
            }
            return;
        }
        if (press.dpad){
            m_state = State::SLOPE;
            step(*press.dpad, WALK_FRAMES);
        }
    }
    bool open(int x, int y) const{
        return !blocked(x, y) && !m_biker_tiles.contains({x, y});
    }
    //  One tile `direction`, taking `frames`; south onto a ledge jumps it.
    //  Nothing against a wall.
    void step(Direction direction, int frames){
        const int x = m_x + dx_of(direction);
        const int y = m_y + dy_of(direction);
        if (ledge(x, y)){
            if (direction != Direction::SOUTH){
                return;
            }
            m_facing = m_moving = direction;
            m_y = y + 1;
            m_busy = JUMP_FRAMES;
            check_sight();
            return;
        }
        if (!open(x, y)){
            return;
        }
        m_facing = m_moving = direction;
        m_x = x;
        m_y = y;
        m_busy = frames;
        check_sight();
        if (m_catch == std::pair<int, int>{m_x, m_y}){
            m_catch.reset();
            m_caught = true;
        }
    }
    bool face_item(){
        const auto item = m_items.find({m_x + dx_of(m_facing), m_y + dy_of(m_facing)});
        if (item == m_items.end()){
            return false;
        }
        m_items.erase(item);
        return true;
    }
    void check_sight(){
        for (const Biker& biker : m_bikers){
            for (const auto& [at, look] : biker.looks){
                for (int c = 1; c <= biker.sight; c++){
                    if (at.first + dx_of(look) * c == m_x && at.second + dy_of(look) * c == m_y){
                        m_seen.push_back(std::format("({}, {}) by the Biker at ({}, {})", m_x, m_y, biker.x, biker.y));
                    }
                }
            }
        }
    }

    int m_x;
    int m_y;
    Direction m_facing = Direction::SOUTH;
    Direction m_moving = Direction::SOUTH;
    State m_state = State::NORMAL;
    double m_clock = 0;
    int m_busy = 0;
    std::map<std::pair<int, int>, std::string> m_items;
    std::vector<Biker> m_bikers;
    std::set<std::pair<int, int>> m_biker_tiles;
    std::vector<std::string> m_seen;
    std::optional<std::pair<int, int>> m_catch;
    bool m_caught = false;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

const StoryStep* slope_step(const StoryCheckpoint& row){
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& data){
        return parse_slope_step(data).has_value();
    });
    return step == row.steps.end() ? nullptr : &*step;
}

SlopeResult ride_on_model(SlopeModel& model, const std::vector<SlopeMove>& moves){
    const SlopeCalls calls{
        .send = [&model](const std::vector<SlopePress>& presses, bool message){ return model.send(presses, message); },
        .fight = []{ return true; },
        .log = [](const std::string&){},
    };
    return ride_slope(moves, calls);
}

//  11.02's slope: from above Route 17's column 14 to the water on Route
//  18's (12, 10), both items found, no Biker's sight entered.
UnitTestResult test_slope_moves(){
    const StoryStep* step = slope_step(checkpoint("11.02"));
    if (step == nullptr){
        return "11.02: no slope step.";
    }
    SlopeModel model(14, -1);
    const SlopeResult result = ride_on_model(model, *parse_slope_step(*step));
    if (result.end != SlopeEnd::DONE || result.found != std::vector<std::string>{"rare-candy", "max-elixir"}){
        return std::format("11.02: the slope ended at move {}, at ({}, {}), {} item(s) found.", result.move + 1, model.x(), model.y(), result.found.size());
    }
    if (model.x() != 12 || model.y() != ROUTE18_ROW + 10){
        return std::format("11.02: the slope ended at ({}, {}), not Route 18's (12, 10).", model.x(), model.y());
    }
    if (!model.seen().empty()){
        return "11.02: a Biker sees the player at " + model.seen().front();
    }
    return true;
}

//  A gap in the controller's queue while B holds the player on the slope
//  rolls them: three steps east with B released between them, as separate
//  presses with a wait would send them, leave the player rolled down
//  column 14 to the water instead of at (17, 29) (red: what the batch
//  avoids).
UnitTestResult test_gap_rolls(){
    SlopeModel batched(14, 20);
    batched.send(slope_move_presses(SlopeMove{.kind = SlopeMoveKind::STEP, .direction = Direction::EAST, .tiles = 3}), false);
    SlopeModel gapped(14, 20);
    for (int c = 0; c < 3; c++){
        gapped.send(slope_move_presses(SlopeMove{.kind = SlopeMoveKind::STEP, .direction = Direction::EAST, .tiles = 1}), false);
        gapped.send({SlopePress{.hold = Milliseconds(300)}}, false);
    }
    if (batched.x() != 17 || batched.y() != 20){
        return std::format("B held: ended ({}, {}), expected (17, 20).", batched.x(), batched.y());
    }
    if (gapped.y() == 20){
        return "A gap with B released should roll the player.";
    }
    //  Every press of a STEP or SEARCH holds B.
    for (SlopeMoveKind kind : {SlopeMoveKind::STEP, SlopeMoveKind::SEARCH}){
        for (const SlopePress& press : slope_move_presses(SlopeMove{.kind = kind, .tiles = 2, .item = "x"})){
            if (!press.b){
                return "A step or search released B.";
            }
        }
    }
    return true;
}

//  A brake out of the window (rows 62..81 on column 17) misses the RARE
//  CANDY: the search runs out of taps and the step fails.
UnitTestResult test_search_misses(){
    std::vector<SlopeMove> moves = *parse_slope_step(*slope_step(checkpoint("11.02")));
    moves[3].tiles = 60;
    SlopeModel model(14, -1);
    const SlopeResult result = ride_on_model(model, moves);
    if (result.end != SlopeEnd::NOT_FOUND || result.move != 5){
        return std::format("A brake past row 83: ended {}, move {}.", int(result.end), result.move + 1);
    }
    return true;
}

//  A Biker who catches the player on (x, y) is fought, and the ride goes
//  on from the stop the slope pulls them to (user, 2026-11-12: "if a
//  Biker catches us, fight"): caught rolling down column 14 or on Route
//  18's last steps south, it still finds both items and ends on Route
//  18's (12, 10).
UnitTestResult check_caught_and_done(int x, int y){
    SlopeModel model(14, -1);
    model.catch_on(x, y);
    const SlopeResult result = ride_on_model(model, *parse_slope_step(*slope_step(checkpoint("11.02"))));
    if (result.end != SlopeEnd::DONE || result.found != std::vector<std::string>{"rare-candy", "max-elixir"}){
        return std::format("Caught on ({}, {}): ended {} at move {}, at ({}, {}), {} item(s) found.", x, y, int(result.end), result.move + 1, model.x(), model.y(), result.found.size());
    }
    if (model.x() != 12 || model.y() != ROUTE18_ROW + 10){
        return std::format("Caught on ({}, {}): ended at ({}, {}), not Route 18's (12, 10).", x, y, model.x(), model.y());
    }
    return true;
}
UnitTestResult test_caught_rolling(){
    return check_caught_and_done(14, 10);
}
UnitTestResult test_caught_on_route18(){
    return check_caught_and_done(12, ROUTE18_ROW + 5);
}

//  Caught part-way along a step east, the column the slope then pulls the
//  player down is not known: the ride stops at that move (move 3, "step
//  east 3"), never guessing.
UnitTestResult test_caught_stepping_east(){
    SlopeModel model(14, -1);
    model.catch_on(16, 29);
    const SlopeResult result = ride_on_model(model, *parse_slope_step(*slope_step(checkpoint("11.02"))));
    if (result.end != SlopeEnd::BATTLE || result.move != 2){
        return std::format("Caught on (16, 29): ended {} at move {}, expected a battle at move 3.", int(result.end), result.move + 1);
    }
    return true;
}

UnitTestResult test_parse(){
    if (parse_slope_move("step east") || parse_slope_move("roll 0") || parse_slope_move("search south 3")
        || parse_slope_move("jump 2") || !parse_slope_move("search south 3 rare-candy")
    ){
        return "Slope moves parsed wrongly.";
    }
    return true;
}

//  11.02's routes: Route 16 through the gate to the slope's top, and from
//  Route 18's water to the Safari Zone's door, the slope taken as declared.
UnitTestResult test_routes(){
    const StoryCheckpoint& row = checkpoint("11.02");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::unique_ptr<OverworldSim> sim = std::make_unique<OverworldSim>(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (step.action == SLOPE_VARIANT){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim = std::make_unique<OverworldSim>(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
            continue;
        }
        if (step.kind != Pokemon::StoryStepKind::ROUTE){
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("11.02: ended on {}; {}", to_string(sim->state().position), failure);
        }
    }
    const MapPosition end{"FuchsiaCity", {24, 6}};
    if (!(sim->state().position == end)){
        return std::format("11.02: ended on {}, not {}.", to_string(sim->state().position), to_string(end));
    }
    return true;
}

class Test_AutoStorySlope : public UnitTest{
public:
    Test_AutoStorySlope(std::string name, std::function<UnitTestResult()> body)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};

} // namespace


void add_tests_AutoStorySlope(UnitTestDatabase& database){
    database.add<Test_AutoStorySlope>("11.02 down the Cycling Road's slope with both items", test_slope_moves);
    database.add<Test_AutoStorySlope>("11.02 a queue gap while braking rolls the player", test_gap_rolls);
    database.add<Test_AutoStorySlope>("11.02 a brake past the RARE CANDY fails the search", test_search_misses);
    database.add<Test_AutoStorySlope>("11.02 caught rolling down column 14, fought, on to both items", test_caught_rolling);
    database.add<Test_AutoStorySlope>("11.02 caught on Route 18's last steps, fought, on to the water", test_caught_on_route18);
    database.add<Test_AutoStorySlope>("11.02 caught part-way along a step east, the ride stops", test_caught_stepping_east);
    database.add<Test_AutoStorySlope>("11.02 slope moves parse", test_parse);
    database.add<Test_AutoStorySlope>("11.02 Route 16 to the slope, Route 18 to the Safari Zone", test_routes);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

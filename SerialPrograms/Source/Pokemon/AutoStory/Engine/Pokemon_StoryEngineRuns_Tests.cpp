/*  Story Engine Runs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine on games of either pose type: a scripted backend for any
 *  pose type, declared poses of the game's kind only, and tile actions
 *  only for tile games. Then readers that opt in to the postcondition
 *  check, and the engine's breakpoints.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngineRuns_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += (text.empty() ? "" : ", ") + item;
    }
    return text;
}

template <StoryPose PoseType>
bool has_line(const FakeStoryBackend<PoseType>& backend, std::string_view line){
    for (const std::string& each : backend.lines()){
        if (each == line){
            return true;
        }
    }
    return false;
}

template <StoryPose PoseType>
std::string expect_inputs(const FakeStoryBackend<PoseType>& backend, std::string_view expected){
    const std::string got = joined(backend.inputs());
    return got == expected ? "" : std::format("expected inputs \"{}\", got \"{}\"", expected, got);
}

template <StoryPose PoseType>
Observation<PoseType> look(std::initializer_list<ScreenEvent> events){
    Observation<PoseType> observation;
    observation.events = ScreenEvents(events);
    return observation;
}

StoryStep game_step(std::string name, std::string action, std::string why = ""){
    StoryStep step;
    step.kind = StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = std::move(action);
    return step;
}

StoryCheckpoint checkpoint_of(std::string id, std::vector<StoryStep> steps){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.steps = std::move(steps);
    return checkpoint;
}

//  A GAME action of no args.
template <StoryPose PoseType>
StoryAction<PoseType> game_action(std::string name, StoryActionHandler<PoseType> handler){
    return {.spec = {.kind = StoryStepKind::GAME, .name = std::move(name), .args = {}}, .handler = std::move(handler)};
}

//  A game whose "flaky" action fails its first `failures` calls, whose
//  save logs "[Test] save <checkpoint>" and whose reload logs "[Test] reload".
template <StoryPose PoseType>
StoryGame<PoseType> flaky_game(size_t failures){
    StoryGame<PoseType> game;
    game.id = "test";
    std::shared_ptr<size_t> calls = std::make_shared<size_t>(0);
    game.actions.push_back(game_action<PoseType>("flaky", [calls, failures](StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
        return (*calls)++ < failures ? outcome_failed("flaky_failed") : outcome_ok();
    }));
    game.actions.push_back(game_action<PoseType>("fine", [](StoryBackend<PoseType>&, const StoryActionCall&, RunState&){
        return outcome_ok();
    }));
    game.save = [](StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint){
        backend.log(std::format("[Test] save {}", checkpoint.id));
        return outcome_ok();
    };
    game.reload = [](StoryBackend<PoseType>& backend){
        backend.log("[Test] reload");
        return outcome_ok();
    };
    return game;
}


//  A zone game's checkpoint read from JSON whose start names
//  only its zone, run on the scripted backend of its pose type.
std::string zone_checkpoint_runs(CancellableScope& scope){
    StoryGame<ZonePose> game = flaky_game<ZonePose>(0);
    FakeStoryBackend<ZonePose> backend({.looks = {look<ZonePose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<ZonePose> engine(game, backend);
    const StoryCheckpoint checkpoint = read_story_checkpoint(parse_json(R"({
        "id": "03.01", "title": "Down to the lobby", "start": {"place": "HotelZ", "spot": "Room", "heading": 180},
        "events": [{"id": "walk", "kind": "game_action", "action": "fine", "declared_after": {"place": "HotelZLobby"}}]
    })"), "zone", engine.registry().specs());
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, state);
    if (report.verdict != CheckpointVerdict::PASSED){
        return std::format("expected a pass, got {}", to_string(report.run.failure));
    }
    return state.pose.map == "HotelZLobby" ? "" : std::format("expected the run's pose in HotelZLobby, got \"{}\"", state.pose.map);
}

//  A declared tile pose is no zone.
std::string zone_rejects_tiles(CancellableScope& scope){
    StoryGame<ZonePose> game = flaky_game<ZonePose>(0);
    FakeStoryBackend<ZonePose> backend({}, scope);
    StoryEngine<ZonePose> engine(game, backend);
    StoryCheckpoint checkpoint = checkpoint_of("03.01", {game_step("walk", "fine")});
    checkpoint.start = TileDeclared{.map = "HotelZ", .tiles = {{1, 2}}, .facing = std::nullopt};
    const std::vector<std::string> lint = engine.registry().lint(checkpoint);
    if (lint.size() != 1 || !lint[0].starts_with("03.01 start: \"HotelZ\" is a map")){
        return std::format("expected one start problem, got \"{}\"", joined(lint));
    }
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, state);
    if (report.run.failure.failure != "lint_failed"){
        return std::format("expected lint_failed, got {}", to_string(report.run.failure));
    }
    return expect_inputs(backend, "");
}

//  A tile game's checkpoint that declares a place, at its start or after
//  a step, fails the lint.
std::string grid_rejects_places(CancellableScope&){
    const StoryGame<GridPose> game = flaky_game<GridPose>(0);
    const StoryActionRegistry<GridPose> registry(game);
    StoryCheckpoint checkpoint = checkpoint_of("00.01", {game_step("walk", "fine")});
    checkpoint.start = ZoneDeclared{.place = "PalletTown", .spot = std::nullopt, .heading = std::nullopt};
    checkpoint.steps[0].pose_after = ZoneDeclared{.place = "Route1", .spot = "Gate", .heading = 90};
    const std::string lint = joined(registry.lint(checkpoint));
    const std::string expected =
        "00.01 start: \"PalletTown\" is a place, but this game's poses are tiles: declare a map, tiles and a facing, "
        "00.01 step 1 walk: declared_after: \"Route1\" is a place, but this game's poses are tiles: declare a map, tiles and a facing";
    return lint == expected ? "" : std::format("expected \"{}\", got \"{}\"", expected, lint);
}

//  A zone game may not use tile args: not in a step (a route's GRID_POSE
//  target), not in its own actions (a LEGS arg). Timed legs are fine.
std::string zone_rejects_tile_args(CancellableScope&){
    StoryGame<ZonePose> game;
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "walk", .args = {
            {.key = "legs", .type = StoryArgType::LEGS, .required = true, .fallback = std::nullopt, .why = ""},
        }},
        .handler = nullptr,
    });
    game.actions.push_back({
        .spec = {.kind = StoryStepKind::GAME, .name = "timed_walk", .args = {
            {.key = "legs", .type = StoryArgType::TIMED_LEGS, .required = true, .fallback = std::nullopt, .why = ""},
        }},
        .handler = nullptr,
    });
    const StoryActionRegistry<ZonePose> registry(game);
    StoryStep route;
    route.kind = StoryStepKind::ROUTE;
    route.name = "route";
    route.args = {{"target", GridPose{.map = "HotelZ", .tiles = {}, .facing = std::nullopt}}};
    StoryStep timed;
    timed.kind = StoryStepKind::GAME;
    timed.name = "timed";
    timed.action = "timed_walk";
    timed.args = {{"legs", std::vector<StoryTimedLeg>{{.turn = 90, .ms = 1200}}}};
    const std::string lint = joined(registry.lint(checkpoint_of("03.01", {route, timed})));
    const std::string expected =
        "action walk takes a tile argument \"legs\", but this game has no tiles, "
        "03.01 step 1 route: argument \"target\" is a tile one, but this game has no tiles";
    return lint == expected ? "" : std::format("expected \"{}\", got \"{}\"", expected, lint);
}

//  No tile actions in a zone game, so a legs step fails the lint;
//  a tile game keeps them.
std::string tile_actions_only_with_tiles(CancellableScope&){
    StoryStep legs;
    legs.kind = StoryStepKind::LEGS;
    legs.name = "legs";
    legs.args = {{"legs", std::vector<StoryLeg>{{Direction::NORTH, 1}}}};
    StoryStep face;
    face.kind = StoryStepKind::STEP;
    face.name = "face";
    face.action = "face";
    face.args = {{"direction", Direction::EAST}};
    const StoryGame<ZonePose> zone_game;
    const StoryActionRegistry<ZonePose> zone(zone_game);
    if (joined(zone.check(legs)) != "unknown action \"\" for LEGS" || joined(zone.check(face)) != "unknown action \"face\" for STEP"){
        return std::format("expected the zone game to have no legs or face, got \"{}\", \"{}\"", joined(zone.check(legs)), joined(zone.check(face)));
    }
    const StoryGame<GridPose> grid_game;
    const StoryActionRegistry<GridPose> grid(grid_game);
    if (!grid.check(legs).empty() || !grid.check(face).empty()){
        return "expected the tile game to keep legs and face";
    }
    return "";
}

//  A reader that logs "[Test] read <name>" when it runs and fills nothing.
StoryGameReader<GridPose> logging_reader(std::string name, std::vector<std::string> supplies, StoryReaderUse use){
    StoryGameReader<GridPose> reader;
    reader.name = name;
    reader.supplies = std::move(supplies);
    reader.use = use;
    reader.read = [name](StoryBackend<GridPose>& backend, Observation<GridPose>&){
        backend.log(std::format("[Test] read {}", name));
    };
    return reader;
}

//  The readers `checkpoint`'s postcondition check runs on a backend of
//  `kind`, as "pose, bag".
std::string readers_run(const StoryCheckpoint& checkpoint, StoryBackendKind kind, CancellableScope& scope){
    StoryGame<GridPose> game;
    game.readers.push_back(logging_reader("pose", {"pose"}, StoryReaderUse::AFTER_CHECKPOINT));
    game.readers.push_back(logging_reader("bag", {"bag.*"}, StoryReaderUse::AFTER_CHECKPOINT));
    game.readers.push_back(logging_reader("heading", {"heading"}, StoryReaderUse::ON_REQUEST));
    StoryGameReader<GridPose> flags = logging_reader("flags", {"flag.*"}, StoryReaderUse::AFTER_CHECKPOINT);
    flags.console_only = true;
    game.readers.push_back(std::move(flags));
    FakeStoryBackend<GridPose> backend({.kind = kind}, scope);
    StoryEngine<GridPose> engine(game, backend);
    engine.check_postconditions(checkpoint);
    std::vector<std::string> read;
    for (const std::string& line : backend.lines()){
        if (line.starts_with("[Test] read ")){
            read.push_back(line.substr(12));
        }
    }
    return joined(read);
}

//  The postcondition check runs only the readers that opted in and supply
//  what it needs, and no console_only one off a console; never the
//  heading, which opted out.
std::string readers_opt_in(CancellableScope& scope){
    StoryCheckpoint items = checkpoint_of("01", {});
    items.postconditions.items = {{.item = "POTION", .quantity = 1}};
    std::string read = readers_run(items, StoryBackendKind::SCRIPT, scope);
    if (read != "bag"){
        return std::format("expected only the bag read for an item, got \"{}\"", read);
    }
    StoryCheckpoint place = checkpoint_of("02", {});
    place.postconditions.position = GridPose{.map = "Town", .tiles = {{1, 2}}, .facing = std::nullopt};
    place.postconditions.flags = {"DOOR"};
    read = readers_run(place, StoryBackendKind::SCRIPT, scope);
    if (read != "pose"){
        return std::format("expected only the pose read off a console, got \"{}\"", read);
    }
    read = readers_run(place, StoryBackendKind::CONSOLE, scope);
    if (read != "pose, flags"){
        return std::format("expected the pose and the console's flags read, got \"{}\"", read);
    }
    read = readers_run(checkpoint_of("03", {}), StoryBackendKind::CONSOLE, scope);
    return read.empty() ? "" : std::format("expected no reader without postconditions, got \"{}\"", read);
}

//  A game whose "mark" action records "run <step>" and whose pause hook
//  records "pause <label>", in `events`, in `mode`.
StoryGame<GridPose> breakpoint_game(StoryBreakpoints mode, const std::shared_ptr<std::vector<std::string>>& events){
    StoryGame<GridPose> game;
    game.id = "test";
    game.actions.push_back(game_action<GridPose>("mark", [events](StoryBackend<GridPose>&, const StoryActionCall& call, RunState&){
        events->push_back(std::format("run {}", call.step.name));
        return outcome_ok();
    }));
    game.breakpoints = mode;
    game.pause_point = [events](std::string_view label){
        events->push_back(std::format("pause {}", label));
    };
    return game;
}

StoryStep legs_step(std::string name){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = std::move(name);
    step.args = {{"legs", std::vector<StoryLeg>{{Direction::NORTH, 2}, {Direction::EAST, 3}}}};
    return step;
}

//  What `events` should be, as one line each, or what differs.
std::string expect_events(const std::vector<std::string>& events, const std::vector<std::string>& expected){
    return events == expected ? "" : std::format("expected [{}], got [{}]", joined(expected), joined(events));
}

//  Breakpoints in STEPS mode: the hook runs before each step, in order,
//  labelled with the checkpoint, the step's place and its name; a walk's
//  legs do not pause.
std::string pauses_before_each_step(CancellableScope& scope){
    const auto events = std::make_shared<std::vector<std::string>>();
    const StoryGame<GridPose> game = breakpoint_game(StoryBreakpoints::STEPS, events);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const StoryCheckpoint checkpoint = checkpoint_of("12.03", {game_step("first", "mark"), legs_step("shore"), game_step("last", "mark")});
    RunState state;
    state.pose = tile_pose("Shore", {TilePos{5, 5}}, Direction::NORTH);
    if (!engine.run_checkpoint(checkpoint, state).run.completed){
        return joined(backend.lines());
    }
    const std::string error = expect_events(*events, {
        "pause 12.03/1 first", "run first", "pause 12.03/2 shore", "pause 12.03/3 last", "run last",
    });
    if (!error.empty()){
        return error;
    }
    return has_line(backend, "[Break] 12.03/1 first: paused") && has_line(backend, "[Break] 12.03/1 first: continued")
        ? "" : "expected the [Break] paused and continued lines";
}

//  LEGS mode adds a pause before each leg, labelled with the step, the leg's
//  place, direction, tiles and the tile it ends on.
std::string pauses_before_each_leg(CancellableScope& scope){
    const auto events = std::make_shared<std::vector<std::string>>();
    const StoryGame<GridPose> game = breakpoint_game(StoryBreakpoints::LEGS, events);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const StoryCheckpoint checkpoint = checkpoint_of("12.03", {game_step("first", "mark"), legs_step("shore")});
    RunState state;
    state.pose = tile_pose("Shore", {TilePos{5, 5}}, Direction::NORTH);
    if (!engine.run_checkpoint(checkpoint, state).run.completed){
        return joined(backend.lines());
    }
    const std::string error = expect_events(*events, {
        "pause 12.03/1 first", "run first", "pause 12.03/2 shore",
        "pause 12.03/2 shore leg 1/2 north 2 -> (5, 3)", "pause 12.03/2 shore leg 2/2 east 3 -> (8, 3)",
    });
    if (!error.empty()){
        return error;
    }
    //  The pause is before the leg's input.
    return expect_inputs(backend, "tap north, tap north, tap east, tap east, tap east");
}

//  A leg's label has no destination when the walk starts on no single tile.
std::string leg_label_without_a_known_tile(CancellableScope& scope){
    const auto events = std::make_shared<std::vector<std::string>>();
    const StoryGame<GridPose> game = breakpoint_game(StoryBreakpoints::LEGS, events);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const StoryCheckpoint checkpoint = checkpoint_of("12.03", {legs_step("shore")});
    RunState state;
    engine.run_checkpoint(checkpoint, state);
    return expect_events(*events, {
        "pause 12.03/1 shore", "pause 12.03/1 shore leg 1/2 north 2", "pause 12.03/1 shore leg 2/2 east 3",
    });
}

//  OFF, and a hook-less game, never pause or log.
std::string off_never_pauses(CancellableScope& scope){
    const auto events = std::make_shared<std::vector<std::string>>();
    StoryGame<GridPose> game = breakpoint_game(StoryBreakpoints::OFF, events);
    FakeStoryBackend<GridPose> backend({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> engine(game, backend);
    const StoryCheckpoint checkpoint = checkpoint_of("12.03", {game_step("first", "mark"), legs_step("shore")});
    RunState state;
    engine.run_checkpoint(checkpoint, state);
    const std::string error = expect_events(*events, {"run first"});
    if (!error.empty()){
        return error;
    }
    StoryGame<GridPose> no_hook = breakpoint_game(StoryBreakpoints::LEGS, events);
    no_hook.pause_point = {};
    FakeStoryBackend<GridPose> quiet({.looks = {look<GridPose>({ScreenEvent::PLAYER_CAN_MOVE})}}, scope);
    StoryEngine<GridPose> quiet_engine(no_hook, quiet);
    state = RunState();
    quiet_engine.run_checkpoint(checkpoint, state);
    for (const std::string& line : quiet.lines()){
        if (line.starts_with("[Break]")){
            return std::format("a game with no hook logged {}", line);
        }
    }
    return "";
}

//  A named test; it returns its error, empty when it passed.
struct RunsTest{
    std::string_view name;
    std::string (*check)(CancellableScope& scope);
};

const RunsTest RUNS_TESTS[] = {
    {.name = "zone checkpoint on a fake backend", .check = &zone_checkpoint_runs},
    {.name = "zone start with tiles fails the lint", .check = &zone_rejects_tiles},
    {.name = "tile start with a place fails the lint", .check = &grid_rejects_places},
    {.name = "zone game rejects tile args", .check = &zone_rejects_tile_args},
    {.name = "tile actions only with tiles", .check = &tile_actions_only_with_tiles},
    {.name = "readers opt in to postconditions", .check = &readers_opt_in},
    {.name = "breakpoints pause before each step", .check = &pauses_before_each_step},
    {.name = "breakpoints pause before each leg", .check = &pauses_before_each_leg},
    {.name = "breakpoints leg label needs a known tile", .check = &leg_label_without_a_known_tile},
    {.name = "breakpoints off never pauses", .check = &off_never_pauses},
};

class StoryEngineRunsTest : public UnitTest{
public:
    explicit StoryEngineRunsTest(RunsTest test)
        : UnitTest(std::format("Pokemon::StoryEngine runs - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = m_case.check(scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    RunsTest m_case;
};


} // namespace


void add_tests_StoryEngineRuns(UnitTestDatabase& database){
    for (const RunsTest& test : RUNS_TESTS){
        database.add<StoryEngineRunsTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

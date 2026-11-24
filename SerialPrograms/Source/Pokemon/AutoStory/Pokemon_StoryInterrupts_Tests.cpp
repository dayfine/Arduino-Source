/*  Story Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  clear_story_interrupt() and judge_settle() against a fake game: scripted
 *  screens and handlers that note what they cleared.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryInterrupts.h"
#include "Pokemon_StoryInterrupts_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const WallClock T0 = WallClock() + std::chrono::hours(1);

struct Frame{
    int ms;
    ScreenEvents events;
};

const ScreenEvents WALK{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};
const ScreenEvents BANNER{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents HOME{ScreenEvent::SWITCH_HOME_MENU};

ScreenLook look_of(const std::vector<Frame>& frames, int until_ms){
    ScreenLook look;
    for (const Frame& frame : frames){
        look.log.record(frame.events, T0 + Milliseconds(frame.ms));
    }
    look.window = {T0, T0 + Milliseconds(until_ms)};
    return look;
}

ScreenLook quiet(){
    return look_of({{0, WALK}, {300, WALK}}, 300);
}
ScreenLook dialog(){
    return look_of({{0, WALK}, {100, BOX}}, 300);
}
ScreenLook wild(){
    return look_of({{0, WALK}, {100, WHITE}, {400, WALK}, {1500, BATTLE}}, 2000);
}
ScreenLook trainer(){
    return look_of({{0, WALK}, {100, BOX}, {2000, BLACK}, {2500, BATTLE}}, 3000);
}
ScreenLook warp(){
    return look_of({{0, WALK}, {100, BLACK}, {600, WALK}, {900, BANNER}}, 1200);
}
ScreenLook white_unsettled(){
    return look_of({{0, WALK}, {100, WHITE}, {700, WALK}}, 1000);
}
//  A trainer's battle after her lines were pressed through.
ScreenLook battle_after_lines(){
    return look_of({{0, WALK}, {100, BLACK}, {1500, BATTLE}}, 2000);
}
ScreenLook trainer_opening(){
    return look_of({{0, WALK}, {100, BOX}, {2000, BLACK}}, 2100);
}
ScreenLook black_unsettled(){
    return look_of({{0, WALK}, {100, BLACK}, {700, WALK}}, 1000);
}
ScreenLook white_settled(){
    return look_of({{0, WALK}, {100, WHITE}, {700, WALK}, {10000, WALK}}, 10000);
}
ScreenLook home_menu(){
    return look_of({{0, HOME}, {300, HOME}}, 300);
}
ScreenLook home_read_as_overworld(){
    ScreenEvents both = HOME;
    both.set(ScreenEvent::PLAYER_CAN_MOVE, true);
    return look_of({{0, both}}, 300);
}


//  Shows `screens`, the last one again once they run out. Each clear shows
//  the next of `after_clears`, or a quiet screen once they run out.
struct FakeGame{
    std::vector<ScreenLook> screens;
    std::vector<std::vector<ScreenLook>> after_clears;
    size_t looks = 0;
    size_t clears = 0;
    std::vector<std::string> trace;

    ScreenLook look(bool fresh){
        trace.push_back(fresh ? "look" : "look again");
        const ScreenLook& out = screens.empty() ? quiet() : screens[std::min(looks, screens.size() - 1)];
        looks++;
        return out;
    }
    void clear(std::string_view name){
        trace.push_back(std::format("clear {}", name));
        screens = clears < after_clears.size() ? after_clears[clears] : std::vector<ScreenLook>{};
        clears++;
        looks = 0;
    }
};

InterruptHandler noting(std::string name, InterruptPose pose, std::string failure, FakeGame& game){
    return InterruptHandler{
        .handle = [&game, name, failure](const InterruptVerdict&) -> std::optional<StoryFailure>{
            game.clear(name);
            if (failure.empty()){
                return std::nullopt;
            }
            return StoryFailure{.code = failure};
        },
        .pose = pose,
    };
}

StoryInterrupts fake_interrupts(FakeGame& game){
    StoryInterrupts interrupts;
    interrupts.look = [&game](bool fresh){
        return game.look(fresh);
    };
    interrupts.handlers.add(InterruptKind::WILD_ENCOUNTER, noting("wild", InterruptPose::UNCHANGED, "", game));
    interrupts.handlers.add(InterruptKind::TRAINER_SIGHT, noting("trainer", InterruptPose::UNCHANGED, "", game));
    interrupts.handlers.add(InterruptKind::DIALOG, noting("dialog", InterruptPose::UNCHANGED, "", game));
    interrupts.handlers.add(InterruptKind::WARP, noting("warp", InterruptPose::LOST, "", game));
    interrupts.handlers.starts_on("scene", InterruptKind::DIALOG);
    return interrupts;
}

UnitTestResult expect_trace(const FakeGame& game, const std::vector<std::string>& expected){
    if (game.trace == expected){
        return true;
    }
    std::string out = "Expected the trace:";
    for (const std::string& line : expected){
        out += std::format("\n  {}", line);
    }
    out += "\ngot:";
    for (const std::string& line : game.trace){
        out += std::format("\n  {}", line);
    }
    return out;
}


UnitTestResult test_quiet_screen(){
    FakeGame game;
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (check.failure || check.verdict.kind != InterruptKind::NONE || check.pose_lost){
        return "Expected no interrupt and no pose loss on quiet screen.";
    }
    return expect_trace(game, {"look"});
}

UnitTestResult test_cleared(ScreenLook screen, InterruptKind expected_kind, const std::string& handler, bool expect_pose_lost){
    FakeGame game;
    game.screens = {std::move(screen)};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (check.failure){
        return std::format("Expected success, got {}", *check.failure);
    }
    if (check.verdict.kind != expected_kind){
        return std::format("Expected {}, got {}", interrupt_kind_name(expected_kind), interrupt_kind_name(check.verdict.kind));
    }
    if (check.pose_lost != expect_pose_lost){
        return std::format("Expected pose_lost={}, got {}", expect_pose_lost, check.pose_lost);
    }
    return expect_trace(game, {"look", std::format("clear {}", handler), "look"});
}

UnitTestResult test_wild_encounter(){
    return test_cleared(wild(), InterruptKind::WILD_ENCOUNTER, "wild", false);
}
UnitTestResult test_trainer_sight(){
    return test_cleared(trainer(), InterruptKind::TRAINER_SIGHT, "trainer", false);
}
UnitTestResult test_dialog(){
    return test_cleared(dialog(), InterruptKind::DIALOG, "dialog", false);
}
UnitTestResult test_warp(){
    return test_cleared(warp(), InterruptKind::WARP, "warp", true);
}

UnitTestResult test_step_starts_on_it(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "scene", "scene");
    if (check.failure || check.verdict.kind != InterruptKind::NONE){
        return "Expected scene action to start on dialog.";
    }
    return expect_trace(game, {"look"});
}

UnitTestResult test_handler_fails_the_step(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers = InterruptHandlerTable();
    interrupts.handlers.add(InterruptKind::DIALOG, noting("dialog", InterruptPose::UNCHANGED, "still_up", game));
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "interrupt_failed" || check.failure->detail != "dialog: still_up"){
        return std::format("Expected interrupt_failed: dialog: still_up, got {}", check.failure ? std::format("{}", *check.failure) : std::string("none"));
    }
    return expect_trace(game, {"look", "clear dialog"});
}

UnitTestResult test_unhandled_kind(){
    FakeGame game;
    game.screens = {trainer()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers = InterruptHandlerTable();
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "unhandled_interrupt"){
        return "Expected unhandled_interrupt.";
    }
    return expect_trace(game, {"look"});
}

UnitTestResult test_unsettled_looks_again(){
    FakeGame game;
    game.screens = {white_unsettled(), white_settled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (check.failure || check.verdict.kind != InterruptKind::WARP){
        return "Expected unsettled white to settle as warp.";
    }
    return expect_trace(game, {"look", "look again", "clear warp", "look"});
}

UnitTestResult test_unsettled_is_bounded(){
    FakeGame game;
    game.screens = {black_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "interrupt_unsettled"){
        return "Expected interrupt_unsettled after max_looks.";
    }
    return expect_trace(game, {"look", "look again", "look again"});
}

UnitTestResult test_unsettled_before_a_battle(){
    FakeGame game;
    game.screens = {white_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers.starts_on("battle", InterruptKind::WILD_ENCOUNTER);
    const InterruptCheck check = clear_story_interrupt(interrupts, "starly", "battle");
    if (check.failure || check.verdict.kind != InterruptKind::NONE){
        return "Expected battle action to start on unsettled opening.";
    }
    return expect_trace(game, {"look"});
}

UnitTestResult test_opening_before_a_walk(ScreenLook opening, ScreenLook battle, const std::string& handler){
    FakeGame game;
    game.screens = {opening, opening, opening, opening, std::move(battle)};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (check.failure){
        return std::format("Expected opening cleared, got {}", *check.failure);
    }
    return expect_trace(game, {
        "look", "look again", "look again", "look again", "look again",
        std::format("clear {}", handler), "look",
    });
}
UnitTestResult test_wild_opening_before_a_walk(){
    return test_opening_before_a_walk(white_unsettled(), wild(), "wild");
}
UnitTestResult test_trainer_opening_before_a_walk(){
    return test_opening_before_a_walk(trainer_opening(), trainer(), "trainer");
}

UnitTestResult test_lines_then_battle(){
    FakeGame game;
    game.screens = {dialog()};
    game.after_clears = {{battle_after_lines()}};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (check.failure || check.verdict.kind != InterruptKind::TRAINER_SIGHT){
        return std::format(
            "Expected the trainer's battle cleared, got {} {}",
            interrupt_kind_name(check.verdict.kind), check.failure ? std::format("{}", *check.failure) : std::string("")
        );
    }
    return expect_trace(game, {"look", "clear dialog", "look", "clear trainer", "look"});
}

UnitTestResult test_interrupts_are_bounded(){
    FakeGame game;
    game.screens = {dialog()};
    game.after_clears = {{dialog()}, {dialog()}, {dialog()}};
    StoryInterrupts interrupts = fake_interrupts(game);
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "too_many_interrupts"){
        return "Expected too_many_interrupts after max_per_step clears.";
    }
    return expect_trace(game, {"look", "clear dialog", "look", "clear dialog", "look", "clear dialog", "look"});
}

UnitTestResult test_opening_is_bounded(){
    FakeGame game;
    game.screens = {white_unsettled()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.max_opening_looks = 5;
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "interrupt_unsettled"){
        return "Expected interrupt_unsettled after max_opening_looks.";
    }
    return expect_trace(game, {"look", "look again", "look again", "look again", "look again"});
}

UnitTestResult test_table_rejects(){
    InterruptHandlerTable table;
    table.add(InterruptKind::DIALOG, {});
    for (InterruptKind kind : {InterruptKind::NONE, InterruptKind::UNSETTLED, InterruptKind::DIALOG}){
        try{
            table.add(kind, {});
            return std::format("Expected a handler for {} rejected.", interrupt_kind_name(kind));
        }catch (InternalProgramError&){}
    }
    if (table.find(InterruptKind::DIALOG) == nullptr || table.find(InterruptKind::WARP) != nullptr){
        return "Expected the dialog's handler found and no warp's.";
    }
    return true;
}

UnitTestResult test_home_menu_before_a_step(){
    for (std::string_view action : {"save", "route", "battle"}){
        FakeGame game;
        game.screens = {home_menu()};
        StoryInterrupts interrupts = fake_interrupts(game);
        interrupts.handlers.starts_on("battle", InterruptKind::TRAINER_SIGHT);
        const InterruptCheck check = clear_story_interrupt(interrupts, "save", action);
        if (!check.failure || check.failure->code != GAME_NOT_RUNNING || !check.failure->repeats){
            return "Expected repeating game_not_running on HOME menu.";
        }
        if (UnitTestResult trace = expect_trace(game, {"look"}); trace.result != UnitTestResult::PASSED){
            return trace;
        }
    }
    return true;
}

UnitTestResult test_handler_throws(){
    FakeGame game;
    game.screens = {dialog()};
    StoryInterrupts interrupts = fake_interrupts(game);
    interrupts.handlers = InterruptHandlerTable();
    interrupts.handlers.add(InterruptKind::DIALOG, InterruptHandler{
        .handle = [&game](const InterruptVerdict&) -> std::optional<StoryFailure>{
            game.trace.push_back("clear dialog");
            throw OperationFailedException(ErrorReportMode::NO_ERROR_REPORT, "the console is gone");
        },
        .pose = InterruptPose::UNCHANGED,
    });
    const InterruptCheck check = clear_story_interrupt(interrupts, "walk", "route");
    if (!check.failure || check.failure->code != "interrupt_failed"){
        return "Expected interrupt_failed when handler throws.";
    }
    return expect_trace(game, {"look", "clear dialog"});
}

UnitTestResult test_settle_of_a_look(){
    struct Row{
        std::string name;
        ScreenLook look;
        std::string expected;
    };
    const std::vector<Row> rows = {
        {"no frame", ScreenLook{}, "no frame seen"},
        {"a quiet overworld", quiet(), "settled overworld (quiet 0.3 s)"},
        {"a box", dialog(), "dialog box, player can move showing (quiet 0.2 s)"},
        {"a door's fade back up", black_unsettled(), "settled overworld (quiet 0.3 s)"},
        {"the HOME menu", home_menu(), "Switch HOME menu (quiet 0.3 s)"},
        {"HOME read as a quiet overworld too", home_read_as_overworld(), "Switch HOME menu (quiet 0.3 s)"},
    };
    for (const Row& row : rows){
        const std::string got = describe(judge_settle(row.look));
        if (got != row.expected){
            return std::format("{}: expected \"{}\", got \"{}\"", row.name, row.expected, got);
        }
    }
    return true;
}


struct StoryInterruptsCase{
    struct Input{
        UnitTestResult (*test)();
    };

    struct Expectation{
        std::string_view description;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

class Test_StoryInterrupts : public UnitTest{
public:
    explicit Test_StoryInterrupts(StoryInterruptsCase scenario)
        : UnitTest(std::format("Pokemon::StoryInterrupts - {}", scenario.name))
        , m_scenario(scenario)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_scenario.input.test();
    }

private:
    StoryInterruptsCase m_scenario;
};

const StoryInterruptsCase STORY_INTERRUPTS_CASES[] = {
    {.name = "a quiet screen: one look", .input = {.test = test_quiet_screen}, .expectation = {.description = "nothing cleared"}},
    {.name = "a wild encounter before a step", .input = {.test = test_wild_encounter}, .expectation = {.description = "the wild handler"}},
    {.name = "a trainer's sight before a step", .input = {.test = test_trainer_sight}, .expectation = {.description = "the trainer handler"}},
    {.name = "a dialog before a step", .input = {.test = test_dialog}, .expectation = {.description = "the dialog handler"}},
    {.name = "a warp before a step", .input = {.test = test_warp}, .expectation = {.description = "the warp handler, pose lost"}},
    {.name = "a step that starts on it", .input = {.test = test_step_starts_on_it}, .expectation = {.description = "a scene's box is the step's"}},
    {.name = "a handler that fails fails the step", .input = {.test = test_handler_fails_the_step}, .expectation = {.description = "interrupt_failed"}},
    {.name = "a kind with no handler fails the step", .input = {.test = test_unhandled_kind}, .expectation = {.description = "unhandled_interrupt"}},
    {.name = "unsettled: look again until it settles", .input = {.test = test_unsettled_looks_again}, .expectation = {.description = "white settles as warp"}},
    {.name = "unsettled: at most max_looks looks", .input = {.test = test_unsettled_is_bounded}, .expectation = {.description = "interrupt_unsettled"}},
    {.name = "unsettled: a battle's opening before a battle step", .input = {.test = test_unsettled_before_a_battle}, .expectation = {.description = "the step's, after one look"}},
    {.name = "unsettled: a wild battle's opening before a walk", .input = {.test = test_wild_opening_before_a_walk}, .expectation = {.description = "waits for battle and clears it"}},
    {.name = "unsettled: a trainer battle's opening before a walk", .input = {.test = test_trainer_opening_before_a_walk}, .expectation = {.description = "waits for battle and clears it"}},
    {.name = "a trainer's lines, then her battle", .input = {.test = test_lines_then_battle}, .expectation = {.description = "dialog cleared, looked again, the battle fought as a trainer's"}},
    {.name = "at most max_per_step interrupts a step", .input = {.test = test_interrupts_are_bounded}, .expectation = {.description = "too_many_interrupts"}},
    {.name = "unsettled: at most max_opening_looks for an opening", .input = {.test = test_opening_is_bounded}, .expectation = {.description = "interrupt_unsettled"}},
    {.name = "table: one handler a kind that can be cleared", .input = {.test = test_table_rejects}, .expectation = {.description = "rejects invalid kinds"}},
    {.name = "the HOME menu fails as game_not_running", .input = {.test = test_home_menu_before_a_step}, .expectation = {.description = "stops with repeats=true"}},
    {.name = "a handler that throws fails as interrupt_failed", .input = {.test = test_handler_throws}, .expectation = {.description = "catches OperationFailedException"}},
    {.name = "judge_settle describes each look", .input = {.test = test_settle_of_a_look}, .expectation = {.description = "settled, fading, HOME, none"}},
};

} // namespace


void add_tests_StoryInterrupts(UnitTestDatabase& database){
    for (const StoryInterruptsCase& scenario : STORY_INTERRUPTS_CASES){
        database.add<Test_StoryInterrupts>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation

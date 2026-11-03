/*  AutoStory Abilities Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The LEVEL step, and which ends of the MART, TEACH, GRIND and fossil
 *  steps the story takes as success, failure or a stop.
 *
 */

#include <format>
#include <functional>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "PokemonFRLG_AutoStoryAbilities.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

//  A game where nothing happens: a lead whose level is set, a Mart whose
//  money is set, a patch whose one battle is shiny if set.
class StubMart : public MartActions{
public:
    size_t money = 3000;

    virtual bool open_buy_list() override{ return true; }
    virtual std::optional<size_t> read_money() override{ return money; }
    virtual void move_cursor(size_t, size_t) override{}
    virtual std::string read_row(size_t) override{ return ""; }
    virtual PurchaseEnd buy(const MartBuy&) override{ return PurchaseEnd::FAILED; }
    virtual bool leave() override{ return true; }
};

class StubTeach : public TeachActions{
public:
    //  Its TM Case is empty.
    virtual ItemOpen open_item(std::string_view, std::string_view) override{ return ItemOpen::NOT_IN_CASE; }
    virtual TeachPrompt choose_member(size_t) override{ return TeachPrompt::FAILED; }
    virtual ForgetEnd forget(size_t) override{ return ForgetEnd::FAILED; }
    virtual void stop_teaching() override{}
    virtual bool leave() override{ return true; }
    virtual void dump_screen(std::string_view) override{}
};

class StubPatch : public GrindActions{
public:
    bool shiny = false;

    virtual bool find_encounter(Pokemon::Direction) override{ return true; }
    virtual EncounterSeen read_encounter() override{ return {.species = "pidgey", .shiny = shiny}; }
    virtual GrindFight fight() override{ return GrindFight::FAILED; }
    virtual bool needs_heal() override{ return false; }
    virtual bool heal_trip() override{ return true; }
    virtual std::optional<WinEstimate> win_chance(const MemberInfo&, std::string_view) override{ return std::nullopt; }
};

class StubFossils : public ChoiceActions{
public:
    virtual bool talk() override{ return false; }
    virtual std::optional<Fossil> read_offer() override{ return std::nullopt; }
    virtual void answer(bool) override{}
    virtual bool finish() override{ return true; }
    virtual std::optional<Fossil> fossil_in_bag() override{ return std::nullopt; }
};

struct StubGame{
    std::optional<unsigned> level = 16;
    StubMart mart;
    StubTeach teach;
    StubPatch patch;
    StubFossils fossils;
};


class Test_AutoStoryAbilities : public UnitTest{
public:
    Test_AutoStoryAbilities(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStoryAbilities - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};


Pokemon::StoryStep game_step(std::string variant){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = "test";
    step.variant = std::move(variant);
    return step;
}

//  The failure name the step ends with ("" for success); "none" if it
//  isn't one of these steps.
std::string run_step(StubGame& game, const Pokemon::StoryStep& step){
    QuietLogger logger;
    const MemberReader members = [&game](size_t slot) -> std::optional<MemberInfo>{
        if (slot != 0 || !game.level){
            return std::nullopt;
        }
        return MemberInfo{.species = "squirtle", .level = game.level, .moves = {"water-gun", "surf"}};
    };
    StoryAbilities abilities{members, game.mart, game.teach, game.patch, game.fossils};
    const std::optional<Pokemon::Outcome> outcome = run_ability_game_step(logger, nullptr, step, abilities);
    return outcome ? outcome->failure : "none";
}

struct StepCase{
    std::string name;
    std::function<void(StubGame&)> setup;
    Pokemon::StoryStep step;
    std::string failure;
};

std::vector<StepCase> step_cases(){
    const auto none = [](StubGame&){};
    const Pokemon::StoryStep grind = grind_step("grind", "", {.member = 0, .level = 20, .max_encounters = 5, .pace = Pokemon::Direction::EAST, .heal_every = 0}, {});
    return {
        {"level reached", none, level_step("check", "", {.slot = 0, .level = 16}), ""},
        {"level below", none, level_step("check", "", {.slot = 0, .level = 17}), "level_below"},
        {"level unread", [](StubGame& game){ game.level.reset(); }, level_step("check", "", {.slot = 0, .level = 16}), "level_unread"},
        {"a Mart with too little money succeeds", [](StubGame& game){ game.mart.money = 100; }, game_step("mart PewterCity_Mart budget=2000 potion=5"), ""},
        {"a Mart purchase that fails", none, game_step("mart PewterCity_Mart budget=2000 potion=5"), "mart_failed"},
        {"a move it knows already succeeds", none, game_step("teach hm03 surf slot=1"), ""},
        {"a TM not in the TM Case is skipped", none, game_step("teach tm03 water-pulse slot=1 forget=water-gun"), ""},
        {"a level that can't be read stops a GRIND", [](StubGame& game){ game.level.reset(); }, grind, "grind_unread"},
        {"a GRIND without a nurse", none, game_step("grind slot=1 level=20"), "bad_variant"},
        {"no YES/NO at the fossil", none, fossil_step("fossil", "", Fossil::HELIX), "fossil_failed"},
        {"a bad variant", none, game_step("mart PewterCity_Mart"), "bad_variant"},
        {"another game step", none, game_step("capture rattata"), "none"},
    };
}

UnitTestResult test_ability_steps(){
    for (const StepCase& item : step_cases()){
        StubGame game;
        item.setup(game);
        const std::string failure = run_step(game, item.step);
        if (failure != item.failure){
            return std::format("{}: \"{}\", expected \"{}\".", item.name, failure, item.failure);
        }
    }
    return true;
}


//  A shiny in a GRIND stops the program, not the checkpoint: a reload
//  would lose it.
UnitTestResult test_grind_shiny_stops(){
    StubGame game;
    game.patch.shiny = true;
    Pokemon::StoryStep step = grind_step("grind", "", {.member = 0, .level = 20, .max_encounters = 5, .pace = Pokemon::Direction::EAST, .heal_every = 0}, {});
    try{
        run_step(game, step);
    }catch (ProgramFinishedException&){
        return true;
    }
    return "Expected the program to stop on a shiny.";
}


UnitTestResult test_level_variant(){
    const std::string variant = level_variant({.slot = 1, .level = 23});
    const std::optional<LevelCheck> back = parse_level_variant(variant);
    if (variant != "level slot=2 min=23" || !back || back->slot != 1 || back->level != 23){
        return "Unexpected level variant \"" + variant + "\".";
    }
    for (std::string_view bad : {"level", "level slot=1", "level min=5", "level slot=0 min=5", "level slot=1 min=101"}){
        if (parse_level_variant(bad)){
            return "Expected \"" + std::string(bad) + "\" refused.";
        }
    }
    return true;
}

}


void add_tests_AutoStoryAbilities(UnitTestDatabase& database){
    const auto add = [&](std::string name, UnitTestResult (*body)()){
        database.add<Test_AutoStoryAbilities>(std::move(name), body);
    };
    add("the steps' ends as outcomes", test_ability_steps);
    add("a shiny in a GRIND stops the program", test_grind_shiny_stops);
    add("the LEVEL step's variant", test_level_variant);
    add_tests_AutoStoryMart(database);
    add_tests_AutoStoryTeach(database);
    add_tests_AutoStoryGrind(database);
    add_tests_AutoStoryChoice(database);
}



}
}
}

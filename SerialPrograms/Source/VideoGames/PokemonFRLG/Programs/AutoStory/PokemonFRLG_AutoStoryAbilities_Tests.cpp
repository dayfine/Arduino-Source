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

//  A game where nothing happens: a lead whose level is set, a patch whose one battle is shiny if set.
class StubMart : public MartActions{
public:
    virtual bool open_buy_list() override{ return true; }
    virtual void press_cursor(bool) override{}
    virtual std::string read_row(size_t) override{ return ""; }
    virtual void dump_screen(std::string_view) override{}
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


struct AutoStoryAbilitiesInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryAbilitiesExpectation{
};

struct AutoStoryAbilitiesCase{
    AutoStoryAbilitiesInput input;
    AutoStoryAbilitiesExpectation expectation;
};

class Test_AutoStoryAbilities : public UnitTest{
public:
    explicit Test_AutoStoryAbilities(AutoStoryAbilitiesCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryAbilities - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryAbilitiesCase m_case;
};


Pokemon::StoryStep game_step(std::string action, Pokemon::StoryActionArgs args){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = "test";
    step.action = std::move(action);
    step.args = std::move(args);
    return step;
}

//  The failure name the step ends with ("" for success, "unchanged" for
//  one that changed nothing in the game); "none" if it isn't one of these
//  steps.
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
    if (!outcome){
        return "none";
    }
    return outcome->changed_nothing ? "unchanged" : outcome->failure;
}

struct StepCase{
    std::string name;
    std::function<void(StubGame&)> setup;
    Pokemon::StoryStep step;
    std::string failure;
};

std::vector<StepCase> step_cases(){
    const auto none = [](StubGame&){};
    const Pokemon::StoryStep mart = mart_step("mart", "", {.map = "PewterCity_Mart", .lines = {{"potion", 5}}});
    const Pokemon::StoryStep grind = grind_step("grind", "", {.member = 0, .level = 20, .max_encounters = 5, .pace = Pokemon::Direction::EAST, .heal_every = 0}, {});
    return {
        {"level reached", none, level_step("check", "", {.slot = 0, .level = 16}), "unchanged"},
        {"level below", none, level_step("check", "", {.slot = 0, .level = 17}), "level_below"},
        {"level unread", [](StubGame& game){ game.level.reset(); }, level_step("check", "", {.slot = 0, .level = 16}), "level_unread"},
        {"a Mart purchase that fails", none, mart, "mart_failed"},
        {"a move it knows already succeeds", none, teach_step("teach", "", {.item = "hm03", .move = "surf", .member = 0}), "unchanged"},
        {"a TM not in the TM Case is skipped", none, teach_step("teach", "", {.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "water-gun"}), "unchanged"},
        {"a level that can't be read stops a GRIND", [](StubGame& game){ game.level.reset(); }, grind, "grind_unread"},
        {"a GRIND without a nurse", none, game_step(GRIND_ACTION, {{"slot", int64_t(1)}, {"level", int64_t(20)}}), "bad_args"},
        {"no YES/NO at the fossil", none, fossil_step("fossil", "", Fossil::HELIX), "fossil_failed"},
        {"bad args", none, game_step(MART_ACTION, {{"map", std::string("PewterCity_Mart")}}), "bad_args"},
        {"another game step", none, capture_step("capture", "", {.species = "rattata"}), "none"},
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


//  TEACH and MART steps are preparation, once per save: a retry from the
//  same save skips one that changed nothing. A LEVEL check, which follows
//  a GRIND, and the GRIND run every attempt.
UnitTestResult test_once_per_save_steps(){
    const std::vector<Pokemon::StoryStep> once{
        teach_step("teach", "", {.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "tail-whip"}),
        mart_step("mart", "", {.map = "CeruleanCity_Mart", .lines = {{.item = "potion", .quantity = 3}}}),
    };
    for (const Pokemon::StoryStep& step : once){
        if (!step.once_per_save){
            return step.name + ": expected once per save.";
        }
    }
    const Pokemon::StoryStep grind = grind_step("grind", "", {.member = 0, .level = 20, .max_encounters = 5, .pace = Pokemon::Direction::EAST, .heal_every = 0}, {});
    if (grind.once_per_save || level_step("level", "", {.slot = 0, .level = 16}).once_per_save){
        return "GRIND and LEVEL: expected every attempt.";
    }
    return true;
}


//  A party whose screens count the summaries read.
class CountingScreens : public PartyScreens{
public:
    virtual bool open_party(size_t& members) override{ members = 2; m_opens++; return true; }
    virtual bool enter_summary() override{ m_summaries++; return true; }
    virtual std::string read_species() override{ m_summaries++; return "squirtle"; }
    virtual bool next_member() override{ return true; }
    virtual bool leave_summary() override{ return true; }
    virtual bool open_member_menu(size_t slot) override{ m_menu = slot; return true; }
    virtual std::optional<PartyMemberMenu> read_member_menu() override{
        return PartyMemberMenu{.field_moves = {"surf"}};
    }
    virtual bool close_member_menu() override{ return true; }
    virtual void close() override{}

    size_t m_opens = 0;
    size_t m_summaries = 0;
    size_t m_menu = SIZE_MAX;
};

//  A TEACH of a field move records its user; the Fly after it opens the
//  party once, at that slot, and reads no summary, though the species
//  cache was emptied by a reload. A TEACH of a plain TM records nothing.
UnitTestResult test_teach_records_fly_user(){
    StubGame game;
    QuietLogger logger;
    const MemberReader members = [](size_t slot) -> std::optional<MemberInfo>{
        if (slot != 0){
            return std::nullopt;
        }
        return MemberInfo{.species = "squirtle", .level = 16, .moves = {"water-gun", "surf"}};
    };
    PartyCache party;
    StoryAbilities abilities{members, game.mart, game.teach, game.patch, game.fossils, {}, &party};
    run_ability_game_step(logger, nullptr, teach_step("teach_surf", "", {.item = "hm03", .move = "surf", .member = 0}), abilities);
    run_ability_game_step(logger, nullptr, teach_step("teach_tm", "", {.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "water-gun"}), abilities);
    const std::optional<FieldMoveUser> user = party.users().find("surf");
    if (!user || user->slot != 0 || user->species != "squirtle"){
        return "the Surf user isn't recorded as slot 1, squirtle.";
    }
    if (party.users().find("water-pulse")){
        return "a TM move was recorded as a field move.";
    }
    party.invalidate_species();
    CountingScreens screens;
    size_t flights = 0;
    const std::string failure = use_field_move(logger, screens, party, "surf", "squirtle", [&flights](size_t){
        flights++;
        return std::string();
    });
    if (!failure.empty() || flights != 1 || screens.m_menu != 0 || screens.m_opens != 1 || screens.m_summaries != 0){
        return std::format(
            "Fly after TEACH: failure '{}', {} flights, menu {}, {} party opens, {} summary reads.",
            failure, flights, screens.m_menu, screens.m_opens, screens.m_summaries
        );
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


UnitTestResult test_level_args(){
    const std::optional<LevelCheck> back = level_check_of(level_args({.slot = 1, .level = 23}));
    if (!back || back->slot != 1 || back->level != 23){
        return "Expected slot 2, level 23 to read back.";
    }
    const std::vector<Pokemon::StoryActionArgs> bad_args{
        {},
        {{"slot", int64_t(1)}},
        {{"min", int64_t(5)}},
        {{"slot", int64_t(0)}, {"min", int64_t(5)}},
        {{"slot", int64_t(1)}, {"min", int64_t(101)}},
    };
    for (const Pokemon::StoryActionArgs& bad : bad_args){
        if (level_check_of(bad)){
            return "Expected args with a missing or out-of-range value refused.";
        }
    }
    return true;
}

} // namespace


const AutoStoryAbilitiesCase AUTO_STORY_ABILITIES_CASES[] = {
    {
        .input = {"the steps' ends as outcomes", test_ability_steps},
        .expectation = {},
    },
    {
        .input = {"a shiny in a GRIND stops the program", test_grind_shiny_stops},
        .expectation = {},
    },
    {
        .input = {"a Fly after a TEACH reads no summary", test_teach_records_fly_user},
        .expectation = {},
    },
    {
        .input = {"the LEVEL step's args", test_level_args},
        .expectation = {},
    },
    {
        .input = {"TEACH and MART run once per save", test_once_per_save_steps},
        .expectation = {},
    },
};

void add_tests_AutoStoryAbilities(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_ABILITIES_CASES){
        database.add<Test_AutoStoryAbilities>(item);
    }
    add_tests_AutoStoryMart(database);
    add_tests_AutoStoryTeach(database);
    add_tests_AutoStoryGrind(database);
    add_tests_AutoStoryChoice(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

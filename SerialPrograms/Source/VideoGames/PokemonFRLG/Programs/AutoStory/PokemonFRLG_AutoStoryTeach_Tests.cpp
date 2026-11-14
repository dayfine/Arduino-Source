/*  AutoStory Teach Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The TEACH step against a fake party and TM Case that answer as
 *  pokefirered's tm_case.c and party_menu.c do (see
 *  PokemonFRLG_AutoStoryTeach.h): a member with room learns, one with four
 *  moves asks to forget one, an HM move can't be forgotten, a TM is used up
 *  and an HM is not. The party is console B's (2026-11-02): Squirtle Lv16
 *  with WATER GUN, TAIL WHIP, BUBBLE, WITHDRAW.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryTeachScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

struct FakeMon{
    std::string species;
    std::vector<std::string> moves;
    std::set<std::string> compatible;
};

struct FakeDisc{
    std::string move;
    size_t count = 1;
};

class FakeTeachGame : public TeachActions{
public:
    std::vector<FakeMon> party;
    std::map<std::string, FakeDisc> tm_case = {};
    bool moves_readable = true;
    //  What the reader says before teaching, if not the party's moves.
    std::optional<std::vector<std::string>> stale_moves = std::nullopt;
    //  The moves page forgets nothing: a bug the confirmation must catch.
    bool forget_ignored = false;
    //  The bag or the TM Case didn't open.
    bool open_fails = false;
    size_t items_opened = 0;
    size_t stopped = 0;
    std::string error = {};

    std::optional<MemberInfo> read_member(size_t slot){
        if (m_screen != Screen::OVERWORLD){
            error = "read the summary off the overworld";
        }
        if (slot >= party.size() || !moves_readable){
            return std::nullopt;
        }
        MemberInfo info{.species = party[slot].species, .level = 16, .moves = party[slot].moves};
        if (stale_moves && items_opened == 0){
            info.moves = *stale_moves;
        }
        return info;
    }
    virtual ItemOpen open_item(std::string_view item, std::string_view) override{
        items_opened++;
        if (open_fails){
            m_screen = Screen::BAG;
            return ItemOpen::FAILED;
        }
        const auto found = tm_case.find(std::string(item));
        if (found == tm_case.end() || found->second.count == 0){
            m_screen = Screen::BAG;
            return ItemOpen::NOT_IN_CASE;
        }
        m_item = found->first;
        m_screen = Screen::CHOOSE_MEMBER;
        return ItemOpen::OPENED;
    }
    virtual TeachPrompt choose_member(size_t slot) override{
        if (m_screen != Screen::CHOOSE_MEMBER || slot >= party.size()){
            error = "chose a member off the party menu";
            return TeachPrompt::FAILED;
        }
        m_slot = slot;
        FakeMon& mon = party[slot];
        const std::string& move = tm_case[m_item].move;
        if (std::find(mon.moves.begin(), mon.moves.end(), move) != mon.moves.end()){
            return TeachPrompt::ALREADY_KNOWS;
        }
        if (!mon.compatible.contains(move)){
            return TeachPrompt::NOT_COMPATIBLE;
        }
        if (mon.moves.size() < MOVE_SLOTS){
            mon.moves.push_back(move);
            used_up();
            return TeachPrompt::LEARNED;
        }
        m_screen = Screen::DELETE_PROMPT;
        return TeachPrompt::DELETE_PROMPT;
    }
    virtual ForgetEnd forget(size_t slot) override{
        if (m_screen != Screen::DELETE_PROMPT){
            error = "forgot a move off the prompt";
            return ForgetEnd::FAILED;
        }
        std::vector<std::string>& moves = party[m_slot].moves;
        if (is_hm_move(moves[slot])){
            return ForgetEnd::HM_REFUSED;
        }
        if (!forget_ignored){
            moves[slot] = tm_case[m_item].move;
        }
        used_up();
        m_screen = Screen::CHOOSE_MEMBER;
        return ForgetEnd::LEARNED;
    }
    virtual void stop_teaching() override{
        stopped++;
        m_screen = Screen::BAG;
    }
    virtual bool leave() override{
        m_screen = Screen::OVERWORLD;
        return true;
    }
    virtual void dump_screen(std::string_view) override{}

private:
    enum class Screen{
        OVERWORLD,
        BAG,
        CHOOSE_MEMBER,
        DELETE_PROMPT,
    };

    //  A TM is used up; an HM is not.
    void used_up(){
        if (m_item.starts_with("tm")){
            tm_case[m_item].count--;
        }
    }

    Screen m_screen = Screen::OVERWORLD;
    std::string m_item;
    size_t m_slot = 0;
};


MemberReader reader_of(FakeTeachGame& game){
    return [&game](size_t slot){
        return game.read_member(slot);
    };
}


struct AutoStoryTeachInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryTeachExpectation{
};

struct AutoStoryTeachCase{
    AutoStoryTeachInput input;
    AutoStoryTeachExpectation expectation;
};

class Test_AutoStoryTeach : public UnitTest{
public:
    explicit Test_AutoStoryTeach(AutoStoryTeachCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryTeach - " + test_case.input.name)
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
    AutoStoryTeachCase m_case;
};


FakeTeachGame console_b_party(){
    FakeTeachGame game;
    game.party = {
        {"squirtle", {"water-gun", "tail-whip", "bubble", "withdraw"}, {"surf", "strength", "dig", "rock-smash"}},
        {"rattata", {"tackle", "tail-whip", "quick-attack"}, {"dig", "cut"}},
        {"oddish", {"absorb", "cut", "poison-powder", "acid"}, {"cut", "flash"}},
    };
    game.tm_case = {{"hm03", {"surf", 1}}, {"tm28", {"dig", 1}}, {"hm01", {"cut", 1}}};
    return game;
}

struct TeachCase{
    std::string name = {};
    TeachPlan plan = {};
    std::function<void(FakeTeachGame&)> setup = {};
    TeachEnd end = TeachEnd::LEARNED;
    //  The member's moves after the step.
    std::vector<std::string> moves = {};
    //  Whether the bag was opened at all.
    bool opened = true;
};

std::vector<TeachCase> teach_cases_1(){
    return {
        {
            .name = "Surf over TAIL WHIP by name",
            .plan = {.item = "hm03", .move = "surf", .member = 0, .forget_move = "tail-whip"},
            .moves = {"water-gun", "surf", "bubble", "withdraw"},
        },
        {
            .name = "Strength over the fourth slot by number",
            .plan = {.item = "hm04", .move = "strength", .member = 0, .forget_slot = 3},
            .setup = [](FakeTeachGame& game){ game.tm_case["hm04"] = {"strength", 1}; },
            .moves = {"water-gun", "tail-whip", "bubble", "strength"},
        },
        {
            .name = "a TM into a free slot",
            .plan = {.item = "tm28", .move = "dig", .member = 1},
            .moves = {"tackle", "tail-whip", "quick-attack", "dig"},
        },
        {
            .name = "a move it knows already: nothing pressed",
            .plan = {.item = "hm01", .move = "cut", .member = 2},
            .end = TeachEnd::ALREADY_KNOWN,
            .moves = {"absorb", "cut", "poison-powder", "acid"},
            .opened = false,
        },
    };
}

std::vector<TeachCase> teach_cases_2(){
    return {
        {
            .name = "four moves and none named to forget",
            .plan = {.item = "hm03", .move = "surf", .member = 0},
            .end = TeachEnd::REFUSED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
            .opened = false,
        },
        {
            .name = "a move to forget it doesn't know (the console Squirtle has no Tackle)",
            .plan = {.item = "hm03", .move = "surf", .member = 0, .forget_move = "tackle"},
            .end = TeachEnd::REFUSED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
            .opened = false,
        },
        {
            .name = "an HM move named to forget",
            .plan = {.item = "tm28", .move = "dig", .member = 2, .forget_move = "cut"},
            .end = TeachEnd::REFUSED,
            .moves = {"absorb", "cut", "poison-powder", "acid"},
            .opened = false,
        },
        {
            .name = "not compatible: given up, back on the overworld",
            .plan = {.item = "hm01", .move = "cut", .member = 0, .forget_move = "withdraw"},
            .end = TeachEnd::REFUSED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
        },
    };
}

std::vector<TeachCase> teach_cases_3(){
    return {
        {
            .name = "moves read with room, but the game asks to forget one",
            .plan = {.item = "hm03", .move = "surf", .member = 0},
            .setup = [](FakeTeachGame& game){ game.stale_moves = std::vector<std::string>{"water-gun", "bubble"}; },
            .end = TeachEnd::FAILED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
        },
        {
            .name = "moves that can't be read: nothing pressed",
            .plan = {.item = "hm03", .move = "surf", .member = 0, .forget_move = "tail-whip"},
            .setup = [](FakeTeachGame& game){ game.moves_readable = false; },
            .end = TeachEnd::UNREAD,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
            .opened = false,
        },
        {
            .name = "an item the TM Case hasn't got: skipped",
            .plan = {.item = "hm05", .move = "flash", .member = 2, .forget_move = "acid"},
            .end = TeachEnd::NO_ITEM,
            .moves = {"absorb", "cut", "poison-powder", "acid"},
        },
        {
            .name = "the TM Case didn't open",
            .plan = {.item = "hm03", .move = "surf", .member = 0, .forget_move = "tail-whip"},
            .setup = [](FakeTeachGame& game){ game.open_fails = true; },
            .end = TeachEnd::FAILED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
        },
        {
            .name = "the move didn't land: the read after catches it",
            .plan = {.item = "hm03", .move = "surf", .member = 0, .forget_move = "tail-whip"},
            .setup = [](FakeTeachGame& game){ game.forget_ignored = true; },
            .end = TeachEnd::FAILED,
            .moves = {"water-gun", "tail-whip", "bubble", "withdraw"},
        },
    };
}

std::vector<TeachCase> teach_cases(){
    std::vector<TeachCase> cases;
    for (std::vector<TeachCase> part : {teach_cases_1(), teach_cases_2(), teach_cases_3()}){
        cases.insert(cases.end(), part.begin(), part.end());
    }
    return cases;
}

UnitTestResult check_teach_case(const TeachCase& item){
    QuietLogger logger;
    FakeTeachGame game = console_b_party();
    if (item.setup){
        item.setup(game);
    }
    const TeachResult result = run_teach(logger, game, reader_of(game), item.plan);
    if (!game.error.empty()){
        return item.name + ": " + game.error;
    }
    if (result.end != item.end){
        return std::format("{}: ended {}, expected {} ({}).", item.name,
            teach_end_name(result.end), teach_end_name(item.end), result.detail);
    }
    if (game.party[item.plan.member].moves != item.moves){
        return item.name + ": the moves after aren't the ones expected.";
    }
    if ((game.items_opened > 0) != item.opened){
        return item.name + (item.opened ? ": the bag wasn't opened." : ": the bag was opened.");
    }
    return true;
}

UnitTestResult test_teach(){
    for (const TeachCase& item : teach_cases()){
        const UnitTestResult result = check_teach_case(item);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}


//  A TM is used up, an HM is not.
UnitTestResult test_tm_used_up(){
    QuietLogger logger;
    FakeTeachGame game = console_b_party();
    run_teach(logger, game, reader_of(game), {.item = "tm28", .move = "dig", .member = 1});
    run_teach(logger, game, reader_of(game), {.item = "hm03", .move = "surf", .member = 0, .forget_move = "withdraw"});
    if (game.tm_case["tm28"].count != 0 || game.tm_case["hm03"].count != 1){
        return "Expected TM28 used up and HM03 kept.";
    }
    return true;
}


UnitTestResult test_teach_args(){
    const TeachPlan plan{.item = "hm03", .move = "surf", .member = 0, .forget_move = "tail-whip"};
    const Pokemon::StoryActionArgs args = teach_args(plan);
    const std::optional<TeachPlan> back = teach_plan_of(args);
    if (!back || back->item != "hm03" || back->move != "surf" || back->member != 0 || back->forget_move != "tail-whip"){
        return "The Surf plan didn't read back from its args.";
    }
    const std::optional<TeachPlan> by_slot = teach_plan_of({
        {"item", std::string("tm28")}, {"move", std::string("dig")}, {"slot", int64_t(2)}, {"forget_slot", int64_t(4)},
    });
    if (!by_slot || by_slot->member != 1 || by_slot->forget_slot != 3u){
        return "Dig into slot 2, forgetting move 4, didn't read back.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"item", std::string("potion")},
        {"slot", int64_t(7)},
        {"forget_slot", int64_t(5)},
        {"move", std::string("")},
    };
    for (const auto& [key, value] : bad_args){
        Pokemon::StoryActionArgs bad = args;
        bad[key] = value;
        if (teach_plan_of(bad)){
            return "Expected a bad " + key + " refused.";
        }
    }
    return true;
}

} // namespace


const AutoStoryTeachCase AUTO_STORY_TEACH_CASES[] = {
    {
        .input = {"TMs and HMs on a fake party", test_teach},
        .expectation = {},
    },
    {
        .input = {"a TM is used up, an HM kept", test_tm_used_up},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_teach_args},
        .expectation = {},
    },
};

void add_tests_AutoStoryTeach(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_TEACH_CASES){
        database.add<Test_AutoStoryTeach>(item);
    }
    add_tests_AutoStoryTeachScreens(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

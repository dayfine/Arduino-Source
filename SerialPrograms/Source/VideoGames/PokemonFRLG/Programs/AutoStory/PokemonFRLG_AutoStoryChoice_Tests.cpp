/*  AutoStory Choice Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The fossil CHOICE against a fake MtMoon_B2F that runs its scripts.inc:
 *  YES on a fossil takes it and Miguel the other; NO leaves both.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryChoice.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

class FakeFossilRoom : public ChoiceActions{
public:
    explicit FakeFossilRoom(Fossil facing)
        : facing(facing)
    {}

    Fossil facing;
    bool offer_readable = true;
    bool bag_readable = true;
    std::optional<Fossil> taken = std::nullopt;
    size_t fossils_left = 2;
    std::string error = {};

    virtual bool talk() override{
        if (fossils_left < 2){
            return false;
        }
        m_prompt = true;
        return true;
    }
    virtual std::optional<Fossil> read_offer() override{
        if (!m_prompt){
            error = "read the offer with no prompt";
        }
        return offer_readable ? std::optional<Fossil>(facing) : std::nullopt;
    }
    virtual void answer(bool yes) override{
        if (!m_prompt){
            error = "answered with no prompt";
        }
        m_prompt = false;
        if (yes){
            //  The player takes this one, Miguel the other.
            taken = facing;
            fossils_left = 0;
        }
    }
    virtual bool finish() override{
        if (m_prompt){
            error = "left the prompt unanswered";
        }
        return true;
    }
    virtual std::optional<Fossil> fossil_in_bag() override{
        return bag_readable ? taken : std::nullopt;
    }

private:
    bool m_prompt = false;
};


struct AutoStoryChoiceInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryChoiceExpectation{
};

struct AutoStoryChoiceCase{
    AutoStoryChoiceInput input;
    AutoStoryChoiceExpectation expectation;
};

class Test_AutoStoryChoice : public UnitTest{
public:
    explicit Test_AutoStoryChoice(AutoStoryChoiceCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryChoice - " + test_case.input.name)
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
    AutoStoryChoiceCase m_case;
};


struct ChoiceCase{
    std::string name = {};
    Fossil wanted = Fossil::HELIX;
    Fossil facing = Fossil::HELIX;
    std::function<void(FakeFossilRoom&)> setup = {};
    ChoiceEnd end = ChoiceEnd::TAKEN;
    std::optional<Fossil> taken = std::nullopt;
    bool confirmed = false;
};

std::vector<ChoiceCase> choice_cases(){
    return {
        {.name = "HELIX, the default", .taken = Fossil::HELIX, .confirmed = true},
        {.name = "DOME", .wanted = Fossil::DOME, .facing = Fossil::DOME, .taken = Fossil::DOME, .confirmed = true},
        {
            .name = "facing the other fossil: NO, both left",
            .facing = Fossil::DOME,
            .end = ChoiceEnd::WRONG_ONE,
        },
        //  The console has no reader for the prompt's text: the table has
        //  walked the player to face the wanted fossil, so YES takes it
        //  (console B: answering NO and stopping left 00.13 stuck there).
        {
            .name = "an unread prompt: YES, facing the wanted fossil",
            .setup = [](FakeFossilRoom& room){ room.offer_readable = false; },
            .taken = Fossil::HELIX,
            .confirmed = true,
        },
        {
            .name = "an unread prompt and an unread bag: YES",
            .setup = [](FakeFossilRoom& room){ room.offer_readable = false; room.bag_readable = false; },
            .taken = Fossil::HELIX,
        },
        {
            .name = "taken, the bag unread",
            .setup = [](FakeFossilRoom& room){ room.bag_readable = false; },
            .taken = Fossil::HELIX,
        },
        {
            .name = "both already gone",
            .setup = [](FakeFossilRoom& room){ room.fossils_left = 0; },
            .end = ChoiceEnd::FAILED,
        },
    };
}

UnitTestResult test_fossil_choice(){
    for (const ChoiceCase& item : choice_cases()){
        QuietLogger logger;
        FakeFossilRoom room(item.facing);
        if (item.setup){
            item.setup(room);
        }
        const ChoiceResult result = run_fossil_choice(logger, room, item.wanted);
        if (!room.error.empty()){
            return item.name + ": " + room.error;
        }
        if (result.end != item.end || room.taken != item.taken || result.confirmed != item.confirmed){
            return std::format("{}: ended {} ({}), expected {}.", item.name,
                choice_end_name(result.end), result.detail, choice_end_name(item.end));
        }
    }
    return true;
}


UnitTestResult test_fossil_args(){
    const Pokemon::StoryStep step = fossil_step("helix", "", Fossil::HELIX);
    if (step.action != FOSSIL_ACTION || fossil_of(step.args) != Fossil::HELIX
        || fossil_of({{"fossil", std::string("dome")}}) != Fossil::DOME
    ){
        return "Expected the HELIX step and \"dome\" to read back.";
    }
    for (const std::string& bad : {"", "old_amber", "helix dome"}){
        if (fossil_of({{"fossil", bad}})){
            return "Expected \"" + bad + "\" refused.";
        }
    }
    if (fossil_of({})){
        return "Expected a step with no fossil refused.";
    }
    //  MtMoon_B2F map.json: DOME at (13, 7), HELIX at (14, 7).
    if (!(fossil_tile(Fossil::DOME) == Pokemon::TilePos{13, 7}) || !(fossil_tile(Fossil::HELIX) == Pokemon::TilePos{14, 7})){
        return "The fossils' tiles aren't MtMoon_B2F's.";
    }
    return true;
}

} // namespace


const AutoStoryChoiceCase AUTO_STORY_CHOICE_CASES[] = {
    {
        .input = {"the fossil on a fake Mt. Moon", test_fossil_choice},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_fossil_args},
        .expectation = {},
    },
};

void add_tests_AutoStoryChoice(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CHOICE_CASES){
        database.add<Test_AutoStoryChoice>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

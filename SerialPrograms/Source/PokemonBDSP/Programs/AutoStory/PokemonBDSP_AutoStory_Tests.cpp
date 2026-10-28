/*  BDSP AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Utils/PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

class Test_IntroDecisions : public UnitTest{
public:
    Test_IntroDecisions()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro decisions")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            const char* screen;
            IntroProgress progress;
            IntroStep expected;
        };
        const IntroProgress start{false, false};
        const IntroProgress named{true, false};
        const IntroProgress done{true, true};
        const Case cases[] = {
            {"user-select",         start,  IntroStep::SELECT_USER},
            {"home",                done,   IntroStep::FAIL_NOT_IN_GAME},
            {"keyboard",            start,  IntroStep::ENTER_PLAYER_NAME},
            {"keyboard",            named,  IntroStep::CANCEL_RIVAL_KEYBOARD},
            {"rival-list-tell",     named,  IntroStep::MOVE_TO_PRESET},
            {"rival-list-preset",   named,  IntroStep::PICK_PRESET},
            {"menu",                done,   IntroStep::CLOSE_MENU_AND_FINISH},
            //  The appearance grid has its prompt in a dialogue box.
            {"dialog",              start,  IntroStep::ADVANCE},
            {"dialog",              done,   IntroStep::ADVANCE},
            //  Language and the appearance preview: no X before the bedroom is possible.
            {"other",               start,  IntroStep::ADVANCE},
            {"other",               named,  IntroStep::ADVANCE},
            {"other",               done,   IntroStep::PROBE_FOR_MENU},
        };
        for (const Case& c : cases){
            const IntroStep actual = decide_intro_step(c.screen, c.progress);
            if (actual != c.expected){
                return std::string("Screen \"") + c.screen + "\" (player named " +
                    (c.progress.player_named ? "yes" : "no") + ", rival named " +
                    (c.progress.rival_named ? "yes" : "no") + "): step " +
                    std::to_string((int)actual) + ", expected " + std::to_string((int)c.expected) + ".";
            }
        }
        if (advance_after_probe(true)){
            return "A after X changed the screen would pick from whatever X opened.";
        }
        if (!advance_after_probe(false)){
            return "A must advance a screen X left unchanged.";
        }
        return true;
    }
};

}


void add_tests_AutoStory(UnitTestDatabase& database){
    database.add<Test_IntroDecisions>();
}


}
}
}

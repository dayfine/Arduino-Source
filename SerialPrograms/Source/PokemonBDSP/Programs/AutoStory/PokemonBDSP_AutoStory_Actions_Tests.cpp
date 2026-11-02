/*  BDSP AutoStory - Actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's intro decisions, and the screenshot checks behind the
 *  Pokemon Center, the save card and the Jubilife School's lamp. Screenshots
 *  are in the unit test resources (the CommandLineTests checkout); a test
 *  whose screenshot is missing is skipped.
 *
 */

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Actions_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{


//  How far the intro has got, with BDSP confirmed: 1, the player named;
//  2, the rival too.
IntroProgress intro_progress(size_t names_given){
    IntroProgress progress;
    progress.player_named = names_given >= 1;
    progress.rival_named = names_given >= 2;
    progress.game_confirmed = true;
    return progress;
}

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
        const IntroProgress start = intro_progress(0);
        const IntroProgress named = intro_progress(1);
        const IntroProgress done = intro_progress(2);
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
            {"dialog",              done,   IntroStep::ADVANCE_WITH_B},
            //  Logos, the title, language and the appearance preview: no X
            //  before the bedroom is possible.
            {"other",               start,  IntroStep::ADVANCE},
            {"other",               named,  IntroStep::ADVANCE},
            {"other",               done,   IntroStep::CLEAR_WITH_B},
            {"black",               start,  IntroStep::WAIT},
            {"black",               done,   IntroStep::WAIT},
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
        return true;
    }
};


//  Runs `screens` through decide_intro_step() and record_intro_step() from
//  `progress`; the steps taken.
std::vector<IntroStep> intro_steps(IntroProgress& progress, const std::vector<std::string>& screens){
    std::vector<IntroStep> steps;
    for (const std::string& screen : screens){
        steps.push_back(decide_intro_step(screen, progress));
        record_intro_step(progress, steps.back());
    }
    return steps;
}

//  The rival's preset name is confirmed with A on "Barry, is it?" and its
//  Yes/No; B there answers No and the list comes back (r4). Only
//  INTRO_SCREENS_AFTER_RIVAL_PICK screens after the pick does B take over.
class Test_IntroRivalConfirm : public UnitTest{
public:
    Test_IntroRivalConfirm()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro: the rival's name confirmed with A")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        IntroProgress progress = intro_progress(1);
        //  r4's frames 029 to 032: the pick, "Barry, is it?", Barry alone
        //  as the Yes/No comes up, the Yes/No.
        const std::vector<IntroStep> confirm = intro_steps(progress, {"rival-list-preset", "dialog", "other", "dialog"});
        const std::vector<IntroStep> expected{IntroStep::PICK_PRESET, IntroStep::ADVANCE, IntroStep::ADVANCE, IntroStep::ADVANCE};
        if (confirm != expected){
            return "The pick and its confirmation must take A, never B.";
        }
        if (progress.rival_named){
            return "The name is not final until INTRO_SCREENS_AFTER_RIVAL_PICK screens after the pick.";
        }
        //  The list back (No was answered): pick again, the count restarts.
        if (intro_steps(progress, {"rival-list-tell", "rival-list-preset"}) !=
            std::vector<IntroStep>{IntroStep::MOVE_TO_PRESET, IntroStep::PICK_PRESET}){
            return "The list coming back after a pick must be picked from again.";
        }
        for (size_t c = 0; c < INTRO_SCREENS_AFTER_RIVAL_PICK; c++){
            if (intro_steps(progress, {c % 2 ? "other" : "dialog"}) != std::vector<IntroStep>{IntroStep::ADVANCE}){
                return "Screen " + std::to_string(c) + " after the pick must take A.";
            }
        }
        if (!progress.rival_named){
            return "INTRO_SCREENS_AFTER_RIVAL_PICK screens after the pick, the name is final.";
        }
        return true;
    }
};


//  After the rival is named, unknown screens take INTRO_B_BEFORE_X B presses,
//  then X; only the menu ends the intro (r2: Rowan's last line, misread).
class Test_IntroBeforeMenu : public UnitTest{
public:
    Test_IntroBeforeMenu()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro: B on unknown screens, then X for the menu")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        IntroProgress progress = intro_progress(1);
        std::vector<std::string> screens{"rival-list-preset", "dialog", "other"};
        std::vector<IntroStep> expected{IntroStep::PICK_PRESET, IntroStep::ADVANCE, IntroStep::ADVANCE};
        for (size_t c = 2; c < INTRO_SCREENS_AFTER_RIVAL_PICK; c++){
            screens.push_back("dialog");
            expected.push_back(IntroStep::ADVANCE);
        }
        for (size_t round = 0; round < 2; round++){
            screens.insert(screens.end(), INTRO_B_BEFORE_X + 1, "other");
            expected.insert(expected.end(), INTRO_B_BEFORE_X, IntroStep::CLEAR_WITH_B);
            expected.push_back(IntroStep::PROBE_FOR_MENU);
        }
        //  A dialogue line in between takes B without counting.
        screens.insert(screens.end(), {"dialog", "other", "menu"});
        expected.insert(expected.end(), {IntroStep::ADVANCE_WITH_B, IntroStep::CLEAR_WITH_B, IntroStep::CLOSE_MENU_AND_FINISH});
        const std::vector<IntroStep> steps = intro_steps(progress, screens);
        for (size_t c = 0; c < steps.size(); c++){
            if (steps[c] != expected[c]){
                return "Screen " + std::to_string(c) + " (\"" + screens[c] + "\"): step " +
                    std::to_string((int)steps[c]) + ", expected " + std::to_string((int)expected[c]) + ".";
            }
        }
        IntroProgress pressed = intro_progress(2);
        pressed.b_presses = INTRO_B_BEFORE_X;
        if (decide_intro_step("other", pressed) != IntroStep::PROBE_FOR_MENU){
            return "X must follow INTRO_B_BEFORE_X B presses.";
        }
        return true;
    }
};


//  X from a still bedroom opening something the detector misses, closed
//  again by B, is the player in control; moving screens or X doing nothing
//  are not.
class Test_IntroInControl : public UnitTest{
public:
    Test_IntroInControl()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro: in control without the menu read")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            XProbe probe;
            bool expected;
            const char* what;
        };
        const Case cases[] = {
            {{1, 60, 2}, true, "still, X opened, B closed"},
            {{12, 60, 2}, false, "moving before X (sparkles, a cutscene)"},
            {{1, 5, 2}, false, "X changed nothing"},
            {{1, 60, 40}, false, "B did not put it back"},
        };
        for (const Case& c : cases){
            if (intro_in_control_without_menu(c.probe) != c.expected){
                return std::string("Wrong for: ") + c.what + ".";
            }
        }
        return true;
    }
};


//  The intro gates its A presses on a BDSP screen within a time from the
//  first screen that is not black: past it, it is another game (r1,
//  2026-11-01, launched FireRed). The boot's black screens, about 25 s of
//  them (r3), neither take presses nor count.
class Test_IntroGate : public UnitTest{
public:
    Test_IntroGate()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro: BDSP or stop")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        using std::chrono::milliseconds;
        IntroProgress progress;
        const std::string boot = check_boot(progress);
        if (!boot.empty()){
            return boot;
        }
        const milliseconds shown = 2 * INTRO_TIME_BEFORE_BDSP + milliseconds(500);
        note_intro_screen(progress, "other", shown);
        if (decide_intro_step("other", progress) != IntroStep::ADVANCE){
            return "The first logo must take A.";
        }
        note_intro_screen(progress, "black", shown + INTRO_TIME_BEFORE_BDSP / 2);
        note_intro_screen(progress, "other", shown + INTRO_TIME_BEFORE_BDSP - milliseconds(1));
        if (decide_intro_step("other", progress) != IntroStep::ADVANCE){
            return "Unknown screens take A until INTRO_TIME_BEFORE_BDSP has passed.";
        }
        IntroProgress other_game = progress;
        note_intro_screen(other_game, "other", shown + INTRO_TIME_BEFORE_BDSP);
        if (decide_intro_step("other", other_game) != IntroStep::FAIL_NOT_BDSP){
            return "Past INTRO_TIME_BEFORE_BDSP of unknown screens, it is another game.";
        }
        note_intro_screen(progress, "dialog", shown + INTRO_TIME_BEFORE_BDSP - milliseconds(1));
        note_intro_screen(progress, "other", shown + 10 * INTRO_TIME_BEFORE_BDSP);
        if (decide_intro_step("other", progress) != IntroStep::ADVANCE){
            return "Once BDSP's dialogue has shown, the clock stops.";
        }
        return check_confirming_screens();
    }

private:
    //  Black from the restart until the first logo, well past the limit.
    static std::string check_boot(IntroProgress& progress){
        using std::chrono::milliseconds;
        for (milliseconds t(0); t <= 2 * INTRO_TIME_BEFORE_BDSP; t += milliseconds(500)){
            note_intro_screen(progress, "black", t);
            if (decide_intro_step("black", progress) != IntroStep::WAIT){
                return "A black screen must take no press.";
            }
        }
        if (progress.first_shown || progress.unconfirmed_for != milliseconds(0)){
            return "Black screens must not start the wrong-game clock.";
        }
        return "";
    }
    static UnitTestResult check_confirming_screens(){
        for (const char* screen : {"dialog", "rival-list-tell", "rival-list-preset", "menu"}){
            if (!intro_screen_confirms_bdsp(screen)){
                return std::string("Expected \"") + screen + "\" to confirm BDSP.";
            }
        }
        for (const char* screen : {"user-select", "home", "keyboard", "black", "other"}){
            if (intro_screen_confirms_bdsp(screen)){
                return std::string("Expected \"") + screen + "\" not to confirm BDSP.";
            }
        }
        return true;
    }
};

//  The Mart's doorway on the failure screenshots of r121a and r122a, kept in
//  the unit test resources (the CommandLineTests checkout): where it is
//  relative to the player, who is in the middle of the screen.
class Test_LitDoorwayScreenshots : public UnitTest{
public:
    Test_LitDoorwayScreenshots()
        : UnitTest("PokemonBDSP::AutoStory - lit doorway screenshots")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{
            const char* file;
            double tiles_across_min;
            double tiles_across_max;
            double rows_up_min;
            double rows_up_max;
        };
        const Case cases[] = {
            //  r121a: below-left of the door, which is part of a tile east, straight ahead.
            {"PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", 0.2, 0.7, 0.5, 1.5},
            //  r122a: a tile west of the door and three or four rows south of its front.
            {"PokemonBDSP/AutoStory/SandgemMartDoorFarLeft-20261030-024648.png", -1.3, -0.5, 2.5, 4.5},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const std::optional<LitDoorway> doorway = find_lit_doorway(screen);
            if (!doorway){
                return std::string(c.file) + ": no lit doorway found.";
            }
            logger.log(std::string(c.file) + ": doorway at " + std::to_string(doorway->x) + ", " +
                std::to_string(doorway->y) + ": " + std::to_string(doorway->tiles_across) + " tiles across, " +
                std::to_string(doorway->rows_up) + " rows up.");
            if (doorway->tiles_across < c.tiles_across_min || doorway->tiles_across > c.tiles_across_max){
                return std::string(c.file) + ": doorway " + std::to_string(doorway->tiles_across) +
                    " tiles across, expected " + std::to_string(c.tiles_across_min) + " to " +
                    std::to_string(c.tiles_across_max) + ".";
            }
            if (doorway->rows_up < c.rows_up_min || doorway->rows_up > c.rows_up_max){
                return std::string(c.file) + ": doorway " + std::to_string(doorway->rows_up) +
                    " rows up, expected " + std::to_string(c.rows_up_min) + " to " +
                    std::to_string(c.rows_up_max) + ".";
            }
        }
        return true;
    }
};


//  The save prompt r124a took beside Sandgem's Poke Mart, sure it stood on
//  Route 202 (in CommandLineTests under PokemonBDSP/AutoStory): its card
//  names Sandgem Town, which the save must read before confirming.
class Test_SaveScreenLocation : public UnitTest{
public:
    Test_SaveScreenLocation()
        : UnitTest("PokemonBDSP::AutoStory - save screen location")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string path = UNIT_TEST_RESOURCE_PATH() + "PokemonBDSP/AutoStory/SaveScreenSandgemTown-20261030-034444.png";
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
        }
        const std::string read = read_save_screen_location(ImageRGB32(path));
        logger.log("Save screen location read: \"" + read + "\".");
        if (!location_banner_matches(read, MAP_SANDGEM_TOWN)){
            return "Save screen location \"" + read + "\" should match Sandgem Town.";
        }
        if (location_banner_matches(read, MAP_ROUTE_202)){
            return "Save screen location \"" + read + "\" should not match Route 202.";
        }
        return true;
    }
};


//  The two shots around r126's door walk: below the Center's sign, outside,
//  and on the entrance mat inside, where the lit floor read as a doorway and
//  the walk kept lining up with it (runs 2 and 3).
class Test_PokemonCenterInside : public UnitTest{
public:
    Test_PokemonCenterInside()
        : UnitTest("PokemonBDSP::AutoStory - Pokemon Center inside")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{
            const char* file;
            bool inside;
            bool doorway;
        };
        const Case cases[] = {
            {.file = "PokemonBDSP/AutoStory/SandgemCenterInside-20261030-053555.png", .inside = true, .doorway = false},
            //  r127c: walked in by the turn north at the door, facing north.
            {.file = "PokemonBDSP/AutoStory/SandgemCenterInsideFacingNorth-r127c.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/SandgemCenterBelowSign-20261030-053527.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/SandgemMartDoorFarLeft-20261030-024648.png", .inside = false, .doorway = true},
            //  Jubilife (2026-10-30): inside its Center twice, at its door, and
            //  on the doorstep after walking out (the banner up).
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", .inside = false, .doorway = true},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const bool inside = pokemon_center_inside(screen);
            const std::optional<LitDoorway> doorway = find_lit_doorway(screen);
            logger.log(std::string(c.file) + ": inside " + std::to_string(inside) + ", doorway " +
                (doorway ? std::to_string(doorway->tiles_across) + " across, " + std::to_string(doorway->rows_up) + " rows up" : "none"));
            if (inside != c.inside){
                return std::string(c.file) + ": expected " + (c.inside ? "inside" : "outside") +
                    " a Pokemon Center, read " + (inside ? "inside" : "outside") + ".";
            }
            if (doorway.has_value() != c.doorway){
                return std::string(c.file) + ": expected " + (c.doorway ? "a doorway" : "no doorway") +
                    ", read " + (doorway ? "one" : "none") + ".";
            }
        }
        return true;
    }
};


//  The School's lamp at night, from its doorsteps' row (2026-10-30): where
//  the door took us in, the lamp reads within half a tile of the door; a
//  tile either side, more.
class Test_SchoolLamp : public UnitTest{
public:
    Test_SchoolLamp()
        : UnitTest("PokemonBDSP::AutoStory - Jubilife School lamp")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const ImageFloatBox box(0.55, 0.20, 0.30, 0.10);
        struct Case{ const char* file; double min; double max; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeSchoolRightOfDoor-20261030.png", -1.5, -0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolOneTileShort-20261030.png", -1.5, -0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor2-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolOneTilePast-20261030.png", 0.5, 1.5},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const std::optional<double> lamp = find_lit_lamp_x(screen, box);
            if (!lamp){
                return std::string(c.file) + ": no lit lamp found.";
            }
            const double across = tiles_across_at(*lamp) - tiles_across_at(0.67);
            logger.log(std::string(c.file) + ": lamp at " + std::to_string(*lamp) + ", door " + std::to_string(across) + " tiles across.");
            if (across < c.min || across > c.max){
                return std::string(c.file) + ": door " + std::to_string(across) + " tiles across, expected " +
                    std::to_string(c.min) + " to " + std::to_string(c.max) + ".";
            }
        }
        return true;
    }
};


}


void add_tests_AutoStory_Actions(UnitTestDatabase& database){
    database.add<Test_IntroDecisions>();
    database.add<Test_IntroGate>();
    database.add<Test_IntroRivalConfirm>();
    database.add<Test_IntroBeforeMenu>();
    database.add<Test_IntroInControl>();
    database.add<Test_LitDoorwayScreenshots>();
    database.add<Test_SaveScreenLocation>();
    database.add<Test_PokemonCenterInside>();
    database.add<Test_SchoolLamp>();
}



}
}
}

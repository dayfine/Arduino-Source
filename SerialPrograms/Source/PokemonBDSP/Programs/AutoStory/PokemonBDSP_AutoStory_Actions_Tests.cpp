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
    database.add<Test_LitDoorwayScreenshots>();
    database.add<Test_SaveScreenLocation>();
    database.add<Test_PokemonCenterInside>();
    database.add<Test_SchoolLamp>();
}



}
}
}

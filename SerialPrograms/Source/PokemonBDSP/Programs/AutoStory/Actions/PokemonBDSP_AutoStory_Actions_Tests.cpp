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

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Logging/Logger.h"
#include "NintendoSwitch/Programs/MenuCursor/NintendoSwitch_MenuCursor_Tests.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Options/PokemonBDSP_PlayerModelOption.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_IntroMenuDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_BattleMoves.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_IntroProbe.h"
#include "PokemonBDSP_AutoStory_IntroSteps.h"
#include "PokemonBDSP_AutoStory_LitDoorway.h"
#include "PokemonBDSP_AutoStory_NewGame.h"
#include "PokemonBDSP_AutoStory_Settings.h"
#include "PokemonBDSP_AutoStory_Actions_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace detail;

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

UnitTestResult test_intro_decisions(Logger&){
    struct Case{
        std::string screen;
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
UnitTestResult test_intro_rival_confirm(Logger&){
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


//  After the rival is named, unknown screens take INTRO_B_BEFORE_X B presses,
//  then X; only the menu ends the intro (r2: Rowan's last line, misread).
UnitTestResult test_intro_before_menu(Logger&){
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


//  X from a still bedroom opening something the detector misses, closed
//  again by B, is the player in control; moving screens or X doing nothing
//  are not.
UnitTestResult test_intro_in_control(Logger&){
    struct Case{
        XProbe probe;
        bool expected;
        std::string what;
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


//  Black from the restart until the first logo, well past the limit.
std::string check_boot(IntroProgress& progress){
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

UnitTestResult check_confirming_screens(){
    for (const std::string screen : {"dialog", "language-select", "rival-list-tell", "rival-list-preset", "menu"}){
        if (!intro_screen_confirms_bdsp(screen)){
            return std::string("Expected \"") + screen + "\" to confirm BDSP.";
        }
    }
    for (const std::string screen : {"user-select", "home", "keyboard", "black", "other"}){
        if (intro_screen_confirms_bdsp(screen)){
            return std::string("Expected \"") + screen + "\" not to confirm BDSP.";
        }
    }
    return true;
}

//  The intro gates its A presses on a BDSP screen within a time from the
//  first screen that is not black: past it, it is another game (r1,
//  2026-11-01, launched FireRed). The boot's black screens, about 25 s of
//  them (r3), neither take presses nor count.
UnitTestResult test_intro_gate(Logger&){
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


//  The menus' data, grids, detectors and the look option agree on their sizes.
UnitTestResult check_intro_menu_sizes(){
    if (bdsp_language_menu().size() != BDSP_LANGUAGE_GRID.count || BDSP_LANGUAGE_GRID.count != LANGUAGE_SELECT_BUTTONS){
        return "The language select's data, grid and detector must agree on ten buttons.";
    }
    if (BDSP_LOOK_GRID.count != LOOK_SELECT_PORTRAITS || BDSP_LOOK_GRID.count != BDSP_PLAYER_MODEL_COUNT){
        return "The look grid, its detector and PlayerModelOption must agree on eight looks.";
    }
    return true;
}

//  The presses for the language select (two columns of five, read row by
//  row, from wherever its cursor is) and the appearance grid (two rows of
//  four, from the first look).
UnitTestResult test_intro_pick_moves(Logger&){
    using Moves = std::optional<std::vector<DpadPosition>>;
    const IntroPicks french{Language::French, 2};
    struct Case{
        std::string_view what;
        Moves actual;
        Moves expected;
    };
    const Case cases[] = {
        {"French from English, where the cursor starts (lab run 3)",
            language_pick_moves(0, french), Moves(std::vector<DpadPosition>{DPAD_DOWN, DPAD_DOWN})},
        {"English from English",
            language_pick_moves(0, IntroPicks{Language::English, 0}), Moves(std::vector<DpadPosition>{})},
        {"French from Traditional Chinese, bottom right",
            language_pick_moves(9, french), Moves(std::vector<DpadPosition>{DPAD_UP, DPAD_UP, DPAD_LEFT})},
        {"Japanese: its kana button, right of English",
            language_pick_moves(0, IntroPicks{Language::Japanese, 0}), Moves(std::vector<DpadPosition>{DPAD_RIGHT})},
        {"Traditional Chinese from English",
            language_pick_moves(0, IntroPicks{Language::ChineseTraditional, 0}),
            Moves(std::vector<DpadPosition>{DPAD_RIGHT, DPAD_DOWN, DPAD_DOWN, DPAD_DOWN, DPAD_DOWN})},
        {"Italian, bottom left",
            language_pick_moves(0, IntroPicks{Language::Italian, 0}), Moves(std::vector<DpadPosition>(4, DPAD_DOWN))},
        {"a language the menu lacks",
            language_pick_moves(0, IntroPicks{Language::None, 0}), std::nullopt},
        {"a cursor outside the menu",
            language_pick_moves(LANGUAGE_SELECT_BUTTONS, french), std::nullopt},
        {"Model 1, where the cursor starts",
            look_pick_moves(0, IntroPicks{Language::English, 0}), Moves(std::vector<DpadPosition>{})},
        {"Model 3, Dawn's first look (lab run 3)",
            look_pick_moves(0, french), Moves(std::vector<DpadPosition>{DPAD_RIGHT, DPAD_RIGHT})},
        {"Model 3 from Model 8",
            look_pick_moves(7, french), Moves(std::vector<DpadPosition>{DPAD_UP, DPAD_LEFT})},
        {"Model 8, the last",
            look_pick_moves(0, IntroPicks{Language::English, 7}), Moves(std::vector<DpadPosition>{DPAD_RIGHT, DPAD_RIGHT, DPAD_RIGHT, DPAD_DOWN})},
        {"no Model 9",
            look_pick_moves(0, IntroPicks{Language::English, 8}), std::nullopt},
        {"a cursor outside the look grid",
            look_pick_moves(LOOK_SELECT_PORTRAITS, french), std::nullopt},
    };
    for (const Case& c : cases){
        if (c.actual != c.expected){
            return std::string("Wrong presses for ") + std::string(c.what) + ".";
        }
    }
    return check_intro_menu_sizes();
}

//  Each select takes its pick, then A if it still shows. Other screens take
//  A. The player's keyboard with a pick still to make fails.
UnitTestResult test_intro_pick_steps(Logger&){
    struct Case{
        std::string_view what;
        std::string_view screen;
        bool language_pending;
        bool look_pending;
        IntroStep expected;
    };
    const Case cases[] = {
        {"the language select",             "language-select", true, true, IntroStep::PICK_LANGUAGE},
        {"the language select, picked",     "language-select", false, true, IntroStep::ADVANCE},
        {"a logo before the language",      "other",    true,   true,   IntroStep::ADVANCE},
        {"a dialogue before the language",  "dialog",   true,   true,   IntroStep::ADVANCE},
        {"the look select",                 "look-select", false, true, IntroStep::PICK_LOOK},
        {"the look select, picked",         "look-select", false, false, IntroStep::ADVANCE},
        {"Rowan, look to pick",             "dialog",   false,  true,   IntroStep::ADVANCE},
        {"a cutscene, look to pick",        "other",    false,  true,   IntroStep::ADVANCE},
        {"both picked",                     "dialog",   false,  false,  IntroStep::ADVANCE},
        {"black, look to pick",             "black",    false,  true,   IntroStep::WAIT},
        {"the user select",                 "user-select", true, true,  IntroStep::SELECT_USER},
        {"the keyboard, language missed",   "keyboard", true,   false,  IntroStep::FAIL_PICK_MISSED},
        {"the keyboard, look missed",       "keyboard", false,  true,   IntroStep::FAIL_PICK_MISSED},
        {"the keyboard, both picked",       "keyboard", false,  false,  IntroStep::ENTER_PLAYER_NAME},
    };
    for (const Case& c : cases){
        IntroProgress progress = intro_progress(0);
        progress.language_pending = c.language_pending;
        progress.look_pending = c.look_pending;
        if (decide_intro_step(c.screen, progress) != c.expected){
            return std::string("Wrong step for ") + std::string(c.what) + ".";
        }
    }
    const IntroProgress first_look = intro_start(IntroPicks{Language::English, 0});
    if (!first_look.language_pending || first_look.look_pending){
        return "The language is always picked on its select; the first look needs no grid.";
    }
    const IntroProgress run3 = intro_start(IntroPicks{Language::French, 2});
    if (!run3.language_pending || !run3.look_pending){
        return "French and Model 3 need both menus.";
    }
    return true;
}

//  The intro's two selects from lab-B 2026-11-09 (each cursor on its first
//  item), the screens one A before and after the look select, and screens
//  that are neither. Screenshots are in the unit test resources; a missing
//  one is passed over, and the test is skipped when all are.
UnitTestResult test_intro_menu_screenshots(Logger& logger){
    ImageRGB32 black(1920, 1080);
    black.fill(0xff000000);
    if (language_select_cursor(black) || look_select_cursor(black)){
        return "A black screen read as an intro menu.";
    }
    struct Case{
        std::string_view file;
        std::optional<size_t> language;
        std::optional<size_t> look;
    };
    const Case cases[] = {
        {.file = "PokemonBDSP/AutoStory/LanguageSelect-20261109.png", .language = 0, .look = std::nullopt},
        {.file = "PokemonBDSP/AutoStory/LookSelect-20261109.png", .language = std::nullopt, .look = 0},
        {.file = "PokemonBDSP/AutoStory/LookSelectBefore-20261109.png", .language = std::nullopt, .look = std::nullopt},
        {.file = "PokemonBDSP/AutoStory/LookSelectAfterA-20261109.png", .language = std::nullopt, .look = std::nullopt},
        {.file = "PokemonBDSP/AutoStory/OreburghCenterCounter-20261108-094735.png", .language = std::nullopt, .look = std::nullopt},
        {.file = "PokemonBDSP/AutoStory/OreburghCenterDoorstep-20261108.png", .language = std::nullopt, .look = std::nullopt},
    };
    size_t read = 0;
    for (const Case& c : cases){
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
        if (!Filesystem::is_regular_file(path)){
            logger.log("Screenshot not found: " + path);
            continue;
        }
        read++;
        const ImageRGB32 screen(path);
        const std::optional<size_t> language = language_select_cursor(screen);
        const std::optional<size_t> look = look_select_cursor(screen);
        logger.log(std::string(c.file) + ": language " + (language ? std::to_string(*language) : "none") +
            ", look " + (look ? std::to_string(*look) : "none"));
        if (language != c.language || look != c.look){
            return std::string(c.file) + ": wrong intro menu read.";
        }
    }
    if (read == 0){
        return UnitTestResult(UnitTestResult::SKIPPED, "No intro menu screenshots found.");
    }
    return true;
}


//  The Mart's doorway on the failure screenshots of r121a and r122a, kept in
//  the unit test resources (the CommandLineTests checkout): where it is
//  relative to the player, who is in the middle of the screen.
UnitTestResult test_lit_doorway_screenshots(Logger& logger){
    struct Case{
        std::string file;
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
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
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


//  The save prompt r124a took beside Sandgem's Poke Mart, sure it stood on
//  Route 202 (in CommandLineTests under PokemonBDSP/AutoStory): its card
//  names Sandgem Town, which the save must read before confirming.
UnitTestResult test_save_screen_location(Logger& logger){
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


//  The two shots around r126's door walk: below the Center's sign, outside,
//  and on the entrance mat inside, where the lit floor read as a doorway and
//  the walk kept lining up with it (runs 2 and 3).
UnitTestResult test_pokemon_center_inside(Logger& logger){
    struct Case{
        std::string file;
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
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
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


//  Oreburgh's Center, lab-B 2026-11-08 (in CommandLineTests under
//  PokemonBDSP/AutoStory, skipped without them): 01.03's run in saw no fade
//  and stopped at the nurse's counter, the camera in close and the lit floor
//  down to the screen's bottom; on the exit mat; and outside on the doorstep
//  at sunset, the ground as orange as the Center's back wall.
UnitTestResult test_pokemon_center_counter(Logger& logger){
    struct Case{
        std::string file;
        bool inside;
    };
    const Case cases[] = {
        {.file = "PokemonBDSP/AutoStory/OreburghCenterCounter-20261108-094735.png", .inside = true},
        {.file = "PokemonBDSP/AutoStory/OreburghCenterExitMat-20261108.png", .inside = true},
        {.file = "PokemonBDSP/AutoStory/OreburghCenterDoorstep-20261108.png", .inside = false},
    };
    for (const Case& c : cases){
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
        }
        const ImageRGB32 screen(path);
        const bool inside = pokemon_center_inside(screen);
        logger.log(std::string(c.file) + ": inside " + std::to_string(inside));
        if (inside != c.inside){
            return std::string(c.file) + ": expected " + (c.inside ? "inside" : "outside") +
                " a Pokemon Center, read " + (inside ? "inside" : "outside") + ".";
        }
        if (inside && find_lit_doorway(screen)){
            return std::string(c.file) + ": inside, yet a lit doorway was read.";
        }
    }
    return true;
}


//  The School's lamp at night, from its doorsteps' row (2026-10-30): where
//  the door took us in, the lamp reads within half a tile of the door; a
//  tile either side, more.
UnitTestResult test_school_lamp(Logger& logger){
    const ImageFloatBox box(0.55, 0.20, 0.30, 0.10);
    struct Case{ std::string file; double min; double max; };
    const Case cases[] = {
        {"PokemonBDSP/AutoStory/JubilifeSchoolRightOfDoor-20261030.png", -1.5, -0.5},
        {"PokemonBDSP/AutoStory/JubilifeSchoolOneTileShort-20261030.png", -1.5, -0.5},
        {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor-20261030.png", -0.5, 0.5},
        {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor2-20261030.png", -0.5, 0.5},
        {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", -0.5, 0.5},
        {"PokemonBDSP/AutoStory/JubilifeSchoolOneTilePast-20261030.png", 0.5, 1.5},
    };
    for (const Case& c : cases){
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
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


//  The Options screen of a French game (console B, 2026-11-09; in
//  CommandLineTests under PokemonBDSP/AutoStory): read by position and
//  colour only, the cursor on Text Speed with Normal (the middle of three)
//  selected, and the first value of the two-value rows below it.
UnitTestResult test_options_french(Logger& logger){
    const std::string path = UNIT_TEST_RESOURCE_PATH() + "PokemonBDSP/AutoStory/OptionsFrench-20261109.png";
    if (!Filesystem::is_regular_file(path)){
        return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
    }
    const ImageRGB32 screen(path);
    if (!options_screen_shown(screen)){
        return "Options screen not recognised.";
    }
    const std::optional<double> y = options_selected_row_y(screen);
    if (!y || *y < 0.137 || *y > 0.177){
        return "Selected row not found on the first row.";
    }
    logger.log("Selected row at y=" + std::to_string(*y));
    if (options_highlighted_column(screen, *y, 3) != size_t(1)){
        return "Text Speed: Normal (column 1 of 3) not read.";
    }
    //  Battle Effects to Gyroscope: rows 2 to 6, the first value highlighted.
    for (size_t row = 2; row <= 6; row++){
        const double row_y = *y + 0.0858 * row;
        if (options_highlighted_column(screen, row_y, 2) != size_t(0)){
            return "Row " + std::to_string(row) + ": the first of two values not read.";
        }
    }
    ImageRGB32 black(1920, 1080);
    black.fill(0xff000000);
    if (options_screen_shown(black)){
        return "A black frame read as Options.";
    }
    return true;
}


//  The move picked from names and PP as read. lab-B 2026-11-09, French:
//  no name read, Piplup's Pound at 35 PP and Growl at 22 PP, and the old
//  "newest move that is not a status move" fallback used Growl until the
//  training gave up after 25 turns.
UnitTestResult test_choose_move(Logger&){
    struct Case{ std::string_view name; std::vector<MoveSlot> slots; size_t best; };
    const Case cases[] = {
        {"no name read, PP 35 and 22", {{"", 35}, {"", 22}}, 1},
        {"no name read, PP 35 and 30", {{"", 35}, {"", 30}}, 1},
        {"no name or PP read", {{"", -1}, {"", -1}}, 1},
        {"Scratch and Leer", {{"scratch", 35}, {"leer", 30}}, 1},
        {"Pound, Growl, Water Gun and Charm", {{"pound", 35}, {"growl", 40}, {"water-gun", 10}, {"charm", 20}}, 3},
        {"Water Gun out of PP", {{"pound", 35}, {"growl", 40}, {"water-gun", 0}, {"charm", 20}}, 1},
        {"slot 1 unread at 38 PP: a status move", {{"", 38}, {"", 40}, {"", 20}}, 3},
        {"Growl unread, Tackle read", {{"", 40}, {"tackle", 35}}, 2},
    };
    for (const Case& c : cases){
        const size_t best = choose_move(c.slots);
        if (best != c.best){
            return std::string(c.name) + ": picked move " + std::to_string(best) + ", expected " + std::to_string(c.best) + ".";
        }
    }
    return true;
}

//  Move menus saved on lab-B (SCREENSHOTS_PATH()/AutoStory/moves/), read in
//  the game's language. The French ones read nothing before the names came
//  from the move name dictionary.
UnitTestResult test_move_menu_screenshots(Logger& logger){
    struct Case{
        std::string file;
        Language language;
        std::vector<MoveSlot> slots;
        size_t best;
    };
    const Case cases[] = {
        {"PokemonBDSP/AutoStory/MoveMenuFrenchPoundGrowl-20261109-192734.png", Language::French,
            {{"pound", 35}, {"growl", 40}}, 1},
        {"PokemonBDSP/AutoStory/MoveMenuFrenchPoundGrowl16-20261109-193035.png", Language::French,
            {{"pound", 35}, {"growl", 16}}, 1},
        {"PokemonBDSP/AutoStory/MoveMenuEnglishPiplup-20261108-181435.png", Language::English,
            {{"pound", 35}, {"growl", 40}, {"water-gun", 10}, {"charm", 20}}, 3},
    };
    const Language story = story_language();
    for (const Case& c : cases){
        const std::string path = UNIT_TEST_RESOURCE_PATH() + std::string(c.file);
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
        }
        const ImageRGB32 screen(path);
        set_story_language(c.language);
        const std::vector<MoveSlot> slots = read_move_menu(screen, detect_battle_move_menu(screen));
        set_story_language(story);
        std::string read;
        for (size_t i = 0; i < slots.size(); i++){
            read += " [" + slots[i].describe(i) + "]";
        }
        logger.log(std::string(c.file) + ":" + read);
        if (slots.size() != c.slots.size()){
            return std::string(c.file) + ": " + std::to_string(slots.size()) + " moves, expected " + std::to_string(c.slots.size()) + ".";
        }
        for (size_t i = 0; i < slots.size(); i++){
            if (slots[i].slug != c.slots[i].slug || slots[i].pp != c.slots[i].pp){
                return std::string(c.file) + ": move " + std::to_string(i + 1) + " read as" + read + ".";
            }
        }
        if (choose_move(slots) != c.best){
            return std::string(c.file) + ": picked move " + std::to_string(choose_move(slots)) + ", expected " + std::to_string(c.best) + ".";
        }
    }
    return true;
}


struct AutoStoryActionsInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryActionsExpectation{
};

struct AutoStoryActionsCase{
    AutoStoryActionsInput input;
    AutoStoryActionsExpectation expectation;
};

class Test_AutoStoryActions : public UnitTest{
public:
    explicit Test_AutoStoryActions(AutoStoryActionsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryActionsCase m_case;
};

} // namespace


const AutoStoryActionsCase AUTO_STORY_ACTIONS_CASES[] = {
    {
        .input = {"new-game intro decisions", test_intro_decisions},
        .expectation = {},
    },
    {
        .input = {"new-game intro: the rival's name confirmed with A", test_intro_rival_confirm},
        .expectation = {},
    },
    {
        .input = {"new-game intro: B on unknown screens, then X for the menu", test_intro_before_menu},
        .expectation = {},
    },
    {
        .input = {"new-game intro: in control without the menu read", test_intro_in_control},
        .expectation = {},
    },
    {
        .input = {"new-game intro: BDSP or stop", test_intro_gate},
        .expectation = {},
    },
    {
        .input = {"new-game intro: language and look presses", test_intro_pick_moves},
        .expectation = {},
    },
    {
        .input = {"new-game intro: steps until the picks are made", test_intro_pick_steps},
        .expectation = {},
    },
    {
        .input = {"intro menu screenshots", test_intro_menu_screenshots},
        .expectation = {},
    },
    {
        .input = {"lit doorway screenshots", test_lit_doorway_screenshots},
        .expectation = {},
    },
    {
        .input = {"save screen location", test_save_screen_location},
        .expectation = {},
    },
    {
        .input = {"Pokemon Center inside", test_pokemon_center_inside},
        .expectation = {},
    },
    {
        .input = {"Pokemon Center inside: Oreburgh's counter, exit mat and doorstep", test_pokemon_center_counter},
        .expectation = {},
    },
    {
        .input = {"Jubilife School lamp", test_school_lamp},
        .expectation = {},
    },
    {
        .input = {"Options read by position, French", test_options_french},
        .expectation = {},
    },
    {
        .input = {"battle moves: the pick from names and PP", test_choose_move},
        .expectation = {},
    },
    {
        .input = {"battle moves: move menu screenshots in each language", test_move_menu_screenshots},
        .expectation = {},
    },
};

void add_tests_AutoStory_Actions(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_ACTIONS_CASES){
        database.add<Test_AutoStoryActions>(item);
    }
    //  The menu cursor's own tests, with those of the intro, its first user.
    add_tests_MenuCursor(database);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  PokemonLZA AutoStory - New game intro Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdlib>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryIntro.h"
#include "PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryLanguageList.h"
#include "PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryIntro_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Keys = std::vector<IntroKey>;

Keys repeated(IntroKey key, size_t count){
    return Keys(count, key);
}

Keys joined(std::initializer_list<Keys> parts){
    Keys all;
    for (const Keys& part : parts){
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

UnitTestResult test_language_index(Logger&){
    struct Row{
        Language language;
        size_t index;
    };
    //  Down the left column, then the right; entry 2 is Latin American
    //  Spanish, which Language lacks.
    const Row rows[] = {
        {Language::English, 0},
        {Language::Spanish, 1},
        {Language::French, 3},
        {Language::German, 4},
        {Language::Italian, 5},
        {Language::Japanese, 6},
        {Language::Korean, 7},
        {Language::ChineseSimplified, 8},
        {Language::ChineseTraditional, 9},
    };
    for (const Row& row : rows){
        if (language_list_index(row.language) != row.index){
            return std::format("{}: expected entry {}.", language_data(row.language).code, row.index);
        }
    }
    if (language_list_index(Language::None)){
        return "Language::None has no entry in the list.";
    }
    return true;
}

UnitTestResult test_language_moves(Logger&){
    struct Row{
        size_t from;
        size_t to;
        Keys expected;
    };
    const Row rows[] = {
        {0, 0, {}},
        //  English to Deutsch: down the left column.
        {0, 4, repeated(IntroKey::DOWN, 4)},
        //  English to Italiano: across.
        {0, 5, {IntroKey::RIGHT}},
        //  English to Traditional Chinese: across, then down.
        {0, 9, joined({{IntroKey::RIGHT}, repeated(IntroKey::DOWN, 4)})},
        //  Korean to Espanol (Espana): across, then up.
        {7, 1, joined({{IntroKey::LEFT}, repeated(IntroKey::UP, 1)})},
        //  Deutsch to Italiano: no wrap round the bottom edge.
        {4, 5, joined({{IntroKey::RIGHT}, repeated(IntroKey::UP, 4)})},
    };
    for (const Row& row : rows){
        const auto moves = language_list_moves(row.from, row.to);
        if (!moves || *moves != row.expected){
            return std::format("Entry {} to {}: expected {} presses.", row.from, row.to, row.expected.size());
        }
    }
    if (language_list_moves(0, LANGUAGE_LIST_SIZE) || language_list_moves(LANGUAGE_LIST_SIZE, 0)){
        return "Entries outside the list have no moves.";
    }
    return true;
}

//  The language list drawn as console B shows it (1920 x 1080): dark blue,
//  10 slate boxes 576 x 84, the columns at x 347 and 996, the rows from
//  y 342, 108 apart, entry `highlighted` (if any) white.
ImageRGB32 language_list_frame(std::optional<size_t> highlighted){
    ImageRGB32 image(1920, 1080);
    image.fill(0xff091838);
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        const size_t left = index < LANGUAGE_LIST_ROWS ? 347 : 996;
        const size_t top = 342 + 108 * (index % LANGUAGE_LIST_ROWS);
        const uint32_t color = highlighted == index ? 0xffffffff : 0xff363c53;
        for (size_t y = top; y < top + 84; y++){
            for (size_t x = left; x < left + 576; x++){
                image.pixel(x, y) = color;
            }
        }
    }
    return image;
}

UnitTestResult test_language_list_synthetic(Logger&){
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        if (read_language_list_cursor(language_list_frame(index)) != index){
            return std::format("Expected the highlight read on entry {}.", index);
        }
    }
    if (is_language_list(language_list_frame(std::nullopt))){
        return "Expected no list without a highlight.";
    }
    ImageRGB32 two = language_list_frame(0);
    for (size_t y = 450; y < 534; y++){
        for (size_t x = 347; x < 923; x++){
            two.pixel(x, y) = 0xffffffff;
        }
    }
    if (is_language_list(two)){
        return "Expected no list with two highlights.";
    }
    for (const uint32_t fill : {0xff000000u, 0xffffffffu, 0xff363c53u}){
        ImageRGB32 flat(1920, 1080);
        flat.fill(fill);
        if (is_language_list(flat)){
            return std::format("Expected a flat {:08x} frame not to be the list.", fill);
        }
    }
    return true;
}

UnitTestResult test_language_list_empty(Logger&){
    if (is_language_list(ImageRGB32()) || is_language_list(ImageRGB32(1, 1)) || is_language_list(ImageRGB32(40, 20))){
        return "Expected empty and tiny frames not to be the list.";
    }
    return true;
}

//  The console's frame, from the folder in PLZA_INTRO_SCREENS
//  (~/BDSPLab/intro-screens/plza on the lab Mac): language-list.png, the
//  list as a new game opens it, English highlighted.
UnitTestResult test_language_list_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    const ImageRGB32 frame(std::format("{}/language-list.png", folder));
    const std::optional<size_t> cursor = read_language_list_cursor(frame);
    if (!cursor){
        return "language-list.png: expected the language list, read none.";
    }
    if (*cursor != 0){
        return std::format("language-list.png: expected English (entry 0) highlighted, read entry {}.", *cursor);
    }
    return true;
}

//  00.00's start on frames whose HOME reading is given: HOME, the
//  language list, anything else, and no video.
UnitTestResult test_launch_start_synthetic(Logger&){
    const ImageRGB32 list = language_list_frame(0);
    ImageRGB32 game(1920, 1080);
    game.fill(0xff405060);
    const ImageRGB32 empty;
    struct Row{
        std::string_view name;
        const ImageRGB32* frame;
        bool home;
        LaunchStart expected;
    };
    const Row rows[] = {
        {"HOME", &game, true, LaunchStart::HOME_MENU},
        {"a game frame", &game, false, LaunchStart::OTHER},
        {"the language list", &list, false, LaunchStart::FIRST_SCREEN},
        {"the language list, HOME read", &list, true, LaunchStart::FIRST_SCREEN},
        {"an empty frame", &empty, false, LaunchStart::NO_VIDEO},
        {"an empty frame, HOME read", &empty, true, LaunchStart::NO_VIDEO},
    };
    for (const Row& row : rows){
        if (read_launch_start(*row.frame, row.home, is_language_list) != row.expected){
            return std::format("{}: expected start {}.", row.name, (int)row.expected);
        }
    }
    if (read_launch_start(game, true, ScreenCheck()) != LaunchStart::HOME_MENU){
        return "HOME with no first-screen detector: expected a launch.";
    }
    return true;
}

//  00.00's start on console B's frames, from the folder in
//  PLZA_INTRO_SCREENS: language-list.png, and if present home.png (the HOME
//  menu) and in-game.png (the station). The HOME reading is
//  HomeMenuDetector's, which needs a console: here it is given.
UnitTestResult test_launch_start_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    struct Row{
        std::string_view file;
        bool home;
        LaunchStart expected;
    };
    const Row rows[] = {
        {"language-list.png", false, LaunchStart::FIRST_SCREEN},
        {"home.png", true, LaunchStart::HOME_MENU},
        {"in-game.png", false, LaunchStart::OTHER},
    };
    for (const Row& row : rows){
        const std::string path = std::format("{}/{}", folder, row.file);
        if (!Filesystem::exists(Filesystem::Path(path))){
            if (row.expected == LaunchStart::FIRST_SCREEN){
                return std::format("{} is missing.", row.file);
            }
            continue;
        }
        const ImageRGB32 frame(path);
        if (read_launch_start(frame, row.home, is_language_list) != row.expected){
            return std::format("{}: expected start {}.", row.file, (int)row.expected);
        }
    }
    return true;
}

//  00.00's moves: HOME launches; a game in front (mid-scene or on its
//  first screen) gets one HOME press, then is refused if HOME still does
//  not show; no video is refused without a press.
UnitTestResult test_launch_moves(Logger&){
    struct Row{
        LaunchStart start;
        bool home_pressed;
        LaunchMove expected;
    };
    const Row rows[] = {
        {LaunchStart::HOME_MENU, false, LaunchMove::LAUNCH},
        {LaunchStart::HOME_MENU, true, LaunchMove::LAUNCH},
        {LaunchStart::OTHER, false, LaunchMove::PRESS_HOME},
        {LaunchStart::OTHER, true, LaunchMove::REFUSE},
        {LaunchStart::FIRST_SCREEN, false, LaunchMove::PRESS_HOME},
        {LaunchStart::FIRST_SCREEN, true, LaunchMove::REFUSE},
        {LaunchStart::NO_VIDEO, false, LaunchMove::REFUSE},
        {LaunchStart::NO_VIDEO, true, LaunchMove::REFUSE},
    };
    for (const Row& row : rows){
        if (next_launch_move(row.start, row.home_pressed) != row.expected){
            return std::format(
                "start {}, HOME pressed {}: expected move {}.",
                (int)row.start, row.home_pressed, (int)row.expected
            );
        }
    }
    return true;
}

//  The HOME press on console B's frames, from the folder in
//  PLZA_INTRO_SCREENS: in-game.png (a game in front) gets the press, and
//  home.png, the screen it opens, launches.
UnitTestResult test_launch_moves_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    const std::string in_game = std::format("{}/in-game.png", folder);
    const std::string home = std::format("{}/home.png", folder);
    if (!Filesystem::exists(Filesystem::Path(in_game)) || !Filesystem::exists(Filesystem::Path(home))){
        return UnitTestResult(UnitTestResult::SKIPPED, "in-game.png or home.png is missing.");
    }
    const LaunchStart before = read_launch_start(ImageRGB32(in_game), false, is_language_list);
    if (next_launch_move(before, false) != LaunchMove::PRESS_HOME){
        return "in-game.png: expected a HOME press.";
    }
    const LaunchStart after = read_launch_start(ImageRGB32(home), true, is_language_list);
    if (next_launch_move(after, true) != LaunchMove::LAUNCH){
        return "home.png after the HOME press: expected a launch.";
    }
    return true;
}

UnitTestResult test_look_moves(Logger&){
    struct Row{
        size_t from;
        size_t to;
        Keys expected;
    };
    const Row rows[] = {
        {1, 1, {}},
        //  Along the top row.
        {1, 4, repeated(IntroKey::RIGHT, 3)},
        {3, 2, {IntroKey::LEFT}},
        //  Down a column.
        {1, 5, {IntroKey::DOWN}},
        //  Corner to corner: across, then down or up.
        {1, 8, joined({repeated(IntroKey::RIGHT, 3), {IntroKey::DOWN}})},
        {8, 1, joined({repeated(IntroKey::LEFT, 3), {IntroKey::UP}})},
        {6, 3, joined({{IntroKey::RIGHT}, {IntroKey::UP}})},
    };
    for (const Row& row : rows){
        const auto moves = look_picker_moves(row.from, row.to);
        if (!moves || *moves != row.expected){
            return std::format("Look {} to {}: expected {} presses.", row.from, row.to, row.expected.size());
        }
    }
    if (look_picker_moves(0, 1) || look_picker_moves(1, 0) || look_picker_moves(1, 9) || look_picker_moves(9, 1)){
        return "Looks 0 and 9 are outside the grid.";
    }
    return true;
}

//  How a highlighted face is drawn in look_picker_frame().
enum class LookHighlight{
    //  A white frame round the tile.
    FRAME,
    //  A light tile.
    TILE,
};

//  The look picker drawn as console B shows it (1920 x 1080): dark blue,
//  8 slate tiles 240 x 240, the columns' left edges at x 373, 685, 997 and
//  1309, the rows' tops at y 324 and 609; look `highlighted` (if any) drawn
//  as `style`.
ImageRGB32 look_picker_frame(std::optional<size_t> highlighted, LookHighlight style = LookHighlight::FRAME){
    const size_t lefts[] = {373, 685, 997, 1309};
    const size_t tops[] = {324, 609};
    ImageRGB32 image(1920, 1080);
    image.fill(0xff0a1b3e);
    for (size_t look = 1; look <= LOOK_COUNT; look++){
        const size_t left = lefts[(look - 1) % 4];
        const size_t top = tops[(look - 1) / 4];
        const bool lit = highlighted == look;
        if (lit && style == LookHighlight::FRAME){
            for (size_t y = top - 10; y < top + 250; y++){
                for (size_t x = left - 10; x < left + 250; x++){
                    image.pixel(x, y) = 0xffffffff;
                }
            }
        }
        const uint32_t tile = lit && style == LookHighlight::TILE ? 0xffc8d2dc : 0xff202f50;
        for (size_t y = top; y < top + 240; y++){
            for (size_t x = left; x < left + 240; x++){
                image.pixel(x, y) = tile;
            }
        }
    }
    return image;
}

UnitTestResult test_look_picker_synthetic(Logger&){
    const ImageRGB32 none = look_picker_frame(std::nullopt);
    if (!is_look_picker(none) || read_look_picker_cursor(none)){
        return "Expected the picker with no highlight.";
    }
    for (const LookHighlight style : {LookHighlight::FRAME, LookHighlight::TILE}){
        for (size_t look = 1; look <= LOOK_COUNT; look++){
            const ImageRGB32 frame = look_picker_frame(look, style);
            if (!is_look_picker(frame) || read_look_picker_cursor(frame) != look){
                return std::format("Expected the highlight read on look {} (style {}).", look, (int)style);
            }
        }
    }
    ImageRGB32 two = look_picker_frame(1, LookHighlight::TILE);
    for (size_t y = 609; y < 849; y++){
        for (size_t x = 1309; x < 1549; x++){
            two.pixel(x, y) = 0xffc8d2dc;
        }
    }
    if (is_look_picker(two) || read_look_picker_cursor(two)){
        return "Expected no picker with two highlights.";
    }
    if (is_look_picker(language_list_frame(0))){
        return "Expected the language list not to be the picker.";
    }
    for (const uint32_t fill : {0xff000000u, 0xffffffffu, 0xff202f50u, 0xff0a1b3eu}){
        ImageRGB32 flat(1920, 1080);
        flat.fill(fill);
        if (is_look_picker(flat)){
            return std::format("Expected a flat {:08x} frame not to be the picker.", fill);
        }
    }
    if (is_look_picker(ImageRGB32()) || is_look_picker(ImageRGB32(1, 1)) || read_look_picker_cursor(ImageRGB32(40, 20))){
        return "Expected empty and tiny frames not to be the picker.";
    }
    return true;
}

//  The console's frames, from the folder in PLZA_INTRO_SCREENS: look-picker.png,
//  the picker as it opens (no face highlighted yet), and the screens before
//  and after it, which are not the picker.
UnitTestResult test_look_picker_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    const ImageRGB32 picker(std::format("{}/look-picker.png", folder));
    if (!is_look_picker(picker)){
        return "look-picker.png: expected the look picker.";
    }
    if (const std::optional<size_t> cursor = read_look_picker_cursor(picker)){
        return std::format("look-picker.png: expected no highlight, read look {}.", *cursor);
    }
    for (const std::string_view name : {"language-list.png", "keyboard.png", "confirm-choices.png"}){
        const std::string path = std::format("{}/{}", folder, name);
        if (!Filesystem::exists(Filesystem::Path(path))){
            continue;
        }
        if (is_look_picker(ImageRGB32(path))){
            return std::format("{}: expected not the look picker.", name);
        }
    }
    return true;
}

//  "Confirm Choices" drawn as console B shows it (1920 x 1080): dark blue,
//  three slate rows x 144 to 742, the two choices from y 578 and 662 (68
//  high) and "Ready!" from y 790 (112 high), row `highlighted` (if any)
//  white.
ImageRGB32 confirm_choices_frame(std::optional<size_t> highlighted){
    ImageRGB32 image(1920, 1080);
    image.fill(0xff0a1d40);
    const size_t tops[] = {578, 662, 790};
    const size_t heights[] = {68, 68, 112};
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        const uint32_t color = highlighted == row ? 0xffffffff : 0xff23344f;
        for (size_t y = tops[row]; y < tops[row] + heights[row]; y++){
            for (size_t x = 144; x < 742; x++){
                image.pixel(x, y) = color;
            }
        }
    }
    return image;
}

//  From each row of "Confirm Choices": DOWN to "Ready!", then A.
UnitTestResult test_confirm_choices_moves(Logger&){
    struct Row{
        size_t cursor;
        Keys expected;
    };
    const Row rows[] = {
        {0, {IntroKey::DOWN, IntroKey::DOWN, IntroKey::A}},
        {1, {IntroKey::DOWN, IntroKey::A}},
        {2, {IntroKey::A}},
    };
    for (const Row& row : rows){
        const auto moves = confirm_choices_moves(row.cursor);
        if (!moves || *moves != row.expected){
            return std::format("Row {}: expected {} presses.", row.cursor, row.expected.size());
        }
    }
    if (confirm_choices_moves(CONFIRM_CHOICES_READY_ROW + 1)){
        return "A row past \"Ready!\" has no moves.";
    }
    return true;
}

//  01.00's start check (require_confirm_choices()) on synthetic frames: it
//  reads the highlight on each row of "Confirm Choices", and refuses the
//  other intro screens, flat and empty frames, and a screen with no row or
//  two rows highlighted.
UnitTestResult test_confirm_choices_synthetic(Logger&){
    for (size_t row = 0; row <= CONFIRM_CHOICES_READY_ROW; row++){
        if (read_confirm_choices_cursor(confirm_choices_frame(row)) != row){
            return std::format("Expected the highlight read on row {}.", row);
        }
    }
    if (is_confirm_choices(confirm_choices_frame(std::nullopt))){
        return "Expected no Confirm Choices without a highlight.";
    }
    ImageRGB32 two = confirm_choices_frame(0);
    for (size_t y = 662; y < 730; y++){
        for (size_t x = 144; x < 742; x++){
            two.pixel(x, y) = 0xffffffff;
        }
    }
    if (is_confirm_choices(two)){
        return "Expected no Confirm Choices with two highlights.";
    }
    for (size_t index = 0; index < LANGUAGE_LIST_SIZE; index++){
        if (is_confirm_choices(language_list_frame(index))){
            return std::format("Expected the language list (entry {} highlighted) not to be Confirm Choices.", index);
        }
    }
    for (const uint32_t fill : {0xff000000u, 0xffffffffu, 0xff0a1d40u, 0xff23344fu}){
        ImageRGB32 flat(1920, 1080);
        flat.fill(fill);
        if (is_confirm_choices(flat)){
            return std::format("Expected a flat {:08x} frame not to be Confirm Choices.", fill);
        }
    }
    if (is_confirm_choices(ImageRGB32()) || is_confirm_choices(ImageRGB32(1, 1)) || is_confirm_choices(ImageRGB32(40, 20))){
        return "Expected empty and tiny frames not to be Confirm Choices.";
    }
    return true;
}

//  01.00's start check on console B's frames, from the folder in
//  PLZA_INTRO_SCREENS: it accepts confirm-choices.png, "Change hair
//  color" highlighted as the screen opens, and refuses every other intro
//  screen there.
UnitTestResult test_confirm_choices_capture(Logger&){
    const auto* folder = std::getenv("PLZA_INTRO_SCREENS");
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_INTRO_SCREENS is not a folder.");
    }
    const std::optional<size_t> cursor = read_confirm_choices_cursor(ImageRGB32(std::format("{}/confirm-choices.png", folder)));
    if (cursor != size_t(0)){
        return "confirm-choices.png: expected Confirm Choices with row 0 highlighted.";
    }
    for (const std::string_view name : {"home.png", "in-game.png", "language-list.png", "look-picker.png", "keyboard.png"}){
        const std::string path = std::format("{}/{}", folder, name);
        if (!Filesystem::exists(Filesystem::Path(path))){
            continue;
        }
        if (is_confirm_choices(ImageRGB32(path))){
            return std::format("{}: expected 01.00 to refuse it.", name);
        }
    }
    return true;
}

UnitTestResult test_name_keys(Logger&){
    //  Kosmos: a capital K by Shift, the rest as plain keys.
    const auto kosmos = name_keys("Kosmos", false);
    if (!kosmos || kosmos->keys != "KOSMOS" || !kosmos->shift_first){
        return "Kosmos: expected keys KOSMOS and Shift first.";
    }
    //  A keyboard that capitalizes by itself needs no Shift for it.
    const auto auto_caps = name_keys("Kosmos", true);
    if (!auto_caps || auto_caps->keys != "KOSMOS" || auto_caps->shift_first){
        return "Kosmos on a capitalizing keyboard: expected no Shift.";
    }
    //  A lower case name there needs Shift to undo it, and without it none.
    const auto lower_auto = name_keys("kosmos", true);
    if (!lower_auto || !lower_auto->shift_first){
        return "kosmos on a capitalizing keyboard: expected Shift first.";
    }
    const auto lower = name_keys("kosmos", false);
    if (!lower || lower->shift_first){
        return "kosmos: expected no Shift.";
    }
    //  A digit first takes no Shift.
    const auto digit = name_keys("7up", true);
    if (!digit || digit->keys != "7UP" || digit->shift_first){
        return "7up: expected keys 7UP and no Shift.";
    }
    return true;
}

UnitTestResult test_name_refused(Logger&){
    for (const std::string_view name : {"", "Ko smos", "Kosmos!", "KOsmos", "Kosmo\xC3\x9F"}){
        if (name_keys(name, false)){
            return std::format("\"{}\" is not a name the typer can make.", name);
        }
    }
    return true;
}

UnitTestResult test_new_game_problem(Logger&){
    NewGameChoices choices;
    if (!new_game_problem(Language::English, choices).empty()){
        return "The defaults are a valid new game.";
    }
    if (new_game_problem(Language::None, choices).empty()){
        return "No language is a problem.";
    }
    for (const size_t look : {size_t(0), size_t(9)}){
        choices.look = look;
        if (new_game_problem(Language::English, choices).empty()){
            return std::format("Look {} is a problem.", look);
        }
    }
    choices.look = 1;
    choices.name = "KOsmos";
    if (new_game_problem(Language::English, choices).empty()){
        return "A second capital is a problem.";
    }
    return true;
}

struct IntroInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct IntroExpectation{
};

struct IntroCase{
    IntroInput input;
    IntroExpectation expectation;
};

const IntroCase INTRO_CASES[] = {
    {
        .input = {"Intro: the language list's entries", test_language_index},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list's cursor moves", test_language_moves},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list's highlight, synthetic frames", test_language_list_synthetic},
        .expectation = {},
    },
    {
        .input = {"Intro: empty frames are not the language list", test_language_list_empty},
        .expectation = {},
    },
    {
        .input = {"Intro: the language list on console B (PLZA_INTRO_SCREENS)", test_language_list_capture},
        .expectation = {},
    },
    {
        .input = {"Intro: the start screen, synthetic frames", test_launch_start_synthetic},
        .expectation = {},
    },
    {
        .input = {"Intro: the start screen on console B (PLZA_INTRO_SCREENS)", test_launch_start_capture},
        .expectation = {},
    },
    {
        .input = {"Intro: the launch step's moves", test_launch_moves},
        .expectation = {},
    },
    {
        .input = {"Intro: the HOME press on console B (PLZA_INTRO_SCREENS)", test_launch_moves_capture},
        .expectation = {},
    },
    {
        .input = {"Intro: the look picker's moves", test_look_moves},
        .expectation = {},
    },
    {
        .input = {"Intro: the look picker's highlight, synthetic frames", test_look_picker_synthetic},
        .expectation = {},
    },
    {
        .input = {"Intro: the look picker on console B (PLZA_INTRO_SCREENS)", test_look_picker_capture},
        .expectation = {},
    },
    {
        .input = {"Intro: Confirm Choices' moves to Ready!", test_confirm_choices_moves},
        .expectation = {},
    },
    {
        .input = {"Intro: 01.00's start check on Confirm Choices, synthetic frames", test_confirm_choices_synthetic},
        .expectation = {},
    },
    {
        .input = {"Intro: 01.00's start check on console B (PLZA_INTRO_SCREENS)", test_confirm_choices_capture},
        .expectation = {},
    },
    {
        .input = {"Intro: the name's keys and Shift", test_name_keys},
        .expectation = {},
    },
    {
        .input = {"Intro: names the typer refuses", test_name_refused},
        .expectation = {},
    },
    {
        .input = {"Intro: the new game's problems", test_new_game_problem},
        .expectation = {},
    },
};

class Test_AutoStoryIntro : public UnitTest{
public:
    explicit Test_AutoStoryIntro(const IntroCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}", test_case.input.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    const IntroCase& m_case;
};

} // namespace


void add_tests_AutoStoryIntro(UnitTestDatabase& database){
    for (const IntroCase& item : INTRO_CASES){
        database.add<Test_AutoStoryIntro>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

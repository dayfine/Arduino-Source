/*  AutoStory Teach Screens Tests
 *
 *  The console's TEACH steps against a fake bag, TM CASE and party menu
 *  that answer as console B did on 2026-11-03 (FireRed): the bag left on
 *  ITEMS; KEY ITEMS with TEACHY TV, TM CASE, HELIX FOSSIL; TM03 WATER
 *  PULSE and TM39 ROCK TOMB in the TM CASE; a Wartortle Lv22 with BITE,
 *  TAIL WHIP, BUBBLE, WITHDRAW. That run's step failed with "couldn't open
 *  tm03 from the TM Case": the console's steps had no readers and stopped
 *  before the bag.
 *
 */

#include <algorithm>
#include <deque>
#include <format>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
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

struct FakeTm{
    std::string move;
    size_t count = 1;
};

//  The game's state behind the screens.
enum class FakeScreen{
    OVERWORLD,
    START_MENU,
    BAG,
    BAG_MENU,
    TM_CASE,
    TM_MENU,
    PROMPT,
    TEXT,
    //  Messages the reader doesn't place: "other".
    UNREAD_TEXT,
    DELETE_YES_NO,
    STOP_YES_NO,
    KNOWN_MOVES,
    //  A tutor's last line in the overworld, after its party menu.
    TUTOR_TEXT,
};

//  The bag, TM CASE and party menu as console B showed them.
struct FakeTeachGame{
    std::vector<std::string> pockets{"items", "key-items", "poke-balls"};
    size_t pocket = 0;
    std::vector<std::string> key_items{"teachy-tv", "tm-case", "helix-fossil", "cancel"};
    size_t bag_cursor = 0;
    std::vector<FakeTm> tms{{"water-pulse", 1}, {"rock-tomb", 1}};
    std::vector<std::string> moves{"bite", "tail-whip", "bubble", "withdraw"};
    std::set<std::string> compatible{"water-pulse", "rock-tomb", "mega-kick"};
    //  The move of the tutor the player faces; empty: none.
    std::string tutor_move = {};
    //  The party menu is the tutor's.
    bool tutoring = false;

    FakeScreen screen = FakeScreen::OVERWORLD;
    size_t cursor = 0;
    //  The messages left to page through, and the screen after them.
    std::deque<TeachText> texts = {};
    FakeScreen after_texts = FakeScreen::OVERWORLD;
    //  The messages after a move is forgotten ("1, 2 and... Poof!", "...
    //  forgot TAIL WHIP.", "And... learned WATER PULSE!") read as "other",
    //  as on console B (2026-11-03).
    bool forget_texts_unread = false;
    //  The TM in use, by row.
    size_t tm = 0;
    std::vector<std::string> presses = {};
    std::vector<std::string> dumps = {};
    std::string error = {};
};

std::vector<std::string> tm_rows(const FakeTeachGame& game){
    std::vector<std::string> rows;
    for (const FakeTm& tm : game.tms){
        if (tm.count > 0){
            rows.push_back(tm.move);
        }
    }
    return rows;
}

void show_texts(FakeTeachGame& game, std::deque<TeachText> texts, FakeScreen after){
    game.texts = std::move(texts);
    game.after_texts = after;
    game.screen = FakeScreen::TEXT;
}

void move_cursor(FakeTeachGame& game, TeachPress press, size_t rows){
    if (press == TeachPress::UP && game.cursor > 0){
        game.cursor--;
    }else if (press == TeachPress::DOWN && game.cursor + 1 < rows){
        game.cursor++;
    }
}

//  Where the party menu goes once done: the TM CASE, or a tutor's last line.
FakeScreen after_party(const FakeTeachGame& game){
    return game.tutoring ? FakeScreen::TUTOR_TEXT : FakeScreen::TM_CASE;
}

std::string tm_move(const FakeTeachGame& game){
    if (game.tutoring){
        return game.tutor_move;
    }
    const std::vector<std::string> rows = tm_rows(game);
    return game.tm < rows.size() ? rows[game.tm] : "";
}

void use_up(FakeTeachGame& game){
    if (game.tutoring){
        return;
    }
    const std::string move = tm_move(game);
    for (FakeTm& tm : game.tms){
        if (tm.move == move && tm.count > 0){
            tm.count--;
            return;
        }
    }
}

void choose_member(FakeTeachGame& game){
    const std::string move = tm_move(game);
    if (!game.compatible.contains(move)){
        show_texts(game, {TeachText::NOT_COMPATIBLE}, FakeScreen::PROMPT);
        return;
    }
    if (game.moves.size() < MOVE_SLOTS){
        game.moves.push_back(move);
        use_up(game);
        game.cursor = 0;
        show_texts(game, {TeachText::WANTS_TO_LEARN, TeachText::UNKNOWN}, after_party(game));
        return;
    }
    show_texts(game, {TeachText::WANTS_TO_LEARN, TeachText::KNOWS_FOUR}, FakeScreen::DELETE_YES_NO);
    game.cursor = 0;
}

void press_bag(FakeTeachGame& game, TeachPress press){
    const bool key_items = game.pockets[game.pocket] == "key-items";
    switch (press){
    case TeachPress::LEFT:
    case TeachPress::RIGHT:
        if (press == TeachPress::LEFT && game.pocket > 0){
            game.pocket--;
            game.cursor = 0;
        }else if (press == TeachPress::RIGHT && game.pocket + 1 < game.pockets.size()){
            game.pocket++;
            game.cursor = 0;
        }
        return;
    case TeachPress::A:
        if (key_items && game.key_items[game.cursor] == "tm-case"){
            game.screen = FakeScreen::BAG_MENU;
            game.bag_cursor = game.cursor;
            game.cursor = 0;
        }
        return;
    case TeachPress::B:
        game.screen = FakeScreen::START_MENU;
        return;
    default:
        move_cursor(game, press, key_items ? game.key_items.size() : 4);
    }
}

void press_bag_menu(FakeTeachGame& game, TeachPress press){
    if (press == TeachPress::A && game.cursor == 0){
        game.screen = FakeScreen::TM_CASE;
        game.cursor = 0;
    }else if (press == TeachPress::A || press == TeachPress::B){
        game.screen = FakeScreen::BAG;
        game.cursor = game.bag_cursor;
    }else{
        move_cursor(game, press, 3);
    }
}

void press_tm_case(FakeTeachGame& game, TeachPress press){
    const size_t rows = tm_rows(game).size();
    if (press == TeachPress::B || (press == TeachPress::A && game.cursor == rows)){
        game.screen = FakeScreen::BAG;
        game.cursor = game.bag_cursor;
    }else if (press == TeachPress::A){
        game.tm = game.cursor;
        game.screen = FakeScreen::TM_MENU;
        game.cursor = 0;
    }else{
        move_cursor(game, press, rows + 1);
    }
}

void press_tm_menu(FakeTeachGame& game, TeachPress press){
    if (press == TeachPress::A && game.cursor == 0){
        game.screen = FakeScreen::PROMPT;
    }else if (press == TeachPress::A || press == TeachPress::B){
        game.screen = FakeScreen::TM_CASE;
        game.cursor = game.tm;
    }else{
        move_cursor(game, press, 3);
    }
}

void press_yes_no(FakeTeachGame& game, TeachPress press){
    const bool yes = press == TeachPress::A && game.cursor == 0;
    const bool no = press == TeachPress::B || (press == TeachPress::A && game.cursor == 1);
    if (game.screen == FakeScreen::DELETE_YES_NO && yes){
        show_texts(game, {TeachText::WHICH_MOVE}, FakeScreen::KNOWN_MOVES);
        game.cursor = 0;
    }else if (game.screen == FakeScreen::STOP_YES_NO && yes){
        show_texts(game, {TeachText::DID_NOT_LEARN}, after_party(game));
        game.cursor = game.tm;
    }else if (no){
        game.screen = game.screen == FakeScreen::DELETE_YES_NO ? FakeScreen::STOP_YES_NO : FakeScreen::DELETE_YES_NO;
        game.cursor = 0;
    }else{
        move_cursor(game, press, 2);
    }
}

void press_known_moves(FakeTeachGame& game, TeachPress press){
    if (press == TeachPress::B || (press == TeachPress::A && game.cursor == MOVE_SLOTS)){
        game.screen = FakeScreen::STOP_YES_NO;
        game.cursor = 0;
    }else if (press == TeachPress::A){
        if (is_hm_move(game.moves[game.cursor])){
            return;
        }
        game.moves[game.cursor] = tm_move(game);
        use_up(game);
        show_texts(game, {TeachText::UNKNOWN, TeachText::UNKNOWN, TeachText::UNKNOWN}, after_party(game));
        if (game.forget_texts_unread){
            game.screen = FakeScreen::UNREAD_TEXT;
        }
        game.cursor = 0;
    }else{
        move_cursor(game, press, MOVE_SLOTS + 1);
    }
}

void press_game(FakeTeachGame& game, TeachPress press){
    switch (game.screen){
    case FakeScreen::OVERWORLD:
    case FakeScreen::START_MENU:
        game.error = std::format("pressed {} off the menus", teach_press_name(press));
        return;
    case FakeScreen::BAG:           press_bag(game, press); return;
    case FakeScreen::BAG_MENU:      press_bag_menu(game, press); return;
    case FakeScreen::TM_CASE:       press_tm_case(game, press); return;
    case FakeScreen::TM_MENU:       press_tm_menu(game, press); return;
    case FakeScreen::DELETE_YES_NO:
    case FakeScreen::STOP_YES_NO:   press_yes_no(game, press); return;
    case FakeScreen::KNOWN_MOVES:   press_known_moves(game, press); return;
    case FakeScreen::PROMPT:
        if (press == TeachPress::A){
            choose_member(game);
        }else if (press == TeachPress::B){
            game.screen = after_party(game);
            game.cursor = game.tm;
        }
        return;
    case FakeScreen::TUTOR_TEXT:
        game.screen = FakeScreen::OVERWORLD;
        return;
    case FakeScreen::TEXT:
    case FakeScreen::UNREAD_TEXT:
        game.texts.pop_front();
        if (game.texts.empty()){
            game.screen = game.after_texts;
        }
        return;
    }
}

TeachView view_of(const FakeTeachGame& game){
    switch (game.screen){
    case FakeScreen::OVERWORLD:
    case FakeScreen::START_MENU:
    case FakeScreen::UNREAD_TEXT:
    case FakeScreen::TUTOR_TEXT:
        return {};
    case FakeScreen::BAG:{
        const bool key_items = game.pockets[game.pocket] == "key-items";
        return {.screen = TeachScreen::BAG, .cursor = game.cursor, .pocket = game.pockets[game.pocket],
            .rows = key_items ? game.key_items : std::vector<std::string>{}};
    }
    case FakeScreen::BAG_MENU:
    case FakeScreen::TM_MENU:
        return {.screen = TeachScreen::ITEM_MENU, .cursor = game.cursor};
    case FakeScreen::TM_CASE:
        return {.screen = TeachScreen::TM_CASE, .cursor = game.cursor, .rows = tm_rows(game)};
    case FakeScreen::PROMPT:
        return {.screen = TeachScreen::PARTY_PROMPT, .text = TeachText::TEACH_WHICH};
    case FakeScreen::TEXT:
        return {.screen = TeachScreen::PARTY_TEXT, .text = game.texts.front()};
    case FakeScreen::DELETE_YES_NO:
        return {.screen = TeachScreen::PARTY_YES_NO, .cursor = game.cursor, .text = TeachText::DELETE_QUESTION};
    case FakeScreen::STOP_YES_NO:
        return {.screen = TeachScreen::PARTY_YES_NO, .cursor = game.cursor, .text = TeachText::STOP_QUESTION};
    case FakeScreen::KNOWN_MOVES:
        return {.screen = TeachScreen::KNOWN_MOVES, .cursor = game.cursor};
    }
    return {};
}

class FakeTeachScreens : public TeachScreens{
public:
    explicit FakeTeachScreens(FakeTeachGame& game)
        : m_game(game)
    {}

    virtual bool open_bag() override{
        if (m_game.screen != FakeScreen::OVERWORLD){
            m_game.error = "opened the bag off the overworld";
            return false;
        }
        m_game.screen = FakeScreen::BAG;
        m_game.cursor = 0;
        return true;
    }
    virtual bool talk_to_tutor() override{
        if (m_game.screen != FakeScreen::OVERWORLD || m_game.tutor_move.empty()){
            m_game.error = "talked to no tutor";
            return false;
        }
        m_game.tutoring = true;
        m_game.screen = FakeScreen::PROMPT;
        m_game.cursor = 0;
        return true;
    }
    virtual TeachView look() override{
        return view_of(m_game);
    }
    virtual void press(TeachPress press) override{
        m_game.presses.emplace_back(teach_press_name(press));
        if (m_game.presses.size() > 200){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "more than 200 presses");
        }
        press_game(m_game, press);
    }
    virtual bool to_overworld() override{
        //  B pages through a tutor's lines too.
        const bool tutor_lines = m_game.screen == FakeScreen::TUTOR_TEXT
            || (m_game.tutoring && m_game.after_texts == FakeScreen::TUTOR_TEXT
                && (m_game.screen == FakeScreen::TEXT || m_game.screen == FakeScreen::UNREAD_TEXT));
        if (tutor_lines || m_game.screen == FakeScreen::START_MENU || m_game.screen == FakeScreen::OVERWORLD){
            m_game.screen = FakeScreen::OVERWORLD;
            return true;
        }
        return false;
    }
    virtual void dump_screen(std::string_view label) override{
        m_game.dumps.emplace_back(label);
    }

private:
    FakeTeachGame& m_game;
};


const TeachPlan MEGA_KICK{.item = std::string(TEACH_TUTOR), .move = "mega-kick", .member = 0, .forget_move = "withdraw"};

struct ScreensCase{
    std::string name = {};
    std::function<void(FakeTeachGame&)> setup = {};
    TeachPlan plan = {.item = "tm03", .move = "water-pulse", .member = 0, .forget_move = "tail-whip"};
    TeachEnd end = TeachEnd::LEARNED;
    std::vector<std::string> moves = {"bite", "water-pulse", "bubble", "withdraw"};
    //  TM03's count after.
    size_t tm03 = 0;
};

void left_on_poke_balls(FakeTeachGame& game){
    game.pocket = 2;
}
void three_moves(FakeTeachGame& game){
    game.moves = {"bite", "bubble", "withdraw"};
}
void no_tm03(FakeTeachGame& game){
    game.tms = {{"rock-tomb", 1}};
}
void forget_texts_unread(FakeTeachGame& game){
    game.forget_texts_unread = true;
}
void nothing_compatible(FakeTeachGame& game){
    game.compatible.clear();
}
//  Route 4's Mega Kick tutor, the party as console B's (2026-11-03).
void mega_kick_tutor(FakeTeachGame& game){
    game.tutor_move = "mega-kick";
    game.moves = {"bite", "water-pulse", "bubble", "withdraw"};
}
void mega_kick_tutor_unread(FakeTeachGame& game){
    mega_kick_tutor(game);
    game.forget_texts_unread = true;
}
void mega_kick_tutor_room(FakeTeachGame& game){
    mega_kick_tutor(game);
    game.moves = {"bite", "water-pulse", "bubble"};
}
void mega_kick_tutor_incompatible(FakeTeachGame& game){
    mega_kick_tutor(game);
    game.compatible.clear();
}

std::vector<ScreensCase> screens_cases(){
    return {
        {.name = "console B: TM03 over TAIL WHIP"},
        {
            //  Console B, 2026-11-03: known_moves, then "other" until the
            //  step gave up; the move was learned.
            .name = "the texts after the forget read as other",
            .setup = forget_texts_unread,
        },
        {
            .name = "the bag left on POKE BALLS",
            .setup = left_on_poke_balls,
        },
        {
            .name = "a free slot: no move forgotten",
            .setup = three_moves,
            .plan = {.item = "tm03", .move = "water-pulse", .member = 0},
            .moves = {"bite", "bubble", "withdraw", "water-pulse"},
        },
        {
            .name = "no TM03 in the TM CASE: skipped, back to the overworld",
            .setup = no_tm03,
            .end = TeachEnd::NO_ITEM,
            .moves = {"bite", "tail-whip", "bubble", "withdraw"},
        },
        {
            .name = "not compatible: given up, back to the overworld",
            .setup = nothing_compatible,
            .end = TeachEnd::REFUSED,
            .moves = {"bite", "tail-whip", "bubble", "withdraw"},
            .tm03 = 1,
        },
    };
}

//  Route 4's Mega Kick tutor: the TM03 still in the TM CASE after each.
std::vector<ScreensCase> tutor_screens_cases(){
    return {
        {
            .name = "the Mega Kick tutor: over WITHDRAW, back to the overworld past his line",
            .setup = mega_kick_tutor,
            .plan = MEGA_KICK,
            .moves = {"bite", "water-pulse", "bubble", "mega-kick"},
            .tm03 = 1,
        },
        {
            .name = "the Mega Kick tutor, the texts after the forget read as other",
            .setup = mega_kick_tutor_unread,
            .plan = MEGA_KICK,
            .moves = {"bite", "water-pulse", "bubble", "mega-kick"},
            .tm03 = 1,
        },
        {
            .name = "the Mega Kick tutor, a free slot",
            .setup = mega_kick_tutor_room,
            .plan = MEGA_KICK,
            .moves = {"bite", "water-pulse", "bubble", "mega-kick"},
            .tm03 = 1,
        },
        {
            .name = "the Mega Kick tutor, not compatible: given up past his line",
            .setup = mega_kick_tutor_incompatible,
            .plan = MEGA_KICK,
            .end = TeachEnd::REFUSED,
            .moves = {"bite", "water-pulse", "bubble", "withdraw"},
            .tm03 = 1,
        },
    };
}

UnitTestResult check_screens_case(const ScreensCase& item){
    QuietLogger logger;
    FakeTeachGame game;
    if (item.setup){
        item.setup(game);
    }
    FakeTeachScreens screens(game);
    ScreenTeachActions actions(logger, screens);
    const MemberReader reader = [&game](size_t) -> std::optional<MemberInfo>{
        if (game.screen != FakeScreen::OVERWORLD){
            game.error = "read the summary off the overworld";
        }
        return MemberInfo{.species = "wartortle", .level = 22, .moves = game.moves};
    };
    const TeachResult result = run_teach(logger, actions, reader, item.plan);
    const size_t tm03 = game.tms.empty() || game.tms[0].move != "water-pulse" ? 0 : game.tms[0].count;
    std::string presses;
    for (const std::string& press : game.presses){
        presses += press + " ";
    }
    if (!game.error.empty()){
        return std::format("{}: {} (presses: {}).", item.name, game.error, presses);
    }
    if (result.end != item.end || game.moves != item.moves || tm03 != item.tm03){
        return std::format("{}: ended {} ({}), TM03 x{}; presses: {}.",
            item.name, teach_end_name(result.end), result.detail, tm03, presses);
    }
    if (game.screen != FakeScreen::OVERWORLD){
        return std::format("{}: didn't end on the overworld; presses: {}.", item.name, presses);
    }
    //  A debug image for each failure, none otherwise.
    const bool failed = item.end == TeachEnd::FAILED || item.end == TeachEnd::REFUSED;
    if (failed == game.dumps.empty()){
        return std::format("{}: {} debug images.", item.name, game.dumps.size());
    }
    return true;
}

UnitTestResult test_screens(){
    for (const std::vector<ScreensCase>& cases : {screens_cases(), tutor_screens_cases()}){
        for (const ScreensCase& item : cases){
            const UnitTestResult result = check_screens_case(item);
            if (result.result != UnitTestResult::PASSED){
                return result;
            }
        }
    }
    return true;
}


struct AutoStoryTeachScreensInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryTeachScreensExpectation{
};

struct AutoStoryTeachScreensCase{
    AutoStoryTeachScreensInput input;
    AutoStoryTeachScreensExpectation expectation;
};

class Test_AutoStoryTeachScreens : public UnitTest{
public:
    explicit Test_AutoStoryTeachScreens(AutoStoryTeachScreensCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryTeachScreens - " + test_case.input.name)
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
    AutoStoryTeachScreensCase m_case;
};

} // namespace


const AutoStoryTeachScreensCase AUTO_STORY_TEACH_SCREENS_CASES[] = {
    {
        .input = {"TM03 through the console's screens", test_screens},
        .expectation = {},
    },
};

void add_tests_AutoStoryTeachScreens(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_TEACH_SCREENS_CASES){
        database.add<Test_AutoStoryTeachScreens>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

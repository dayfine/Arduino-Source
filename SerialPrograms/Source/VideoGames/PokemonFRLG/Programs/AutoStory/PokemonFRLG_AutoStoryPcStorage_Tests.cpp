/*  AutoStory PC Storage Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The PC step against a fake PC that answers presses as pokefirered's
 *  Pokemon Storage System does (see PokemonFRLG_AutoStoryPcStorage.h):
 *  deposits and withdrawals end with the Pokemon moved and the overworld
 *  back; the refusals; a screen that isn't read fails instead of pressing
 *  on; and RELEASE is never reached.
 *
 */

#include <algorithm>
#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

constexpr size_t BOXES = 14;
constexpr size_t BOX_SLOTS = 30;
constexpr size_t COLUMNS = 6;

//  The PC's menus and box screens, as pokefirered runs them.
class FakePc : public PcStorageActions{
public:
    std::vector<PcPartyMember> party;
    //  Each box's slots: a species, or empty.
    std::vector<std::vector<std::string>> boxes = std::vector<std::vector<std::string>>(BOXES, std::vector<std::string>(BOX_SLOTS));
    //  The box the PC shows on WITHDRAW: the last one viewed.
    size_t current_box = 0;
    //  Screens whose cursor the step can't read.
    std::vector<PcScreen> cursor_unread = {};
    std::vector<std::string> presses;
    //  Set by a press the game would take some other way than the step means.
    std::string error;

    virtual std::vector<PcPartyMember> read_party() override{
        return party;
    }
    virtual PcView read_view() override{
        PcView view{.screen = m_screen};
        if (std::find(cursor_unread.begin(), cursor_unread.end(), m_screen) == cursor_unread.end()
            && m_screen != PcScreen::OVERWORLD && m_screen != PcScreen::TEXT
        ){
            view.cursor = m_cursor;
        }
        if (m_screen == PcScreen::BOX_GRID){
            view.box = current_box;
            view.occupied = !boxes[current_box][m_cursor].empty();
        }
        if (m_screen == PcScreen::BOX_CHOOSER){
            view.box = m_chooser_box;
        }
        return view;
    }
    virtual void press(PcPress press) override{
        presses.push_back(pc_press_name(press));
        switch (m_screen){
        case PcScreen::OVERWORLD:
            if (press == PcPress::A){
                show_text(PcScreen::PC_LIST, 1);
            }
            return;
        case PcScreen::TEXT:
            if ((press == PcPress::A || press == PcPress::B) && --m_texts == 0){
                m_screen = m_after_text;
                m_cursor = 0;
            }
            return;
        case PcScreen::PC_LIST:         press_pc_list(press);       return;
        case PcScreen::STORAGE_MENU:    press_storage_menu(press);  return;
        case PcScreen::PARTY_PANEL:     press_party_panel(press);   return;
        case PcScreen::BOX_GRID:        press_box_grid(press);      return;
        case PcScreen::MON_MENU:        press_mon_menu(press);      return;
        case PcScreen::BOX_CHOOSER:     press_box_chooser(press);   return;
        case PcScreen::YES_NO:          press_yes_no(press);        return;
        case PcScreen::OTHER:           return;
        }
    }

private:
    //  SOMEONE'S PC, the player's PC, PROF. OAK's PC, LOG OFF.
    void press_pc_list(PcPress press){
        if (press == PcPress::B){
            m_screen = PcScreen::OVERWORLD;
        }else if (press == PcPress::A){
            if (m_cursor != 0){
                error = std::format("opened PC list row {}", m_cursor + 1);
            }
            //  The storage menu opens on WITHDRAW from here.
            m_chooser_box = 0;
            show_text(PcScreen::STORAGE_MENU, 2);
        }else{
            m_cursor = move_wrapping(m_cursor, press, 4);
        }
    }
    void press_storage_menu(PcPress press){
        if (press == PcPress::B){
            m_screen = PcScreen::PC_LIST;
            m_cursor = 0;
            return;
        }
        if (press != PcPress::A){
            m_cursor = move_wrapping(m_cursor, press, 5);
            return;
        }
        m_option = m_cursor;
        if (m_cursor > 1){
            error = std::format("chose storage menu row {}", m_cursor + 1);
        }
        m_screen = m_cursor == 0 ? PcScreen::BOX_GRID : PcScreen::PARTY_PANEL;
        m_cursor = 0;
    }
    //  The party's six slots, then CLOSE BOX.
    void press_party_panel(PcPress press){
        if (press == PcPress::B){
            m_back = PcScreen::PARTY_PANEL;
            m_screen = PcScreen::YES_NO;
        }else if (press == PcPress::A && m_cursor == 6){
            error = "pressed CLOSE BOX";
        }else if (press == PcPress::A && m_cursor < party.size()){
            open_mon_menu(PcScreen::PARTY_PANEL);
        }else if (press == PcPress::UP || press == PcPress::DOWN){
            m_cursor = move_wrapping(m_cursor, press, 7);
        }else if (press != PcPress::A){
            error = "LEFT or RIGHT on the party panel";
        }
    }
    void press_box_grid(PcPress press){
        const size_t row = m_cursor / COLUMNS * COLUMNS;
        if (press == PcPress::B){
            m_back = PcScreen::BOX_GRID;
            m_screen = PcScreen::YES_NO;
        }else if (press == PcPress::A && !boxes[current_box][m_cursor].empty()){
            m_grid_cursor = m_cursor;
            open_mon_menu(PcScreen::BOX_GRID);
        }else if (press == PcPress::DOWN){
            m_cursor = (m_cursor + COLUMNS) % BOX_SLOTS;
        }else if (press == PcPress::UP){
            m_cursor = (m_cursor + BOX_SLOTS - COLUMNS) % BOX_SLOTS;
        }else if (press == PcPress::RIGHT){
            m_cursor = row + (m_cursor + 1) % COLUMNS;
        }else if (press == PcPress::LEFT){
            m_cursor = row + (m_cursor + COLUMNS - 1) % COLUMNS;
        }
    }
    void open_mon_menu(PcScreen from){
        m_back = from;
        m_held = m_cursor;
        m_screen = PcScreen::MON_MENU;
        m_cursor = 0;
    }
    //  STORE or WITHDRAW, SUMMARY, MARK, RELEASE, CANCEL.
    void press_mon_menu(PcPress press){
        if (press == PcPress::B){
            m_screen = m_back;
            m_cursor = m_held;
            return;
        }
        if (press != PcPress::A){
            m_cursor = move_wrapping(m_cursor, press, 5);
            return;
        }
        if (m_cursor != 0){
            error = m_cursor == 3 ? "RELEASE" : std::format("chose the Pokemon's menu row {}", m_cursor + 1);
            return;
        }
        if (m_back == PcScreen::PARTY_PANEL){
            m_screen = PcScreen::BOX_CHOOSER;
            return;
        }
        party.push_back({.species = boxes[current_box][m_held]});
        boxes[current_box][m_held].clear();
        m_screen = PcScreen::BOX_GRID;
        m_cursor = m_grid_cursor;
    }
    void press_box_chooser(PcPress press){
        if (press == PcPress::RIGHT || press == PcPress::LEFT){
            m_chooser_box = (m_chooser_box + (press == PcPress::RIGHT ? 1 : BOXES - 1)) % BOXES;
        }else if (press == PcPress::B){
            m_screen = PcScreen::PARTY_PANEL;
            m_cursor = m_held;
        }else if (press == PcPress::A){
            store();
        }
    }
    void store(){
        std::vector<std::string>& box = boxes[m_chooser_box];
        const auto free = std::find(box.begin(), box.end(), std::string());
        if (free == box.end()){
            error = "stored in a full box";
            return;
        }
        *free = party[m_held].species;
        party.erase(party.begin() + ptrdiff_t(m_held));
        m_screen = PcScreen::PARTY_PANEL;
        m_cursor = 0;
    }
    //  "Continue BOX operations?" on YES; B leaves for the storage menu, on
    //  the option used.
    void press_yes_no(PcPress press){
        if (press == PcPress::A){
            m_screen = m_back;
            m_cursor = 0;
        }else if (press == PcPress::B){
            m_screen = PcScreen::STORAGE_MENU;
            m_cursor = m_option;
        }
    }

    void show_text(PcScreen after, size_t pages){
        m_screen = PcScreen::TEXT;
        m_after_text = after;
        m_texts = pages;
    }
    static size_t move_wrapping(size_t cursor, PcPress press, size_t rows){
        if (press == PcPress::DOWN){
            return (cursor + 1) % rows;
        }
        if (press == PcPress::UP){
            return (cursor + rows - 1) % rows;
        }
        return cursor;
    }

    PcScreen m_screen = PcScreen::OVERWORLD;
    size_t m_cursor = 0;
    PcScreen m_after_text = PcScreen::OVERWORLD;
    size_t m_texts = 0;
    size_t m_option = 0;
    size_t m_chooser_box = 0;
    PcScreen m_back = PcScreen::OVERWORLD;
    size_t m_held = 0;
    size_t m_grid_cursor = 0;
};


struct AutoStoryPcStorageInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryPcStorageExpectation{
};

struct AutoStoryPcStorageCase{
    AutoStoryPcStorageInput input;
    AutoStoryPcStorageExpectation expectation;
};

class Test_AutoStoryPcStorage : public UnitTest{
public:
    explicit Test_AutoStoryPcStorage(AutoStoryPcStorageCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryPcStorage - " + test_case.input.name)
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
    AutoStoryPcStorageCase m_case;
};


std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += (text.empty() ? "" : " ") + item;
    }
    return text;
}

std::string party_species(const std::vector<PcPartyMember>& party){
    std::vector<std::string> species;
    for (const PcPartyMember& member : party){
        species.push_back(member.species);
    }
    return joined(species);
}

const std::vector<PcPartyMember> SQUIRTLE_MAGIKARP = {{.species = "squirtle"}, {.species = "magikarp"}};


struct PcCase{
    std::string name = {};
    PcTransfer transfer = {};
    std::vector<PcPartyMember> party = {};
    //  Box 1's first slots, then box 2's.
    std::vector<std::string> box1 = {};
    std::vector<std::string> box2 = {};
    size_t current_box = 0;
    std::vector<PcScreen> cursor_unread = {};
    PcEnd end = PcEnd::DONE;
    std::string party_after = {};
    std::string box1_after = {};
    std::string box2_after = {};
    //  The presses, when the case checks them.
    std::string presses = {};
};
const std::vector<PcCase> PC_CASES = {
    {
        .name = "deposit the Magikarp in box 1 (console B's inputs, 2026-11-02)",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 1, .box = 0},
        .party = SQUIRTLE_MAGIKARP,
        .end = PcEnd::DONE,
        .party_after = "squirtle",
        .box1_after = "magikarp",
        .presses = "A B A B B DOWN A DOWN A A A B B B B",
    },
    {
        .name = "deposit the third Pokemon in box 2, after one already there",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 2, .box = 1},
        .party = {{.species = "squirtle"}, {.species = "magikarp"}, {.species = "rattata"}},
        .box2 = {"pidgey"},
        .end = PcEnd::DONE,
        .party_after = "squirtle magikarp",
        .box2_after = "pidgey rattata",
    },
    {
        .name = "withdraw box 1 slot 8 (second row)",
        .transfer = {.action = PcAction::WITHDRAW, .box = 0, .box_slot = 7},
        .party = {{.species = "squirtle"}},
        .box1 = {"pidgey", "spearow", "ekans", "", "", "", "", "magikarp"},
        .end = PcEnd::DONE,
        .party_after = "squirtle magikarp",
        .box1_after = "pidgey spearow ekans",
    },
    {
        .name = "withdraw an empty slot",
        .transfer = {.action = PcAction::WITHDRAW, .box = 0, .box_slot = 3},
        .party = {{.species = "squirtle"}},
        .box1 = {"magikarp"},
        .end = PcEnd::FAILED,
        .party_after = "squirtle",
        .box1_after = "magikarp",
    },
    {
        .name = "withdraw from a box the PC doesn't show",
        .transfer = {.action = PcAction::WITHDRAW, .box = 1, .box_slot = 0},
        .party = {{.species = "squirtle"}},
        .box2 = {"magikarp"},
        .end = PcEnd::FAILED,
        .party_after = "squirtle",
        .box2_after = "magikarp",
    },
    {
        .name = "refuse to deposit the last Pokemon",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 0},
        .party = {{.species = "squirtle"}},
        .end = PcEnd::REFUSED,
        .party_after = "squirtle",
        .presses = "",
    },
    {
        .name = "refuse to deposit the last Pokemon that can fight",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 1},
        .party = {{.species = "squirtle", .fainted = true}, {.species = "magikarp"}},
        .end = PcEnd::REFUSED,
        .party_after = "squirtle magikarp",
        .presses = "",
    },
    {
        .name = "skip a slot the party hasn't got, when told to",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 1, .box = 0, .if_missing = PcMissing::SKIP},
        .party = {{.species = "squirtle"}},
        .end = PcEnd::SKIPPED,
        .party_after = "squirtle",
        .presses = "",
    },
    {
        .name = "refuse a slot the party hasn't got",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 2},
        .party = SQUIRTLE_MAGIKARP,
        .end = PcEnd::REFUSED,
        .party_after = "squirtle magikarp",
        .presses = "",
    },
    {
        .name = "refuse to withdraw into a full party",
        .transfer = {.action = PcAction::WITHDRAW, .box = 0, .box_slot = 0},
        .party = {{.species = "a"}, {.species = "b"}, {.species = "c"}, {.species = "d"}, {.species = "e"}, {.species = "f"}},
        .box1 = {"magikarp"},
        .end = PcEnd::REFUSED,
        .party_after = "a b c d e f",
        .box1_after = "magikarp",
        .presses = "",
    },
    {
        .name = "a storage menu whose cursor isn't read: stop there",
        .transfer = {.action = PcAction::DEPOSIT, .party_slot = 1},
        .party = SQUIRTLE_MAGIKARP,
        .cursor_unread = {PcScreen::STORAGE_MENU},
        .end = PcEnd::FAILED,
        .party_after = "squirtle magikarp",
        .presses = "A B A B B",
    },
};

//  Each transfer on the fake PC, by its end and what moved.
UnitTestResult test_pc_transfers(){
    for (const PcCase& item : PC_CASES){
        QuietLogger logger;
        FakePc pc;
        pc.party = item.party;
        for (size_t c = 0; c < item.box1.size(); c++){
            pc.boxes[0][c] = item.box1[c];
        }
        for (size_t c = 0; c < item.box2.size(); c++){
            pc.boxes[1][c] = item.box2[c];
        }
        pc.current_box = item.current_box;
        pc.cursor_unread = item.cursor_unread;
        const PcResult result = run_pc_transfer(logger, pc, item.transfer);
        if (!pc.error.empty()){
            return item.name + ": " + pc.error + " after " + joined(pc.presses);
        }
        std::vector<std::string> box1;
        std::vector<std::string> box2;
        for (size_t c = 0; c < BOX_SLOTS; c++){
            if (!pc.boxes[0][c].empty()){
                box1.push_back(pc.boxes[0][c]);
            }
            if (!pc.boxes[1][c].empty()){
                box2.push_back(pc.boxes[1][c]);
            }
        }
        const bool presses_ok = item.presses.empty() && item.end != PcEnd::REFUSED
            ? true
            : joined(pc.presses) == item.presses;
        if (result.end != item.end || party_species(pc.party) != item.party_after
            || joined(box1) != item.box1_after || joined(box2) != item.box2_after || !presses_ok
        ){
            return std::format(
                "{}: expected end {} with party \"{}\", box 1 \"{}\", box 2 \"{}\"; got end {} ({}) with party \"{}\", "
                "box 1 \"{}\", box 2 \"{}\" after \"{}\".",
                item.name, int(item.end), item.party_after, item.box1_after, item.box2_after,
                int(result.end), result.detail, party_species(pc.party), joined(box1), joined(box2), joined(pc.presses)
            );
        }
    }
    return true;
}


//  The dialog alone on screens the fake PC doesn't reach: a storage menu
//  left on another option, a Pokemon's menu not on its top row, nothing
//  recognized.
UnitTestResult test_pc_dialog_screens(){
    struct Case{
        std::string name = {};
        PcTransfer transfer = {};
        std::vector<PcView> views = {};
        //  The last view's decision.
        PcPress press = PcPress::WAIT;
    };
    const PcTransfer deposit{.action = PcAction::DEPOSIT, .party_slot = 1};
    const PcTransfer withdraw{.action = PcAction::WITHDRAW};
    std::vector<PcView> others(PC_MAX_OTHER_IN_A_ROW, PcView{});
    const std::vector<Case> cases = {
        {.name = "DEPOSIT from MOVE ITEMS: up", .transfer = deposit,
            .views = {{.screen = PcScreen::STORAGE_MENU, .cursor = 3}}, .press = PcPress::UP},
        {.name = "DEPOSIT from SEE YA: down, wrapping", .transfer = deposit,
            .views = {{.screen = PcScreen::STORAGE_MENU, .cursor = 4}}, .press = PcPress::DOWN},
        {.name = "WITHDRAW from SEE YA: down, wrapping", .transfer = withdraw,
            .views = {{.screen = PcScreen::STORAGE_MENU, .cursor = 4}}, .press = PcPress::DOWN},
        {.name = "a Pokemon's menu not on its top row: never moved", .transfer = deposit,
            .views = {{.screen = PcScreen::MON_MENU, .cursor = 3}}, .press = PcPress::FAIL},
        {.name = "a Pokemon's menu with its cursor unread: A, the menu opens on its top", .transfer = deposit,
            .views = {{.screen = PcScreen::MON_MENU}}, .press = PcPress::A},
        {.name = "a YES/NO before the transfer", .transfer = deposit,
            .views = {{.screen = PcScreen::YES_NO, .cursor = 0}}, .press = PcPress::FAIL},
        {.name = "nothing recognized for long", .transfer = deposit, .views = others, .press = PcPress::FAIL},
        {.name = "the PC still booting after A: wait", .transfer = deposit,
            .views = {{.screen = PcScreen::OVERWORLD}, {.screen = PcScreen::OVERWORLD}}, .press = PcPress::WAIT},
        {.name = "the overworld again before the transfer", .transfer = deposit,
            .views = {{.screen = PcScreen::OVERWORLD}, {.screen = PcScreen::TEXT}, {.screen = PcScreen::OVERWORLD}}, .press = PcPress::FAIL},
        {.name = "the PC never booting", .transfer = deposit,
            .views = std::vector<PcView>(PC_BOOT_READS + 1, PcView{.screen = PcScreen::OVERWORLD}), .press = PcPress::FAIL},
        {.name = "the box chooser after a move, unread", .transfer = {.action = PcAction::DEPOSIT, .box = 2},
            .views = {{.screen = PcScreen::BOX_CHOOSER}, {.screen = PcScreen::BOX_CHOOSER}}, .press = PcPress::FAIL},
    };
    for (const Case& item : cases){
        PcStorageDialog dialog(item.transfer);
        PcDecision decision;
        for (const PcView& view : item.views){
            decision = dialog.next(view);
        }
        if (decision.press != item.press){
            return std::format("{}: expected {}, got {} ({}).", item.name,
                pc_press_name(item.press), pc_press_name(decision.press), decision.reason);
        }
    }
    return true;
}


//  The step's args, both ways, and the args it refuses.
UnitTestResult test_pc_args(){
    const std::vector<PcTransfer> transfers = {
        {.action = PcAction::DEPOSIT, .party_slot = 1, .box = 0},
        {.action = PcAction::WITHDRAW, .box = 13, .box_slot = 29},
        {.action = PcAction::DEPOSIT, .party_slot = 1, .box = 0, .if_missing = PcMissing::SKIP},
    };
    for (size_t c = 0; c < transfers.size(); c++){
        const PcTransfer& transfer = transfers[c];
        const std::optional<PcTransfer> back = pc_transfer_of(pc_args(transfer));
        if (!back || back->action != transfer.action || back->box != transfer.box
            || back->party_slot != transfer.party_slot || back->box_slot != transfer.box_slot
            || back->if_missing != transfer.if_missing
        ){
            return std::format("Transfer {} didn't read back from its args.", c + 1);
        }
    }
    const auto args = [](std::string action, int64_t slot, int64_t box){
        return Pokemon::StoryActionArgs{{"do", std::move(action)}, {"slot", slot}, {"box", box}};
    };
    const std::vector<Pokemon::StoryActionArgs> bad_args{
        {},
        args("deposit", 7, 1),
        args("deposit", 0, 1),
        args("deposit", 2, 15),
        args("withdraw", 31, 1),
        args("release", 1, 1),
    };
    for (const Pokemon::StoryActionArgs& bad : bad_args){
        if (pc_transfer_of(bad)){
            return "Expected a missing or out-of-range arg refused.";
        }
    }
    return true;
}

} // namespace


const AutoStoryPcStorageCase AUTO_STORY_PC_STORAGE_CASES[] = {
    {
        .input = {"deposits, withdrawals and refusals on a fake PC", test_pc_transfers},
        .expectation = {},
    },
    {
        .input = {"the dialog on other screens", test_pc_dialog_screens},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_pc_args},
        .expectation = {},
    },
};

void add_tests_AutoStoryPcStorage(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_PC_STORAGE_CASES){
        database.add<Test_AutoStoryPcStorage>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

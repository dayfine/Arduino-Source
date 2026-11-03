/*  AutoStory PC Storage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  pokefirered: PARTY_SIZE, TOTAL_BOXES_COUNT, IN_BOX_COUNT (6 columns).
constexpr size_t PARTY_SIZE = 6;
constexpr size_t BOX_COUNT = 14;
constexpr size_t BOX_SLOTS = 30;
constexpr size_t BOX_COLUMNS = 6;
//  The storage menu's options: WITHDRAW, DEPOSIT, MOVE POKEMON, MOVE ITEMS, SEE YA!.
constexpr size_t STORAGE_MENU_ROWS = 5;
//  A second look at a frame that reads as the overworld.
constexpr Milliseconds OVERWORLD_CONFIRM = 800ms;

std::vector<std::string_view> split_words(std::string_view text){
    std::vector<std::string_view> words;
    while (!text.empty()){
        const size_t space = text.find(' ');
        const std::string_view word = text.substr(0, space);
        if (!word.empty()){
            words.push_back(word);
        }
        if (space == std::string_view::npos){
            break;
        }
        text.remove_prefix(space + 1);
    }
    return words;
}

//  "key=N" with N from 1 to `max`, as N - 1.
std::optional<size_t> counted_value(std::string_view word, std::string_view key, size_t max){
    if (!word.starts_with(key) || word.size() <= key.size() + 1 || word[key.size()] != '='){
        return std::nullopt;
    }
    size_t value = 0;
    for (char c : word.substr(key.size() + 1)){
        if (c < '0' || c > '9' || value > max){
            return std::nullopt;
        }
        value = value * 10 + size_t(c - '0');
    }
    if (value < 1 || value > max){
        return std::nullopt;
    }
    return value - 1;
}

}


std::string pc_variant(const PcTransfer& transfer){
    if (transfer.action == PcAction::DEPOSIT){
        return std::format("{}deposit slot={} box={}", PC_VARIANT_PREFIX, transfer.party_slot + 1, transfer.box + 1);
    }
    return std::format("{}withdraw box={} slot={}", PC_VARIANT_PREFIX, transfer.box + 1, transfer.box_slot + 1);
}

std::optional<PcTransfer> parse_pc_variant(std::string_view variant){
    if (!variant.starts_with(PC_VARIANT_PREFIX)){
        return std::nullopt;
    }
    const std::vector<std::string_view> words = split_words(variant.substr(PC_VARIANT_PREFIX.size()));
    if (words.size() != 3){
        return std::nullopt;
    }
    PcTransfer transfer;
    std::optional<size_t> slot;
    std::optional<size_t> box;
    if (words[0] == "deposit"){
        transfer.action = PcAction::DEPOSIT;
        slot = counted_value(words[1], "slot", PARTY_SIZE);
        box = counted_value(words[2], "box", BOX_COUNT);
    }else if (words[0] == "withdraw"){
        transfer.action = PcAction::WITHDRAW;
        box = counted_value(words[1], "box", BOX_COUNT);
        slot = counted_value(words[2], "slot", BOX_SLOTS);
    }
    if (!slot || !box){
        return std::nullopt;
    }
    transfer.box = *box;
    if (transfer.action == PcAction::DEPOSIT){
        transfer.party_slot = *slot;
    }else{
        transfer.box_slot = *slot;
    }
    return transfer;
}


std::string pc_transfer_error(const PcTransfer& transfer, const std::vector<PcPartyMember>& party){
    if (transfer.action == PcAction::WITHDRAW){
        return party.size() >= PARTY_SIZE ? "the party is full" : "";
    }
    if (transfer.party_slot >= party.size()){
        return std::format("the party has no slot {} ({} Pokemon)", transfer.party_slot + 1, party.size());
    }
    if (party.size() == 1){
        return "it is the party's last Pokemon";
    }
    for (size_t c = 0; c < party.size(); c++){
        if (c != transfer.party_slot && !party[c].fainted){
            return "";
        }
    }
    return std::format("slot {} is the party's last Pokemon that can fight", transfer.party_slot + 1);
}


const char* pc_press_name(PcPress press){
    switch (press){
    case PcPress::A:        return "A";
    case PcPress::B:        return "B";
    case PcPress::UP:       return "UP";
    case PcPress::DOWN:     return "DOWN";
    case PcPress::LEFT:     return "LEFT";
    case PcPress::RIGHT:    return "RIGHT";
    case PcPress::WAIT:     return "wait";
    case PcPress::DONE:     return "done";
    case PcPress::FAIL:     return "fail";
    }
    return "?";
}


PcStorageDialog::PcStorageDialog(const PcTransfer& transfer)
    : m_transfer(transfer)
{}

PcDecision PcStorageDialog::next(const PcView& view){
    if (view.screen != PcScreen::OTHER){
        m_others_in_a_row = 0;
    }
    switch (view.screen){
    case PcScreen::OVERWORLD:
        return on_overworld();
    case PcScreen::TEXT:
        m_booting_reads = PC_BOOT_READS;
        return {PcPress::B};
    case PcScreen::PC_LIST:
        //  The list opens on its top row each time.
        if (m_transferred){
            return {PcPress::B};
        }
        return {view.cursor && *view.cursor > 0 ? PcPress::UP : PcPress::A};
    case PcScreen::STORAGE_MENU:
        return on_storage_menu(view);
    case PcScreen::PARTY_PANEL:
        return on_party_panel(view);
    case PcScreen::BOX_GRID:
        return on_box_grid(view);
    case PcScreen::MON_MENU:
        return on_mon_menu(view);
    case PcScreen::BOX_CHOOSER:
        return on_box_chooser(view);
    case PcScreen::YES_NO:
        //  "Continue BOX operations?": B leaves the box.
        if (m_transferred){
            return {PcPress::B};
        }
        return {PcPress::FAIL, "a YES/NO before the transfer"};
    case PcScreen::OTHER:
        if (++m_others_in_a_row >= PC_MAX_OTHER_IN_A_ROW){
            return {PcPress::FAIL, std::format("no PC screen recognized in {} reads", m_others_in_a_row)};
        }
        return {PcPress::WAIT};
    }
    return {PcPress::FAIL, "unknown screen"};
}

PcDecision PcStorageDialog::on_overworld(){
    if (m_transferred){
        return {PcPress::DONE};
    }
    if (!m_booted){
        m_booted = true;
        return {PcPress::A};
    }
    //  Still the overworld while the PC boots, or again after a step.
    if (++m_booting_reads < PC_BOOT_READS){
        return {PcPress::WAIT};
    }
    return {PcPress::FAIL, "the overworld came back before the transfer"};
}

PcDecision PcStorageDialog::on_storage_menu(const PcView& view){
    if (m_transferred){
        return {PcPress::B};
    }
    //  It opens on the last option used when it comes back from a box, so
    //  the cursor must be read.
    if (!view.cursor || *view.cursor >= STORAGE_MENU_ROWS){
        return {PcPress::FAIL, "the storage menu's cursor wasn't read"};
    }
    const size_t target = m_transfer.action == PcAction::WITHDRAW ? 0 : 1;
    if (*view.cursor == target){
        return {PcPress::A};
    }
    //  It wraps: the shorter way round.
    const size_t down = (target + STORAGE_MENU_ROWS - *view.cursor) % STORAGE_MENU_ROWS;
    return {down <= STORAGE_MENU_ROWS / 2 ? PcPress::DOWN : PcPress::UP};
}

PcDecision PcStorageDialog::on_party_panel(const PcView& view){
    if (m_transferred){
        return {PcPress::B};
    }
    if (m_transfer.action != PcAction::DEPOSIT){
        return {PcPress::FAIL, "the party panel opened for a withdrawal"};
    }
    if (!view.cursor){
        return {PcPress::FAIL, "the hand wasn't read on the party panel"};
    }
    if (*view.cursor < m_transfer.party_slot){
        return {PcPress::DOWN};
    }
    if (*view.cursor > m_transfer.party_slot){
        return {PcPress::UP};
    }
    return {PcPress::A};
}

PcDecision PcStorageDialog::on_box_grid(const PcView& view){
    if (m_transferred){
        return {PcPress::B};
    }
    if (m_transfer.action != PcAction::WITHDRAW){
        return {PcPress::FAIL, "the box grid opened for a deposit"};
    }
    if (view.box && *view.box != m_transfer.box){
        return {PcPress::FAIL, std::format("the PC shows box {}, not box {}", *view.box + 1, m_transfer.box + 1)};
    }
    if (!view.cursor || *view.cursor >= BOX_SLOTS){
        return {PcPress::FAIL, "the hand wasn't read on the box"};
    }
    const size_t row = *view.cursor / BOX_COLUMNS;
    const size_t column = *view.cursor % BOX_COLUMNS;
    const size_t target_row = m_transfer.box_slot / BOX_COLUMNS;
    const size_t target_column = m_transfer.box_slot % BOX_COLUMNS;
    if (row != target_row){
        return {row < target_row ? PcPress::DOWN : PcPress::UP};
    }
    if (column != target_column){
        return {column < target_column ? PcPress::RIGHT : PcPress::LEFT};
    }
    if (view.occupied == false){
        return {PcPress::FAIL, std::format("box {} slot {} is empty", m_transfer.box + 1, m_transfer.box_slot + 1)};
    }
    return {PcPress::A};
}

PcDecision PcStorageDialog::on_mon_menu(const PcView& view){
    //  RELEASE is on this menu: the cursor is never moved on it.
    if (m_transferred){
        return {PcPress::B};
    }
    if (view.cursor && *view.cursor != 0){
        return {PcPress::FAIL, std::format("the Pokemon's menu is on row {}, not its top", *view.cursor + 1)};
    }
    if (m_transfer.action == PcAction::WITHDRAW){
        m_transferred = true;
    }
    return {PcPress::A};
}

PcDecision PcStorageDialog::on_box_chooser(const PcView& view){
    if (m_transferred){
        return {PcPress::B};
    }
    if (m_transfer.action != PcAction::DEPOSIT){
        return {PcPress::FAIL, "the box chooser opened for a withdrawal"};
    }
    std::optional<size_t> box = view.box;
    //  It opens on BOX 1 each time the PC is entered.
    if (!box && !m_chooser_seen){
        box = 0;
    }
    if (!box){
        return {PcPress::FAIL, "the box chooser's box wasn't read"};
    }
    m_chooser_seen = true;
    if (*box < m_transfer.box){
        return {PcPress::RIGHT};
    }
    if (*box > m_transfer.box){
        return {PcPress::LEFT};
    }
    m_transferred = true;
    return {PcPress::A};
}


PcResult run_pc_transfer(Logger& logger, PcStorageActions& actions, const PcTransfer& transfer){
    const std::string name = pc_variant(transfer);
    const std::string refused = pc_transfer_error(transfer, actions.read_party());
    if (!refused.empty()){
        logger.log("PC: refused \"" + name + "\": " + refused + ".", COLOR_RED);
        return {PcEnd::REFUSED, refused};
    }
    PcStorageDialog dialog(transfer);
    for (size_t c = 0; c < PC_MAX_PRESSES; c++){
        const PcView view = actions.read_view();
        const PcDecision decision = dialog.next(view);
        if (decision.press == PcPress::DONE){
            logger.log("PC: \"" + name + "\" done.");
            return {PcEnd::DONE, ""};
        }
        if (decision.press == PcPress::FAIL){
            logger.log("PC: \"" + name + "\" failed on the " + pc_screen_name(view.screen) + ": " + decision.reason + ".", COLOR_RED);
            return {PcEnd::FAILED, decision.reason};
        }
        logger.log(std::format(
            "PC: {} (cursor {}): {}.",
            pc_screen_name(view.screen),
            view.cursor ? std::to_string(*view.cursor) : "-",
            pc_press_name(decision.press)
        ));
        actions.press(decision.press);
    }
    return {PcEnd::FAILED, std::format("not back on the overworld after {} presses", PC_MAX_PRESSES)};
}


size_t read_party_size(ConsoleHandle& console, ProControllerContext& context){
    open_party_menu_from_overworld(console, context);
    context.wait_for_all_requests();
    const size_t size = size_t(detect_last_occupied_party_slot(console)) + 1;
    mash_until_overworld(console, context, 15000ms);
    return size;
}


ConsolePcStorageActions::ConsolePcStorageActions(ConsoleHandle& console, ProControllerContext& context)
    : m_console(console)
    , m_context(context)
{}

std::vector<PcPartyMember> ConsolePcStorageActions::read_party(){
    return std::vector<PcPartyMember>(read_party_size(m_console, m_context));
}

PcView ConsolePcStorageActions::read_view(){
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_console.video().snapshot();
    if (!screen){
        return {};
    }
    const PcView view = read_pc_screen(m_console, screen);
    if (view.screen != PcScreen::OVERWORLD){
        return view;
    }
    pbf_wait(m_context, OVERWORLD_CONFIRM);
    m_context.wait_for_all_requests();
    VideoSnapshot again = m_console.video().snapshot();
    return again ? read_pc_screen(m_console, again) : PcView{};
}

void ConsolePcStorageActions::press(PcPress press){
    switch (press){
    case PcPress::A:
        pbf_press_button(m_context, BUTTON_A, 160ms, 640ms);
        break;
    case PcPress::B:
        pbf_press_button(m_context, BUTTON_B, 160ms, 640ms);
        break;
    case PcPress::UP:
        pbf_press_dpad(m_context, DPAD_UP, 160ms, 320ms);
        break;
    case PcPress::DOWN:
        pbf_press_dpad(m_context, DPAD_DOWN, 160ms, 320ms);
        break;
    case PcPress::LEFT:
        pbf_press_dpad(m_context, DPAD_LEFT, 160ms, 320ms);
        break;
    case PcPress::RIGHT:
        pbf_press_dpad(m_context, DPAD_RIGHT, 160ms, 320ms);
        break;
    case PcPress::WAIT:
    case PcPress::DONE:
    case PcPress::FAIL:
        pbf_wait(m_context, 500ms);
        break;
    }
    m_context.wait_for_all_requests();
}



}
}
}

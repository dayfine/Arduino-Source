/*  AutoStory Summary
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_PartyDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStorySummary.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

void press(ProControllerContext& context, Button button){
    pbf_press_button(context, button, 160ms, 0ms);
}

void tap(ProControllerContext& context, DpadPosition position, Milliseconds after = 400ms){
    pbf_press_dpad(context, position, 160ms, after);
}

//  Up to `timeout` for `watcher`'s screen, after a press. A screen that
//  shows is given a moment to finish drawing before it is read.
bool wait_drawn(ConsoleHandle& console, ProControllerContext& context, VisualInferenceCallback& watcher, Milliseconds timeout){
    context.wait_for_all_requests();
    if (wait_until(console, context, timeout, {watcher}) < 0){
        return false;
    }
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
    return true;
}

//  POKEMON SKILLS' stats, none unless all six read.
std::optional<Stats> skills_stats(ConsoleHandle& console, Language language, const ImageViewRGB32& screen){
    PokemonFRLG_Stats read;
    StatsReader().read_page2(console, language, screen, read);
    if (!read.hp || !read.attack || !read.defense || !read.sp_attack || !read.sp_defense || !read.speed){
        return std::nullopt;
    }
    return Stats{
        .hp = static_cast<uint16_t>(*read.hp), .attack = static_cast<uint16_t>(*read.attack),
        .defense = static_cast<uint16_t>(*read.defense), .sp_attack = static_cast<uint16_t>(*read.sp_attack),
        .sp_defense = static_cast<uint16_t>(*read.sp_defense), .speed = static_cast<uint16_t>(*read.speed),
    };
}

//  From the party menu with the cursor on the member: its summary's first
//  page shown.
bool open_summary(ConsoleHandle& console, ProControllerContext& context){
    PartySelectionWatcher submenu(COLOR_RED);
    press(context, BUTTON_A);
    if (!wait_drawn(console, context, submenu, 5000ms)){
        console.log("Summary: the party member's menu didn't show.", COLOR_ORANGE);
        return false;
    }
    //  SUMMARY, the menu's top row.
    SummaryWatcher info(COLOR_RED);
    press(context, BUTTON_A);
    if (!wait_drawn(console, context, info, 5000ms)){
        console.log("Summary: the summary's first page didn't show.", COLOR_ORANGE);
        return false;
    }
    return true;
}

//  From the party menu with the cursor on the member: its summary, read.
std::optional<SummaryRead> read_open_summary(ConsoleHandle& console, ProControllerContext& context, Language language){
    if (!open_summary(console, context)){
        return std::nullopt;
    }
    SummaryRead read;
    {
        VideoSnapshot screen = console.video().snapshot();
        if (!screen){
            return std::nullopt;
        }
        PokemonFRLG_Stats stats;
        StatsReader().read_page1(console, language, screen, stats);
        read.species = stats.name;
        read.level = stats.level;
    }
    SummaryPage2Watcher skills(COLOR_RED);
    tap(context, DPAD_RIGHT, 0ms);
    if (!wait_drawn(console, context, skills, 3000ms)){
        console.log("Summary: POKEMON SKILLS didn't show.", COLOR_ORANGE);
        return std::nullopt;
    }
    if (VideoSnapshot page = console.video().snapshot()){
        read.stats = skills_stats(console, language, page);
    }
    SummaryPage3Watcher moves(COLOR_RED);
    tap(context, DPAD_RIGHT, 0ms);
    if (!wait_drawn(console, context, moves, 3000ms)){
        console.log("Summary: KNOWN MOVES didn't show.", COLOR_ORANGE);
        return std::nullopt;
    }
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    read.moves = read_summary_moves(console, language, screen);
    dump_debug_image(console, "AutoStory/summary", "moves", screen);
    return read;
}


//  The member switch (DOWN on the summary's first page): the sprite and
//  text are redrawn over a few frames.
constexpr Milliseconds MEMBER_SWITCH_SETTLE = 900ms;
constexpr size_t SPECIES_READ_TRIES = 3;
//  B on a member's menu: it is gone the next frames; checked for 3 s.
constexpr Milliseconds MENU_CLOSE_CHECK = 100ms;
constexpr size_t MENU_CLOSE_CHECKS = 30;

class ConsolePartyScreens : public PartyScreens{
public:
    ConsolePartyScreens(ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language)
        : m_console(console)
        , m_context(context)
        , m_menu(menu)
        , m_language(language)
    {}

    virtual bool open_party(size_t& members) override{
        if (m_menu == StartMenu::BEFORE_POKEMON){
            return false;
        }
        open_party_menu_from_overworld(m_console, m_context, start_menu_context(m_menu));
        m_context.wait_for_all_requests();
        //  The cursor on the lead, as read_party_summary() does.
        tap(m_context, DPAD_LEFT);
        m_context.wait_for_all_requests();
        members = size_t(detect_last_occupied_party_slot(m_console)) + 1;
        m_members = members;
        m_cursor = 0;
        return true;
    }

    virtual bool enter_summary() override{
        return open_summary(m_console, m_context);
    }

    virtual std::string read_species() override{
        for (size_t c = 0; c < SPECIES_READ_TRIES; c++){
            if (VideoSnapshot screen = m_console.video().snapshot()){
                PokemonFRLG_Stats stats;
                StatsReader().read_page1(m_console, m_language, screen, stats);
                if (!stats.name.empty()){
                    return stats.name;
                }
            }
            pbf_wait(m_context, 400ms);
            m_context.wait_for_all_requests();
        }
        return "";
    }

    virtual bool next_member() override{
        //  DOWN on the last member does nothing.
        if (m_cursor + 1 < m_members){
            m_cursor++;
        }
        tap(m_context, DPAD_DOWN, 0ms);
        pbf_wait(m_context, MEMBER_SWITCH_SETTLE);
        m_context.wait_for_all_requests();
        return true;
    }

    virtual bool leave_summary() override{
        PartySelectionWatcher menu(COLOR_RED);
        //  The party menu comes back with the member's menu open
        //  (pokefirered party_menu.c, CB2_ReturnToPartyMenuFromSummaryScreen()).
        press(m_context, BUTTON_B);
        return wait_drawn(m_console, m_context, menu, 5000ms);
    }

    virtual bool open_member_menu(size_t slot) override{
        //  The party menu's UP and DOWN step one member, through CANCEL
        //  past the ends; the scans never go past them.
        for (; m_cursor < slot; m_cursor++){
            tap(m_context, DPAD_DOWN);
        }
        for (; m_cursor > slot; m_cursor--){
            tap(m_context, DPAD_UP);
        }
        return open_menu_on_cursor();
    }

    virtual std::optional<PartyMemberMenu> read_member_menu() override{
        VideoSnapshot screen = m_console.video().snapshot();
        if (!screen){
            return std::nullopt;
        }
        std::optional<PartyMemberMenu> menu = read_party_member_menu(m_console.logger(), m_language, screen);
        if (!menu || !menu->all_read()){
            m_console.log(std::format("Party: slot {}'s menu isn't read.", m_cursor + 1), COLOR_RED);
            dump_debug_image(m_console, "AutoStory/party", "member_menu", screen);
        }
        return menu;
    }

    virtual bool close_member_menu() override{
        press(m_context, BUTTON_B);
        m_context.wait_for_all_requests();
        PartySelectionDetector menu(COLOR_RED);
        for (size_t c = 0; c < MENU_CLOSE_CHECKS; c++){
            pbf_wait(m_context, MENU_CLOSE_CHECK);
            m_context.wait_for_all_requests();
            VideoSnapshot screen = m_console.video().snapshot();
            if (screen && !menu.detect(screen)){
                return true;
            }
        }
        return false;
    }

    virtual void close() override{
        mash_until_overworld(m_console, m_context, 20000ms);
    }

private:
    bool open_menu_on_cursor(){
        PartySelectionWatcher menu(COLOR_RED);
        press(m_context, BUTTON_A);
        return wait_drawn(m_console, m_context, menu, 5000ms);
    }

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    StartMenu m_menu;
    Language m_language;
    size_t m_members = 0;
    //  The member the party menu's cursor is on.
    size_t m_cursor = 0;
};

} // namespace

std::unique_ptr<PartyScreens> console_party_screens(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language
){
    return std::make_unique<ConsolePartyScreens>(console, context, menu, language);
}

SpeciesFinder console_species_finder(
    ConsoleHandle& console, ProControllerContext& context, Language language, PartyCache& party
){
    return [&console, &context, language, &party](std::string_view species){
        const std::unique_ptr<PartyScreens> screens = console_party_screens(console, context, StartMenu::FULL, language);
        return open_member_menu_of(console.logger(), *screens, party, species, false);
    };
}

std::optional<SummaryRead> read_party_summary(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language, size_t slot
){
    if (menu == StartMenu::BEFORE_POKEMON){
        return std::nullopt;
    }
    open_party_menu_from_overworld(console, context, start_menu_context(menu));
    context.wait_for_all_requests();
    //  LEFT puts the cursor on the lead from any other slot (pokefirered
    //  party_menu.c, MENU_DIR_LEFT); on the lead it does nothing. The menu
    //  doesn't always open on the lead: 10.01's teach_fly read slot 1 as the
    //  Pidgey, the party's third (console B, 2026-11-12 01:38). DOWN steps
    //  through the slots from there.
    tap(context, DPAD_LEFT);
    for (size_t c = 0; c < slot; c++){
        tap(context, DPAD_DOWN);
    }
    const std::optional<SummaryRead> read = read_open_summary(console, context, language);
    mash_until_overworld(console, context, 20000ms);
    if (read){
        std::string moves;
        for (const SummaryMove& move : read->moves){
            moves += std::format(
                "{}{} {}/{}", moves.empty() ? "" : ", ", move.move.empty() ? "?" : move.move, move.pp, move.max_pp
            );
        }
        const std::string stats = read->stats
            ? std::format(
                "{} HP, {}/{}/{}/{}/{}", read->stats->hp, read->stats->attack, read->stats->defense,
                read->stats->sp_attack, read->stats->sp_defense, read->stats->speed
            )
            : "stats not read";
        console.log(std::format(
            "Summary of party slot {}: {} Lv{}, {}, {}.",
            slot + 1, read->species.empty() ? "?" : read->species,
            read->level ? std::to_string(*read->level) : "?", stats, moves.empty() ? "no moves read" : moves
        ));
    }
    return read;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

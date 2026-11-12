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

//  From the party menu with the cursor on the member: its summary, read.
std::optional<SummaryRead> read_open_summary(ConsoleHandle& console, ProControllerContext& context, Language language){
    PartySelectionWatcher submenu(COLOR_RED);
    press(context, BUTTON_A);
    if (!wait_drawn(console, context, submenu, 5000ms)){
        console.log("Summary: the party member's menu didn't show.", COLOR_ORANGE);
        return std::nullopt;
    }
    //  SUMMARY, the menu's top row.
    SummaryWatcher info(COLOR_RED);
    press(context, BUTTON_A);
    if (!wait_drawn(console, context, info, 5000ms)){
        console.log("Summary: the summary's first page didn't show.", COLOR_ORANGE);
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

} // namespace

std::optional<SummaryRead> read_party_summary(
    ConsoleHandle& console, ProControllerContext& context, StartMenu menu, Language language, size_t slot
){
    if (menu == StartMenu::BEFORE_POKEMON){
        return std::nullopt;
    }
    open_party_menu_from_overworld(
        console, context, menu == StartMenu::BEFORE_POKEDEX ? StartMenuContext::NO_DEX : StartMenuContext::STANDARD
    );
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

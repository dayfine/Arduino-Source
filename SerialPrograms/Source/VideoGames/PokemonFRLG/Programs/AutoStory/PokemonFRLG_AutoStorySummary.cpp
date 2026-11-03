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

}

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
    //  The party menu opens on the lead; DOWN steps through the slots.
    for (size_t c = 0; c < slot; c++){
        tap(context, DPAD_DOWN);
    }
    const std::optional<SummaryRead> read = read_open_summary(console, context, language);
    mash_until_overworld(console, context, 20000ms);
    if (read){
        std::string moves;
        for (const SummaryMove& move : read->moves){
            moves += std::format("{}{} {}/{}", moves.empty() ? "" : ", ", move.move, move.pp, move.max_pp);
        }
        console.log(std::format(
            "Summary of party slot {}: {} Lv{}, {}.",
            slot + 1, read->species.empty() ? "?" : read->species,
            read->level ? std::to_string(*read->level) : "?", moves.empty() ? "no moves read" : moves
        ));
    }
    return read;
}



}
}
}

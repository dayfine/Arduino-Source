/*  AutoStory Battle Switch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "PokemonFRLG_AutoStoryBattleSwitch.h"
#include "PokemonFRLG_AutoStoryLeadOut.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

void tap(ProControllerContext& context, DpadPosition position){
    pbf_press_dpad(context, position, /*hold=*/160ms, /*release=*/240ms);
}

//  POKEMON, bottom left of the battle menu, and time for the party list to show.
void open_party_list(ProControllerContext& context){
    tap(context, DPAD_RIGHT);
    tap(context, DPAD_UP);
    tap(context, DPAD_LEFT);
    tap(context, DPAD_DOWN);
    pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/1500ms);
}

} // namespace


void send_member(ProControllerContext& context, size_t slot){
    for (size_t c = 0; c < slot; c++){
        pbf_press_dpad(context, DPAD_DOWN, /*hold=*/160ms, /*release=*/400ms);
    }
    pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/800ms);
    pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/1500ms);
}

bool switch_lead_in(ConsoleHandle& console, ProControllerContext& context, Language language, std::string_view lead){
    open_party_list(context);
    VideoSnapshot screen = console.video().snapshot();
    const std::optional<size_t> slot = screen && PartyMenuDetector(COLOR_RED).detect(screen)
        ? party_slot_of(console.logger(), language, screen, lead)
        : std::nullopt;
    if (!slot || *slot == 0){
        console.log("Battle: the lead isn't in the party list off the field; fighting on.", COLOR_RED);
        pbf_press_button(context, BUTTON_B, /*hold=*/160ms, /*release=*/1000ms);
        return false;
    }
    console.log(std::format("Battle: the lead back in from party list slot {}.", *slot + 1), COLOR_ORANGE);
    send_member(context, *slot);
    return true;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

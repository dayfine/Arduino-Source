/*  Trainer Card
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_TrainerCardDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_TrainerIdReader.h"
#include "PokemonFRLG_StartMenuNavigation.h"
#include "PokemonFRLG_TrainerCard.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void navigate_to_trainer_card(ConsoleHandle& console, ProControllerContext& context){
    int errors = 0;
    while (true){
        if (errors >= 5){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT, "navigate_to_trainer_card(): failed 5 times to navigate to the Trainer Card", console
            ); 
        }

        open_start_menu(console, context);

        // the positions are different before recieving a Pokemon and Pokedex
        bool cursor = move_cursor_to_position(
            console, context, SelectionArrowPositionStartMenu(1)
        );
        if (!cursor){
            errors++;
            pbf_mash_button(context, BUTTON_B, 2000ms);
            continue;
        }

        TrainerCardWatcher trainer_card(COLOR_RED);
        context.wait_for_all_requests();
        int ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& context){
                pbf_press_button(context, BUTTON_A, 200ms, 2300ms);
                pbf_press_button(context, BUTTON_A, 200ms, 2300ms);
            },
            { trainer_card }
        );

        if (ret < 0){
            console.log("navigate_to_trainer_card(): failed to detect Trainer Card");
            errors++;
            pbf_mash_button(context, BUTTON_B, 2000ms);
            continue;
        }

        break;
    }
}

namespace{

template <typename Read>
auto read_on_card(ConsoleHandle& console, ProControllerContext& context, Read read){
    TrainerIdReader reader;
    VideoOverlaySet overlays(console.overlay());
    reader.make_overlays(overlays);

    VideoSnapshot screen = console.video().snapshot();
    console.log("Trainer Card detected.");
    console.log("Reading TID...");
    auto tid = read(reader, screen);

    context.wait_for_all_requests();

    return tid;
}

} // namespace

uint16_t read_trainer_card_tid(ConsoleHandle& console, ProControllerContext& context, Language language){
    uint16_t tid = read_on_card(console, context, [&](TrainerIdReader& reader, const VideoSnapshot& screen){
        return reader.read_tid(console.logger(), language, screen);
    });
    console.log("TID: " + std::to_string(tid));
    return tid;
}

std::optional<uint16_t> try_read_trainer_card_tid(ConsoleHandle& console, ProControllerContext& context, Language language){
    std::optional<uint16_t> tid = read_on_card(console, context, [&](TrainerIdReader& reader, const VideoSnapshot& screen){
        return reader.try_read_tid(console.logger(), language, screen);
    });
    console.log(tid ? "TID: " + std::to_string(*tid) : std::string("TID: not read"));
    return tid;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

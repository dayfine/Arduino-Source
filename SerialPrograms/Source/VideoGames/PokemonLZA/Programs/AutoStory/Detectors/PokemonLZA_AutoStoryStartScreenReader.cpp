/*  PokemonLZA AutoStory - Start screen reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryMinimapMatch.h"
#include "PokemonLZA_AutoStoryStartScreenReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


StartSignature read_start_screen(const ImageViewRGB32& screen){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(screen);
    const bool unreadable = events.has(ScreenEvent::FADE_BLACK) || events.has(ScreenEvent::FADE_WHITE);

    if (unreadable){
        return StartSignature{};
    }
    PartyBarDetector party_bar;
    std::optional<size_t> party;
    if (party_bar.detect(screen) && party_bar.filled_slots() > 0){
        party = (size_t)party_bar.filled_slots();
    }
    return StartSignature{
        .talk_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .party = party,
        .minimap = matching_minimap(screen),
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

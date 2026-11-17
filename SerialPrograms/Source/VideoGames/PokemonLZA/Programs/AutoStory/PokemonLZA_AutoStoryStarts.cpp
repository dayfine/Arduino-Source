/*  PokemonLZA AutoStory - Checkpoint starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryBattlePanelDetector.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;


std::string to_string(const StartSignature& signature){
    std::vector<std::string> parts;
    if (signature.registered){
        parts.push_back(std::format("registered {}", *signature.registered));
    }
    if (signature.talk_prompt){
        parts.push_back(*signature.talk_prompt ? "talk prompt" : "no talk prompt");
    }
    if (signature.wild_zone){
        parts.push_back(*signature.wild_zone ? "in a wild zone" : "not in a wild zone");
    }
    if (signature.party){
        parts.push_back(std::format("party {}", *signature.party));
    }
    if (parts.empty()){
        return "nothing read";
    }
    std::string out = parts[0];
    for (size_t index = 1; index < parts.size(); index++){
        out += std::format(", {}", parts[index]);
    }
    return out;
}

const CheckpointStart* find_start(std::span<const CheckpointStart> starts, std::string_view id){
    for (const CheckpointStart& start : starts){
        if (start.checkpoint == id){
            return &start;
        }
    }
    return nullptr;
}


namespace{

//  How a known field and a read one compare: none when one of them is
//  missing.
template <typename Type>
std::optional<bool> field_agrees(const std::optional<Type>& known, const std::optional<Type>& read){
    if (!known || !read){
        return std::nullopt;
    }
    return *known == *read;
}

//  Whether `read` matches the start `known`: they share a field and agree
//  on every field they share.
bool start_matches(const StartSignature& known, const StartSignature& read){
    const std::optional<bool> fields[] = {
        field_agrees(known.registered, read.registered),
        field_agrees(known.talk_prompt, read.talk_prompt),
        field_agrees(known.wild_zone, read.wild_zone),
        field_agrees(known.party, read.party),
    };
    bool shared = false;
    for (const std::optional<bool>& agrees : fields){
        if (agrees && !*agrees){
            return false;
        }
        shared = shared || agrees.has_value();
    }
    return shared;
}

//  Whether `starts` names row `row`'s start and `read` matches it.
bool row_matches(const StoryCheckpoint& row, std::span<const CheckpointStart> starts, const StartSignature& read){
    const CheckpointStart* start = find_start(starts, row.id);
    return start != nullptr && start_matches(start->signature, read);
}

} // namespace


std::optional<size_t> checkpoint_at(
    std::span<const StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
){
    if (asked >= rows.size()){
        return std::nullopt;
    }
    if (row_matches(rows[asked], starts, read)){
        return asked;
    }
    std::optional<size_t> found;
    for (size_t index = 0; index < asked; index++){
        if (!row_matches(rows[index], starts, read)){
            continue;
        }
        if (found){
            return std::nullopt;
        }
        found = index;
    }
    return found;
}

std::optional<size_t> run_start(
    std::span<const StoryCheckpoint> rows, std::span<const CheckpointStart> starts,
    size_t asked, const StartSignature& read
){
    if (asked < rows.size() && find_start(starts, rows[asked].id) == nullptr){
        return asked;
    }
    return checkpoint_at(rows, starts, asked, read);
}

StartSignature read_start_screen(const ImageViewRGB32& screen){
    PlzaScreenObserver observer;
    const ScreenEvents events = observer.read(screen);
    const bool unreadable = events.has(ScreenEvent::FADE_BLACK) || events.has(ScreenEvent::FADE_WHITE)
        || events.has(ScreenEvent::DIALOG_BOX);
    if (unreadable){
        return StartSignature{};
    }
    BattlePanelDetector lead_panel;
    PartyBarDetector party_bar;
    std::optional<size_t> party;
    if (party_bar.detect(screen) && party_bar.filled_slots() > 0){
        party = (size_t)party_bar.filled_slots();
    }
    return StartSignature{
        .talk_prompt = events.has(ScreenEvent::INTERACT_PROMPT),
        .wild_zone = lead_panel.detect(screen),
        .party = party,
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

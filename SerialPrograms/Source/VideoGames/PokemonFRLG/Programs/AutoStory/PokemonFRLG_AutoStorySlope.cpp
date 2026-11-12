/*  AutoStory Slope
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <charconv>
#include <format>
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStorySlope.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;
using namespace SlopeMechanics;


namespace{

std::vector<std::string_view> split_words(std::string_view text){
    std::vector<std::string_view> words;
    while (!text.empty()){
        const size_t start = text.find_first_not_of(' ');
        if (start == std::string_view::npos){
            break;
        }
        text.remove_prefix(start);
        const size_t end = std::min(text.find(' '), text.size());
        words.push_back(text.substr(0, end));
        text.remove_prefix(end);
    }
    return words;
}

std::optional<Direction> parse_direction(std::string_view word){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        if (Pokemon::direction_name(direction) == word){
            return direction;
        }
    }
    return std::nullopt;
}

std::optional<size_t> parse_tiles(std::string_view word){
    size_t tiles = 0;
    const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), tiles);
    if (error != std::errc() || end != word.data() + word.size() || tiles == 0){
        return std::nullopt;
    }
    return tiles;
}

//  "roll <n>" or "brake <n>".
std::optional<SlopeMove> parse_roll(SlopeMoveKind kind, const std::vector<std::string_view>& words){
    const std::optional<size_t> tiles = words.size() == 2 ? parse_tiles(words[1]) : std::nullopt;
    if (!tiles){
        return std::nullopt;
    }
    return SlopeMove{.kind = kind, .tiles = *tiles};
}

//  "step <direction> <n>" or "search <direction> <n> <item>".
std::optional<SlopeMove> parse_directed(SlopeMoveKind kind, const std::vector<std::string_view>& words){
    const size_t count = kind == SlopeMoveKind::SEARCH ? 4 : 3;
    if (words.size() != count){
        return std::nullopt;
    }
    const std::optional<Direction> direction = parse_direction(words[1]);
    const std::optional<size_t> tiles = parse_tiles(words[2]);
    if (!direction || !tiles){
        return std::nullopt;
    }
    SlopeMove move{.kind = kind, .direction = *direction, .tiles = *tiles};
    if (kind == SlopeMoveKind::SEARCH){
        move.item = std::string(words[3]);
    }
    return move;
}

SlopePress held_b(Milliseconds hold){
    return SlopePress{.b = true, .hold = hold};
}

} // namespace


std::optional<SlopeMove> parse_slope_move(std::string_view text){
    const std::vector<std::string_view> words = split_words(text);
    if (words.empty()){
        return std::nullopt;
    }
    if (words[0] == "roll"){
        return parse_roll(SlopeMoveKind::ROLL, words);
    }
    if (words[0] == "brake"){
        return parse_roll(SlopeMoveKind::BRAKE, words);
    }
    if (words[0] == "step"){
        return parse_directed(SlopeMoveKind::STEP, words);
    }
    if (words[0] == "search"){
        return parse_directed(SlopeMoveKind::SEARCH, words);
    }
    return std::nullopt;
}

std::optional<std::vector<SlopeMove>> parse_slope_step(const Pokemon::StoryStep& step){
    const std::vector<std::string>* words = step.action == SLOPE_VARIANT
        ? game_arg<std::vector<std::string>>(step.args, "moves")
        : nullptr;
    if (words == nullptr || words->empty()){
        return std::nullopt;
    }
    std::vector<SlopeMove> moves;
    for (const std::string& text : *words){
        std::optional<SlopeMove> move = parse_slope_move(text);
        if (!move){
            return std::nullopt;
        }
        moves.push_back(std::move(*move));
    }
    return moves;
}

Pokemon::StoryActionSpec slope_action_spec(){
    return {.kind = Pokemon::StoryStepKind::GAME, .name = SLOPE_VARIANT, .args = {
        required_arg("moves", Pokemon::StoryArgType::WORDS,
            "the moves on Route 17's slope, in order: \"step <direction> <n>\", \"roll <n>\", \"brake <n>\", "
            "\"search <direction> <n> <item>\" (PokemonFRLG_AutoStorySlope.h)"),
    }};
}

std::vector<SlopePress> slope_move_presses(const SlopeMove& move){
    std::vector<SlopePress> presses;
    switch (move.kind){
    case SlopeMoveKind::STEP:
        for (size_t c = 0; c < move.tiles; c++){
            presses.push_back(SlopePress{.b = true, .dpad = move.direction, .hold = STEP_TAP});
            presses.push_back(held_b(STEP_HOLD));
        }
        break;
    case SlopeMoveKind::ROLL:
        presses.push_back(SlopePress{.hold = ROLL_TILE_SLOWEST * move.tiles + BRAKE_SETTLE});
        break;
    case SlopeMoveKind::BRAKE:
        presses.push_back(SlopePress{.hold = ROLL_TILE * move.tiles});
        presses.push_back(held_b(BRAKE_SETTLE));
        break;
    case SlopeMoveKind::SEARCH:
        for (size_t c = 0; c < move.tiles; c++){
            presses.push_back(SlopePress{.b = true, .dpad = move.direction, .hold = STEP_TAP});
            presses.push_back(held_b(STEP_HOLD));
            presses.push_back(SlopePress{.b = true, .a = true, .hold = SEARCH_A});
            presses.push_back(held_b(SEARCH_WAIT));
        }
        break;
    }
    return presses;
}

std::vector<SlopePress> close_message_presses(){
    std::vector<SlopePress> presses;
    for (size_t c = 0; c < CLOSE_PRESSES; c++){
        presses.push_back(SlopePress{.b = true, .a = true, .hold = CLOSE_PRESS});
        presses.push_back(held_b(CLOSE_WAIT));
    }
    return presses;
}

SlopeResult ride_slope(const std::vector<SlopeMove>& moves, const SlopeCalls& calls){
    SlopeResult result;
    std::vector<SlopePress> batch;
    size_t first = 0;
    for (size_t m = 0; m < moves.size(); m++){
        const std::vector<SlopePress> presses = slope_move_presses(moves[m]);
        batch.insert(batch.end(), presses.begin(), presses.end());
        const bool search = moves[m].kind == SlopeMoveKind::SEARCH;
        if (!search && m + 1 < moves.size()){
            continue;
        }
        calls.log(std::format("Slope: moves {} to {}, {} presses.", first + 1, m + 1, batch.size()));
        const SlopeSent sent = calls.send(batch, search);
        batch.clear();
        first = m + 1;
        if (sent == SlopeSent::BATTLE){
            return SlopeResult{.end = SlopeEnd::BATTLE, .found = result.found, .move = m};
        }
        if (search && sent != SlopeSent::MESSAGE){
            return SlopeResult{.end = SlopeEnd::NOT_FOUND, .found = result.found, .move = m};
        }
        if (search){
            calls.log("Slope: " + moves[m].item + "'s message opened.");
            result.found.push_back(moves[m].item);
            batch = close_message_presses();
        }
    }
    if (!batch.empty()){
        //  A search was the last move: its message is closed with B held.
        calls.send(batch, false);
    }
    return result;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

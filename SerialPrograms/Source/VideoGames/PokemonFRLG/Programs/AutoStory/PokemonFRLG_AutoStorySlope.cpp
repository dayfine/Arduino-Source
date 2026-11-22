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

std::optional<std::vector<SlopeMove>> parse_slope_step(const StoryStep& step){
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

std::vector<SlopePress> slope_move_presses(const SlopeMove& move){
    std::vector<SlopePress> presses;
    switch (move.kind){
    case SlopeMoveKind::STEP:
        presses.push_back(held_b(STEP_HOLD));
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
        presses.push_back(held_b(STEP_HOLD));
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

namespace{

//  A move that keeps the player's column: a step south.
bool keeps_column(const SlopeMove& move){
    return move.kind == SlopeMoveKind::STEP && move.direction == Pokemon::Direction::SOUTH;
}

//  The move a batch's `press` falls in: `ends` holds, for each move of the
//  batch from `first`, the press after its last.
size_t move_at_press(const std::vector<size_t>& ends, size_t first, size_t press){
    for (size_t c = 0; c < ends.size(); c++){
        if (press < ends[c]){
            return first + c;
        }
    }
    return first + ends.size() - 1;
}

} // namespace

std::optional<size_t> slope_resume_move(const std::vector<SlopeMove>& moves, size_t move){
    size_t next = move;
    while (next < moves.size() && keeps_column(moves[next])){
        next++;
    }
    if (next < moves.size()){
        return moves[next].kind == SlopeMoveKind::ROLL ? std::optional<size_t>(next) : std::nullopt;
    }
    for (size_t back = move; back-- > 0;){
        if (moves[back].kind == SlopeMoveKind::ROLL){
            return back;
        }
        if (!keeps_column(moves[back])){
            return std::nullopt;
        }
    }
    return std::nullopt;
}

SlopeResult ride_slope(const std::vector<SlopeMove>& moves, const SlopeCalls& calls){
    SlopeResult result;
    std::vector<SlopePress> batch;
    std::vector<size_t> ends;
    size_t first = 0;
    size_t battles = 0;
    size_t dialogs = 0;
    for (size_t m = first; m < moves.size(); m++){
        const size_t move_start = batch.size();
        const std::vector<SlopePress> presses = slope_move_presses(moves[m]);
        batch.insert(batch.end(), presses.begin(), presses.end());
        ends.push_back(batch.size());
        const bool search = moves[m].kind == SlopeMoveKind::SEARCH;
        if (!search && m + 1 < moves.size()){
            continue;
        }
        //  A hidden item's message can only open once the SEARCH move's first
        //  A press has been sent (press 3 of slope_move_presses(SEARCH)).
        size_t search_a_start = search ? move_start + 3 : batch.size();
        calls.log(std::format("Slope: moves {} to {}, {} presses.", first + 1, m + 1, batch.size()));
        bool resumed_from_move = false;
        while (true){
            const SlopeSendResult sent = calls.send(batch, true);
            const size_t under_way = move_at_press(ends, first, sent.press);
            const bool item_message = search
                && sent.sent == SlopeSent::MESSAGE
                && sent.press >= search_a_start;
            bool battle = sent.sent == SlopeSent::BATTLE;
            if (sent.sent == SlopeSent::MESSAGE && !item_message){
                calls.log(std::format("Slope: unexpected dialog in move {}; clearing it.", under_way + 1));
                battle = calls.clear_dialog && calls.clear_dialog();
                if (!battle){
                    if (++dialogs > SLOPE_MAX_DIALOGS){
                        return SlopeResult{.end = SlopeEnd::BATTLE, .found = result.found, .move = under_way};
                    }
                    const std::optional<size_t> resume = slope_resume_move(moves, under_way);
                    if (resume){
                        calls.log(std::format("Slope: dialog cleared in move {}; on from move {}.", under_way + 1, *resume + 1));
                        batch.clear();
                        ends.clear();
                        first = *resume;
                        m = first - 1;
                        resumed_from_move = true;
                        break;
                    }
                    //  B stayed held while clearing the dialog (e.g. "REPEL's
                    //  effect wore off!"), so the player did not roll: continue
                    //  with the remaining presses of the batch.
                    const bool keep_current = sent.press < batch.size()
                        && !batch[sent.press].b
                        && sent.press_remaining > Milliseconds(0);
                    const size_t drop = keep_current ? sent.press : std::min(sent.press + 1, batch.size());
                    if (drop >= batch.size()){
                        batch.clear();
                        ends.clear();
                        first = m + 1;
                        if (search){
                            return SlopeResult{.end = SlopeEnd::NOT_FOUND, .found = result.found, .move = m};
                        }
                        break;
                    }
                    std::vector<SlopePress> remaining(batch.begin() + drop, batch.end());
                    if (keep_current){
                        remaining.front().hold = sent.press_remaining;
                    }
                    for (size_t& end : ends){
                        end = end > drop ? end - drop : 0;
                    }
                    search_a_start = search_a_start > drop ? search_a_start - drop : 0;
                    batch = std::move(remaining);
                    calls.log(std::format("Slope: dialog cleared in move {}; resuming {} remaining presses.", under_way + 1, batch.size()));
                    continue;
                }
            }
            batch.clear();
            ends.clear();
            first = m + 1;
            if (battle){
                const bool won = calls.fight();
                const std::optional<size_t> resume = slope_resume_move(moves, under_way);
                if (!won || !resume || ++battles > SLOPE_MAX_BATTLES){
                    return SlopeResult{.end = SlopeEnd::BATTLE, .found = result.found, .move = under_way};
                }
                calls.log(std::format("Slope: a battle in move {}; on from move {}, rolled to its stop.", under_way + 1, *resume + 1));
                first = *resume;
                m = first - 1;
                resumed_from_move = true;
                break;
            }
            if (search && !item_message){
                return SlopeResult{.end = SlopeEnd::NOT_FOUND, .found = result.found, .move = m};
            }
            if (search){
                calls.log("Slope: " + moves[m].item + "'s message opened.");
                result.found.push_back(moves[m].item);
                batch = close_message_presses();
            }
            break;
        }
        if (resumed_from_move){
            continue;
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

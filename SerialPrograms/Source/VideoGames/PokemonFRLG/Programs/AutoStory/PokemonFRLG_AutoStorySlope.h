/*  AutoStory Slope
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Route 17, the Cycling Road: its slope (pokefirered
 *  MB_CYCLING_ROAD_PULL_DOWN) pulls a rider south a tile at a time unless B
 *  is held. The route has no runtime map (user, 2026-11-12: "needs to be
 *  mapless"): the story rides it with presses, counted from where its
 *  stops leave the player. Each stop is water or a wall the slope rolls the
 *  player onto, or the message of a hidden item found with A.
 *
 *  The SLOPE_VARIANT GAME step's `moves` are words, one move each:
 *    "step <direction> <n>"  B held, n taps of the D-pad, a tile each.
 *    "roll <n>"              B released: the slope pulls the player to the
 *                            stop n tiles south, and the wait is n tiles'.
 *    "brake <n>"             B released for n tiles' roll, then held.
 *    "search <direction> <n> <item>"  B held: a tap, then A, up to n times,
 *                            until the hidden item's message opens.
 *  The controller's queue is never empty between two moves, nor while B
 *  holds the player on the slope (a gap rolls them): the presses up to a
 *  search, and from its message to the next, go to the controller in one
 *  batch.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySlope_H
#define PokemonAutomation_PokemonFRLG_AutoStorySlope_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon/Navigation/Pokemon_Direction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline const std::string SLOPE_VARIANT = "slope";

//  Not measured on the console (11.02 has not run yet): each is a guess,
//  to be measured on Route 17 before the first run counts.
namespace SlopeMechanics{
//  A tile of the slope's pull with B released. pokefirered's downhill
//  ride is the bike's top speed; on flat ground the Bicycle took 100ms a
//  tile from rest (MovementMechanics::BIKE_TILE_TIME). Sets where "brake"
//  stops the player.
inline constexpr Milliseconds ROLL_TILE(100);
//  The slowest a roll may be, for the wait of "roll" to its stop: an
//  overlong wait is harmless, the player stands at the stop.
inline constexpr Milliseconds ROLL_TILE_SLOWEST(150);
//  After B is pressed mid-roll, until the player stands: the tile under
//  way finishes. Whether B stops the player at once is not known.
inline constexpr Milliseconds BRAKE_SETTLE(300);
//  A D-pad tap with B held: whether it moves exactly a tile on the slope
//  is not known (on flat ground a 40ms tap does, BIKE_TAP).
inline constexpr Milliseconds STEP_TAP(40);
//  B alone after each tap, start to start BIKE_TAP_PERIOD and more.
inline constexpr Milliseconds STEP_HOLD(160);
//  A with B held: whether the game takes the A with B down is not known.
inline constexpr Milliseconds SEARCH_A(160);
//  B alone after the A, for the item's message to open.
inline constexpr Milliseconds SEARCH_WAIT(900);
//  A and B pressed together, then B alone, this many times to close the
//  item's two pages ("found a ..." and "put the ... in the ITEMS POCKET").
//  An A more faces a tile with nothing on it.
inline constexpr size_t CLOSE_PRESSES = 4;
inline constexpr Milliseconds CLOSE_PRESS(160);
inline constexpr Milliseconds CLOSE_WAIT(700);
}

enum class SlopeMoveKind{
    STEP,
    ROLL,
    BRAKE,
    SEARCH,
};

struct SlopeMove{
    SlopeMoveKind kind = SlopeMoveKind::ROLL;
    //  STEP's and SEARCH's.
    Pokemon::Direction direction = Pokemon::Direction::SOUTH;
    //  STEP: taps; ROLL: tiles to the stop; BRAKE: tiles rolled before B;
    //  SEARCH: most taps.
    size_t tiles = 0;
    //  SEARCH's item, its bag slug ("rare-candy").
    std::string item = {};
};

//  One move's words, or nothing if they aren't one.
std::optional<SlopeMove> parse_slope_move(std::string_view words);
//  A SLOPE_VARIANT step's moves, or nothing if it isn't one or a move
//  doesn't parse.
std::optional<std::vector<SlopeMove>> parse_slope_step(const Pokemon::StoryStep& step);
//  SLOPE_VARIANT and its args.
Pokemon::StoryActionSpec slope_action_spec();

//  One state of the controller, held for `hold`.
struct SlopePress{
    bool b = false;
    bool a = false;
    std::optional<Pokemon::Direction> dpad = std::nullopt;
    Milliseconds hold = Milliseconds(0);
};

//  The presses of `move`, back to back.
std::vector<SlopePress> slope_move_presses(const SlopeMove& move);
//  The presses that close a found item's message, ending with B held.
std::vector<SlopePress> close_message_presses();

//  How a batch of presses went: all sent, or cut short by a message (an
//  item's, after a search's A) or a battle.
enum class SlopeSent{
    ALL,
    MESSAGE,
    BATTLE,
};
//  What `send` of SlopeCalls reports.
struct SlopeSendResult{
    SlopeSent sent = SlopeSent::ALL;
    //  BATTLE: the press under way when it started, from 0.
    size_t press = 0;
};
//  The console side of ride_slope(): `send` sends `presses` back to back,
//  watching for a message only when `message` is set, and for a battle;
//  `fight` fights a battle that cut a batch short, back to the overworld,
//  and tells whether it was won.
struct SlopeCalls{
    std::function<SlopeSendResult(const std::vector<SlopePress>& presses, bool message)> send;
    std::function<bool()> fight;
    std::function<void(const std::string& message)> log;
};

//  The most battles one ride goes on from.
inline constexpr size_t SLOPE_MAX_BATTLES = 3;
//  After a battle that cut `moves[move]` short ("if a Biker catches us,
//  fight", user 2026-11-12): the ROLL to go on from, which pulls the player
//  down the column they were caught in to a stop the moves know. Only
//  STEP south moves may come between the two, which keep the column: on
//  to the next ROLL, or, with only those left, back to the last one
//  (rolled again past its stop, the player stays where they are, and the
//  steps south after it end at the same water). Nothing if a step east or
//  west, a brake or a search comes first: the column or the row is not
//  known, and the ride stops.
std::optional<size_t> slope_resume_move(const std::vector<SlopeMove>& moves, size_t move);

enum class SlopeEnd{
    DONE,
    //  A search's taps all went with no message: the item isn't where the
    //  moves before it left the player.
    NOT_FOUND,
    //  A battle cut the presses short where slope_resume_move() can't
    //  find the player again, was lost, or came after SLOPE_MAX_BATTLES.
    BATTLE,
};
struct SlopeResult{
    SlopeEnd end = SlopeEnd::DONE;
    //  The items whose messages opened, in order.
    std::vector<std::string> found = {};
    //  The move under way when it ended other than DONE, from 0.
    size_t move = 0;
};
//  Rides `moves`: the presses up to and including each search in one
//  batch, and from its message on with the next. After a battle, fights it
//  and goes on from slope_resume_move().
SlopeResult ride_slope(const std::vector<SlopeMove>& moves, const SlopeCalls& calls);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

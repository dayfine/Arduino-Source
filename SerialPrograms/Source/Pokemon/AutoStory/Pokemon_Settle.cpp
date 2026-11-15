/*  Settle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include "Pokemon_Settle.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A door's settle: the player still this long after the fade is back up
//  ends it; a walk any earlier could still go nowhere in the fade-in
//  (BDSP lab-B 2026-11-10, 00.04b's nurse).
constexpr Milliseconds DOOR_QUIET = Milliseconds(1000);
//  A door's settle: the overworld back up this long ends it, still or not
//  (someone walking past the player the whole time).
constexpr Milliseconds DOOR_CALM = Milliseconds(2500);

} // namespace


SettleRule settle_rule(SettleKind kind){
    switch (kind){
    case SettleKind::TALK:
        return {};
    case SettleKind::AFTER_WARP:
        return {.judge = {.warp = true}};
    case SettleKind::DOOR:
        return {
            .judge = {.calm = DOOR_CALM, .quiet = DOOR_QUIET, .still = ScreenEvent::PLAYER_STILL},
            .look_every = Milliseconds(200),
        };
    }
    return {};
}


namespace{

//  Why a settle that hit its cap failed.
std::string timed_out(const TalkOver& judge, const SettleRule& rule, std::string_view label){
    const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(rule.cap).count();
    if (!judge.warped()){
        return std::format("{}: no fade or banner within {} s, so we are not where the step goes (map mismatch).", label, seconds);
    }
    if (!rule.judge.end_on_quiet){
        return std::format("{}: no battle within {} s of the talk.", label, seconds);
    }
    return std::format("{}: not back on a quiet overworld within {} s.", label, seconds);
}

} // namespace


SettleEnd run_settle(const SettleRule& rule, std::string_view label, SettleIO& io){
    TalkOver judge(rule.judge);
    std::optional<WallClock> deadline;
    while (true){
        const SettleLook look = io.look();
        if (!deadline.has_value()){
            deadline = look.when + rule.cap;
        }
        if (look.when >= *deadline){
            break;
        }
        const SceneMove move = judge.look(look.events, look.when);
        switch (move){
        case SceneMove::DONE:
            io.log(std::format("[Settle] {}: over; {}.", label, judge.describe()));
            return {SettleEnd::Result::QUIET, judge.describe()};
        case SceneMove::BATTLE:
            if (!io.flee_battle()){
                io.log(std::format("[Settle] {}: a battle started.", label));
                return {SettleEnd::Result::BATTLE, judge.describe()};
            }
            io.log(std::format("[Settle] {}: fled a wild battle.", label));
            continue;
        case SceneMove::NEXT:
        case SceneMove::B:
            io.press(move, rule.look_every);
            continue;
        }
    }
    const std::string failure = timed_out(judge, rule, label);
    io.log(std::format("[Settle] {}", failure));
    return {SettleEnd::Result::TIMED_OUT, failure};
}



} // namespace Pokemon
} // namespace PokemonAutomation

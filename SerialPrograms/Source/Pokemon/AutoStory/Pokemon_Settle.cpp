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


SettleEnd run_settle(SettleIO& io, const SettleRule& rule, std::string_view label){
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
            return {SettleResult::QUIET, judge.describe()};
        case SceneMove::BATTLE:
            if (!io.flee_battle()){
                io.log(std::format("[Settle] {}: a battle started.", label));
                return {SettleResult::BATTLE, judge.describe()};
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
    io.log("[Settle] " + failure);
    return {SettleResult::TIMED_OUT, failure};
}



} // namespace Pokemon
} // namespace PokemonAutomation

/*  BDSP RNG Timeline
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <compare>
#include <functional>
#include <queue>
#include <vector>
#include "PokemonBDSP_BlinkModel.h"
#include "PokemonBDSP_RngTimeline.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


namespace{

enum class TimelineEventType : uint8_t{
    NpcTick = 0,
    PokemonBlink = 1,
};

struct TimelineEvent{
    double time_seconds;
    TimelineEventType type;
    uint8_t index;

    //  Member order is the tie-break
    auto operator<=>(const TimelineEvent&) const = default;
};

using TimelineQueue = std::priority_queue<
    TimelineEvent, std::vector<TimelineEvent>, std::greater<TimelineEvent>
>;

TimelineQueue seed_timeline_queue(
    BdspTimelineResult& result, Xorshift128& rng,
    const BdspTimelineContext& context, double now
){
    TimelineQueue queue;
    for (uint8_t c = 0; c < context.npcs; c++){
        queue.push(TimelineEvent{now + BDSP_NPC_TICK_SECONDS, TimelineEventType::NpcTick, c});
    }
    for (uint8_t c = 0; c < context.pokemon_models; c++){
        const double interval = bdsp_pokemon_blink_interval(rng.next());
        result.advance_times.emplace_back(now);
        queue.push(TimelineEvent{now + interval, TimelineEventType::PokemonBlink, c});
    }
    return queue;
}

void advance_timeline_event(
    BdspTimelineResult& result, Xorshift128& rng, TimelineEvent& event
){
    const double now = event.time_seconds;
    switch (event.type){
    case TimelineEventType::NpcTick:
        rng.next();
        result.advance_times.emplace_back(now);
        event.time_seconds += BDSP_NPC_TICK_SECONDS;
        return;
    case TimelineEventType::PokemonBlink:
        event.time_seconds += bdsp_pokemon_blink_interval(rng.next());
        result.advance_times.emplace_back(now);
        return;
    }
}

} // namespace


static void spend_advances(
    BdspTimelineResult& result, Xorshift128& rng,
    double now_seconds, uint64_t count
){
    for (uint64_t c = 0; c < count; c++){
        rng.next();
        result.advance_times.emplace_back(now_seconds);
    }
}


BdspTimelineResult simulate_timeline(
    Xorshift128 rng,
    const BdspTimelineContext& context,
    uint64_t max_advances
){
    BdspTimelineResult result;
    double now = 0.0;

    spend_advances(result, rng, now, 1);
    if (context.plus_one_on_menu_close){
        spend_advances(result, rng, now, 1);
    }
    now += context.white_delay_seconds;
    spend_advances(result, rng, now, context.advance_delay);

    TimelineQueue queue = seed_timeline_queue(result, rng, context, now);
    uint64_t events_handled = 0;
    bool second_delay_spent = false;

    while (result.advance_times.size() < max_advances){
        if (queue.empty()){
            result.end_state = rng.state();
            return result;
        }
        TimelineEvent event = queue.top();
        queue.pop();
        now = event.time_seconds;
        if (context.advance_delay_2 != 0
            && !second_delay_spent
            && events_handled >= context.advance_delay_2_after_events
        ){
            spend_advances(result, rng, now, context.advance_delay_2);
            second_delay_spent = true;
        }
        advance_timeline_event(result, rng, event);
        queue.push(event);
        events_handled++;
    }

    result.end_state = rng.state();
    result.reached_target = true;
    return result;
}


bool time_of_advance(const BdspTimelineResult& result, uint64_t advance, double& seconds){
    if (advance >= result.advance_times.size()){
        return false;
    }
    seconds = result.advance_times[(size_t)advance];
    return true;
}




} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

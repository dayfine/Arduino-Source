/*  Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Pokemon_Interrupts.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* warp_judgement_name(WarpJudgement judgement){
    switch (judgement){
    case WarpJudgement::NO_FADE:        return "no fade";
    case WarpJudgement::FADE_SHOWING:   return "fade still showing";
    case WarpJudgement::TOO_SHORT:      return "fade too short";
    case WarpJudgement::SAME_SCENE:     return "same scene after the fade";
    case WarpJudgement::WARPED:         return "warped";
    case WarpJudgement::FLASH:          return "an encounter's flash";
    case WarpJudgement::WHITE_UNSURE:   return "a white, flash or fade";
    }
    return "?";
}

const char* interrupt_kind_name(InterruptKind kind){
    switch (kind){
    case InterruptKind::NONE:           return "none";
    case InterruptKind::WILD_ENCOUNTER: return "wild encounter";
    case InterruptKind::TRAINER_SIGHT:  return "trainer sight";
    case InterruptKind::DIALOG:         return "dialog";
    case InterruptKind::WARP:           return "warp";
    case InterruptKind::UNSETTLED:      return "unsettled";
    }
    return "?";
}


namespace{

using Changes = std::vector<ScreenEventChange>;

Changes changes_in(const ScreenEventLog& log, InterruptWindow window){
    Changes out;
    for (const ScreenEventChange& change : log.changes()){
        if (window.start <= change.time && change.time <= window.end){
            out.push_back(change);
        }
    }
    return out;
}

bool rose(const ScreenEventChange& change, ScreenEvent event){
    return change.event == event && change.edge == ScreenEventEdge::ROSE;
}
bool fade_rose(const ScreenEventChange& change){
    return rose(change, ScreenEvent::FADE_BLACK) || rose(change, ScreenEvent::FADE_WHITE);
}

//  The first rise of `event`, if any.
std::optional<ScreenEventChange> first_rise(const Changes& changes, ScreenEvent event){
    for (const ScreenEventChange& change : changes){
        if (rose(change, event)){
            return change;
        }
    }
    return std::nullopt;
}

//  The last fade that went down, and when it came back up.
std::optional<FadeSpan> last_fade(const Changes& changes){
    std::optional<FadeSpan> out;
    for (const ScreenEventChange& change : changes){
        if (fade_rose(change)){
            out = FadeSpan{fade_in({change.event}), change.time, std::nullopt};
        }else if (out && !out->up && change.edge == ScreenEventEdge::FELL && fade_events(out->fade).has(change.event)){
            out->up = change.time;
        }
    }
    return out;
}

ScreenEvent fade_event(WarpFade fade){
    return fade == WarpFade::WHITE ? ScreenEvent::FADE_WHITE : ScreenEvent::FADE_BLACK;
}
void add_fade_evidence(Changes& evidence, const FadeSpan& fade){
    evidence.push_back({fade.down, fade_event(fade.fade), ScreenEventEdge::ROSE});
    if (fade.up){
        evidence.push_back({*fade.up, fade_event(fade.fade), ScreenEventEdge::FELL});
    }
}


//  A battle, and whose. The battle's fade is the last one that went down
//  no more than battle_after_fade before it; a box that rose before that
//  fade (or before the battle, with no fade) is a trainer's lines. A box
//  after the fade is the battle's own opening text.
std::optional<InterruptVerdict> battle_verdict(const Changes& changes, const InterruptKnobs& knobs){
    const std::optional<ScreenEventChange> battle = first_rise(changes, ScreenEvent::BATTLE_STARTED);
    if (!battle){
        return std::nullopt;
    }
    std::optional<ScreenEventChange> fade;
    for (const ScreenEventChange& change : changes){
        if (change.time > battle->time){
            break;
        }
        if (fade_rose(change) && battle->time - change.time <= knobs.battle_after_fade){
            fade = change;
        }
    }
    const WallClock cut = fade ? fade->time : battle->time;
    std::optional<ScreenEventChange> lines;
    for (const ScreenEventChange& change : changes){
        if (change.time >= cut){
            break;
        }
        if (rose(change, ScreenEvent::DIALOG_BOX) && cut - change.time <= knobs.sight_lines_lead){
            lines = change;
            break;
        }
    }
    InterruptVerdict out{lines ? InterruptKind::TRAINER_SIGHT : InterruptKind::WILD_ENCOUNTER, {}, false};
    for (const std::optional<ScreenEventChange>& change : {lines, fade, battle}){
        if (change){
            out.evidence.push_back(*change);
        }
    }
    return out;
}

//  A warp: a banner, or a fade back up with no battle after it for
//  battle_after_fade. A fade still showing, or back up too recently, may
//  yet be an encounter's flash.
std::optional<InterruptVerdict> warp_verdict(
    const ScreenEventLog& log, InterruptWindow window, const Changes& changes, const InterruptKnobs& knobs
){
    const WarpVerdict warp = judge_warp(log, window, knobs, SceneChange::UNKNOWN);
    const std::optional<ScreenEventChange> banner = first_rise(changes, ScreenEvent::LOCATION_BANNER);
    InterruptVerdict out{InterruptKind::WARP, {}, false};
    if (warp.fade && warp.judgement != WarpJudgement::TOO_SHORT){
        add_fade_evidence(out.evidence, *warp.fade);
    }
    if (banner){
        out.evidence.push_back(*banner);
        std::stable_sort(out.evidence.begin(), out.evidence.end(), [](const auto& a, const auto& b){
            return a.time < b.time;
        });
        return out;
    }
    switch (warp.judgement){
    case WarpJudgement::FADE_SHOWING:
        out.kind = InterruptKind::UNSETTLED;
        return out;
    case WarpJudgement::WARPED:{
        const WallClock seen_until = std::min(window.end, log.last_frame().value_or(window.end));
        if (seen_until - *warp.fade->up < knobs.battle_after_fade){
            out.kind = InterruptKind::UNSETTLED;
        }
        return out;
    }
    case WarpJudgement::NO_FADE:
    case WarpJudgement::TOO_SHORT:
    case WarpJudgement::SAME_SCENE:
    case WarpJudgement::FLASH:
    case WarpJudgement::WHITE_UNSURE:
        break;
    }
    return std::nullopt;
}

std::optional<InterruptVerdict> dialog_verdict(const Changes& changes){
    const std::optional<ScreenEventChange> box = first_rise(changes, ScreenEvent::DIALOG_BOX);
    if (!box){
        return std::nullopt;
    }
    return InterruptVerdict{InterruptKind::DIALOG, {*box}, false};
}

} // namespace


WarpVerdict judge_warp(
    const ScreenEventLog& log, InterruptWindow window,
    const InterruptKnobs& knobs, SceneChange change
){
    WarpVerdict out{WarpJudgement::NO_FADE, last_fade(changes_in(log, window))};
    if (!out.fade){
        return out;
    }
    if (!out.fade->up){
        out.judgement = WarpJudgement::FADE_SHOWING;
    }else if (*out.fade->up - out.fade->down < knobs.min_fade_hold){
        out.judgement = WarpJudgement::TOO_SHORT;
    }else if (change == SceneChange::SAME){
        out.judgement = WarpJudgement::SAME_SCENE;
    }else{
        out.judgement = WarpJudgement::WARPED;
    }
    return out;
}

WarpVerdict judge_warp(const Transition& transition, const InterruptKnobs& knobs){
    const WarpFade fade = transition.kind == TransitionKind::FADE_BLACK ? WarpFade::BLACK : WarpFade::WHITE;
    WarpVerdict out{WarpJudgement::WARPED, FadeSpan{fade, transition.start, transition.end}};
    if (!transition.end){
        out.judgement = WarpJudgement::FADE_SHOWING;
    }else if (transition.kind == TransitionKind::FLASH){
        out.judgement = WarpJudgement::FLASH;
    }else if (transition.change == SceneChange::SAME){
        out.judgement = WarpJudgement::SAME_SCENE;
    }else if (transition.kind == TransitionKind::WHITE_UNSURE){
        out.judgement = WarpJudgement::WHITE_UNSURE;
    }else if (transition.peak && transition.change != SceneChange::DIFFERENT && transition.hold() < knobs.min_fade_hold){
        out.judgement = WarpJudgement::TOO_SHORT;
    }
    return out;
}


std::string describe(const InterruptVerdict& verdict, WallClock origin){
    std::string out = interrupt_kind_name(verdict.kind);
    const char* separator = ": ";
    for (const ScreenEventChange& change : verdict.evidence){
        out += std::format(
            "{}{:+} ms {} {}",
            separator,
            std::chrono::duration_cast<Milliseconds>(change.time - origin).count(),
            screen_event_name(change.event),
            change.edge == ScreenEventEdge::ROSE ? "rose" : "fell"
        );
        separator = ", ";
    }
    if (verdict.log_truncated){
        out += " (log truncated)";
    }
    return out;
}


InterruptVerdict classify_interrupt(
    const ScreenEventLog& log, InterruptWindow window, const InterruptKnobs& knobs
){
    const Changes changes = changes_in(log, window);
    std::optional<InterruptVerdict> verdict = battle_verdict(changes, knobs);
    if (!verdict){
        verdict = warp_verdict(log, window, changes, knobs);
    }
    if (!verdict){
        verdict = dialog_verdict(changes);
    }
    InterruptVerdict out = verdict.value_or(InterruptVerdict{});
    out.log_truncated = log.dropped() > 0 &&
        (log.changes().empty() || window.start <= log.changes().front().time);
    return out;
}



} // namespace Pokemon
} // namespace PokemonAutomation

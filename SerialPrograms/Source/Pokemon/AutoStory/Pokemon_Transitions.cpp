/*  Transitions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "Pokemon_Transitions.h"

namespace PokemonAutomation{
namespace Pokemon{


const FadeProfile BDSP_FADE_PROFILE{
    .uniform_max_stddev_sum = 10,
    .partial_max_stddev_sum = 60,
    .dark_max_sum = 225,
    .partial_dark_max_sum = 300,
    .white_min_sum = 500,
    .partial_white_min_sum = 450,
    .max_fall = Milliseconds(1000),
    .max_hold = Milliseconds(8000),
    .has_white_fade = true,
    .flash_max_hold = Milliseconds(150),
    .white_fade_min_hold = Milliseconds(400),
};
const FadeProfile FRLG_FADE_PROFILE{
    .uniform_max_stddev_sum = 10,
    .partial_max_stddev_sum = 60,
    .dark_max_sum = 100,
    .partial_dark_max_sum = 200,
    .white_min_sum = 500,
    .partial_white_min_sum = 450,
    .max_fall = Milliseconds(1000),
    .max_hold = Milliseconds(8000),
    .has_white_fade = false,
    .flash_max_hold = Milliseconds(150),
    .white_fade_min_hold = Milliseconds(400),
};


std::string transition_kind_name(TransitionKind kind){
    switch (kind){
    case TransitionKind::FADE_BLACK:    return "black fade";
    case TransitionKind::FADE_WHITE:    return "white fade";
    case TransitionKind::FLASH:         return "flash";
    case TransitionKind::WHITE_UNSURE:  return "white, flash or fade";
    }
    return "?";
}


TransitionSample transition_sample(const VideoSnapshot& frame){
    TransitionSample out{frame.timestamp, {}, frame};
    if (frame){
        out.middle = image_stats(extract_box_reference(*frame.frame, ImageFloatBox(0.1, 0.1, 0.8, 0.8)));
    }
    return out;
}


Milliseconds Transition::hold() const{
    if (!peak || !rise){
        return Milliseconds(0);
    }
    return std::chrono::duration_cast<Milliseconds>(*rise - *peak);
}

namespace{

std::string offset(std::optional<WallClock> time, WallClock origin){
    if (!time){
        return "unseen";
    }
    return std::format("{:+} ms", std::chrono::duration_cast<Milliseconds>(*time - origin).count());
}
std::string scene_change_name(SceneChange change){
    switch (change){
    case SceneChange::UNKNOWN:      return "scene unknown";
    case SceneChange::SAME:         return "same scene";
    case SceneChange::DIFFERENT:    return "other scene";
    }
    return "?";
}

} // namespace

std::string describe(const Transition& transition, WallClock origin){
    std::string out = std::format(
        "{} {}, peak {}, rise {}",
        transition_kind_name(transition.kind),
        offset(transition.start, origin),
        offset(transition.peak, origin),
        offset(transition.rise, origin)
    );
    if (!transition.end){
        return std::format("{}, in progress", out);
    }
    return std::format("{}, end {}, {}", out, offset(transition.end, origin), scene_change_name(transition.change));
}


TransitionTracker::TransitionTracker(FadeProfile profile, CompareScenes compare)
    : m_profile(profile)
    , m_compare(std::move(compare))
{}

TransitionTracker::Level TransitionTracker::level_of(const ImageStats& middle) const{
    const double stddev = middle.stddev.sum();
    const double sum = middle.average.sum();
    if (stddev <= m_profile.uniform_max_stddev_sum){
        if (sum <= m_profile.dark_max_sum){
            return Level::DARK;
        }
        if (sum >= m_profile.white_min_sum){
            return Level::WHITE;
        }
    }
    if (stddev <= m_profile.partial_max_stddev_sum){
        if (sum <= m_profile.partial_dark_max_sum){
            return Level::PARTIAL_DARK;
        }
        if (sum >= m_profile.partial_white_min_sum){
            return Level::PARTIAL_WHITE;
        }
    }
    return Level::SCENE;
}

std::optional<Transition> TransitionTracker::push(const TransitionSample& sample){
    switch (level_of(sample.middle)){
    case Level::SCENE:
        return on_scene(sample);
    case Level::PARTIAL_DARK:
        return on_partial(sample, false);
    case Level::PARTIAL_WHITE:
        return on_partial(sample, true);
    case Level::DARK:
        on_uniform(sample, false);
        return std::nullopt;
    case Level::WHITE:
        on_uniform(sample, true);
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<Transition> TransitionTracker::on_scene(const TransitionSample& sample){
    m_scene_is_partial = false;
    if (m_phase == TransitionPhase::SCENE){
        m_before = sample;
        return std::nullopt;
    }
    if (m_phase == TransitionPhase::HOLD){
        m_current.rise = sample.time;
    }
    return finish(sample);
}

//  A partial frame starts a fall from a scene, unless the scene itself
//  looks partial (a dim floor). Partial frames for longer than max_fall,
//  falling or rising, are a scene again.
std::optional<Transition> TransitionTracker::on_partial(const TransitionSample& sample, bool white){
    switch (m_phase){
    case TransitionPhase::SCENE:
        if (m_scene_is_partial){
            m_before = sample;
            return std::nullopt;
        }
        m_current = Transition{.start = sample.time};
        m_white = white;
        m_phase = TransitionPhase::FALLING;
        return std::nullopt;
    case TransitionPhase::FALLING:
        m_white = white;
        if (sample.time - m_current.start > m_profile.max_fall){
            m_phase = TransitionPhase::SCENE;
            m_scene_is_partial = true;
            m_before = sample;
        }
        return std::nullopt;
    case TransitionPhase::HOLD:
        m_current.rise = sample.time;
        m_phase = TransitionPhase::RISING;
        return std::nullopt;
    case TransitionPhase::RISING:
        if (sample.time - *m_current.rise <= m_profile.max_fall){
            return std::nullopt;
        }
        m_scene_is_partial = true;
        return finish(sample);
    }
    return std::nullopt;
}

void TransitionTracker::on_uniform(const TransitionSample& sample, bool white){
    switch (m_phase){
    case TransitionPhase::SCENE:
        m_current = Transition{.start = sample.time, .peak = sample.time};
        m_white = white;
        break;
    case TransitionPhase::FALLING:
        m_current.peak = sample.time;
        m_white = white;
        break;
    case TransitionPhase::HOLD:
        break;
    case TransitionPhase::RISING:
        //  Back down again before a scene showed: the same hold.
        m_current.rise.reset();
        break;
    }
    m_phase = TransitionPhase::HOLD;
}

//  Back on a scene: a transition if it held, or if it fell to another
//  scene with its hold unseen. A fall back to the same scene is a dip.
std::optional<Transition> TransitionTracker::finish(const TransitionSample& sample){
    const SceneChange change = m_compare && m_before ? m_compare(*m_before, sample) : SceneChange::UNKNOWN;
    m_phase = TransitionPhase::SCENE;
    m_before = sample;
    if (!m_current.peak && change != SceneChange::DIFFERENT){
        return std::nullopt;
    }
    m_current.end = sample.time;
    m_current.change = change;
    m_current.kind = m_white ? kind_of(m_current.hold(), change) : TransitionKind::FADE_BLACK;
    m_last = m_current;
    return m_current;
}

TransitionKind TransitionTracker::kind_of(Milliseconds hold, SceneChange change) const{
    if (!m_profile.has_white_fade){
        return TransitionKind::FLASH;
    }
    if (hold <= m_profile.flash_max_hold && change != SceneChange::DIFFERENT){
        return TransitionKind::FLASH;
    }
    if (hold >= m_profile.white_fade_min_hold && change != SceneChange::SAME){
        return TransitionKind::FADE_WHITE;
    }
    return TransitionKind::WHITE_UNSURE;
}

std::optional<Transition> TransitionTracker::current() const{
    if (m_phase == TransitionPhase::SCENE){
        return m_last;
    }
    Transition out = m_current;
    out.kind = m_white ? TransitionKind::WHITE_UNSURE : TransitionKind::FADE_BLACK;
    return out;
}

bool TransitionTracker::timed_out(WallClock now) const{
    return m_phase != TransitionPhase::SCENE && now - m_current.start > m_profile.max_hold;
}

void TransitionTracker::reset(){
    m_phase = TransitionPhase::SCENE;
    m_white = false;
    m_scene_is_partial = false;
    m_before.reset();
    m_current = Transition{};
    m_last.reset();
}



} // namespace Pokemon
} // namespace PokemonAutomation

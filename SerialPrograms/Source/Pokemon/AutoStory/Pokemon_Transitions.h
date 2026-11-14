/*  Transitions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A screen transition followed over many frames rather than judged on one:
 *  the scene, the screen falling toward a uniform colour, the uniform hold,
 *  the rise, and the scene it comes back on. fade_of() alone judges one
 *  frame, so a 50 ms door fade that falls between two sampled frames is never
 *  seen, and the step waits out its whole timeout (12 s for a BDSP door).
 *  The tracker sees the frames around it instead: the screen falling, then a
 *  different scene.
 *
 *  Each game's numbers are a FadeProfile; the tracker is the same for all.
 *
 */

#ifndef PokemonAutomation_Pokemon_Transitions_H
#define PokemonAutomation_Pokemon_Transitions_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "Pokemon_FadeProfiles.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where the tracker is in a transition.
enum class TransitionPhase : uint8_t{
    SCENE,      //  No transition: the screen shows a scene.
    FALLING,    //  Partial frames, toward dark or white.
    HOLD,       //  Uniform frames.
    RISING,     //  Partial frames after the hold.
};

//  One frame, as the tracker reads it.
struct TransitionSample{
    WallClock time = {};
    //  The stats of the screen's middle 80%, as fade_of_screen() reads.
    ImageStats middle = {};
    //  The frame itself, for the caller's scene comparison. May be empty.
    VideoSnapshot frame = {};
};
//  A sample of `frame`, at its timestamp.
TransitionSample transition_sample(const VideoSnapshot& frame);

//  One transition, complete or in progress.
struct Transition{
    TransitionKind kind = TransitionKind::FADE_BLACK;
    //  The first frame off the scene: falling, or the hold if no fall was seen.
    WallClock start = {};
    //  The first uniform frame. None: the hold fell between two frames, and
    //  only the falling frames and the new scene were seen.
    std::optional<WallClock> peak = {};
    //  The first frame off the hold.
    std::optional<WallClock> rise = {};
    //  The first frame back on a scene. None while in progress.
    std::optional<WallClock> end = {};
    //  The scene after against the one before; UNKNOWN in progress.
    SceneChange change = SceneChange::UNKNOWN;

    //  How long the uniform frames showed; zero with no peak.
    Milliseconds hold() const;
};
//  "black fade +200 ms, peak +300 ms, rise +700 ms, end +800 ms, other scene",
//  times from `origin`.
std::string describe(const Transition& transition, WallClock origin);


//  The transition state machine, fed one frame at a time: scene, falling,
//  hold, rising, scene. Not thread-safe.
class TransitionTracker{
public:
    //  Compares the last scene frame before a transition with the first
    //  after it.
    using CompareScenes = std::function<SceneChange(const TransitionSample& before, const TransitionSample& after)>;

    //  With no `compare`, every scene change is UNKNOWN, and a transition
    //  whose hold went unseen is not reported.
    explicit TransitionTracker(FadeProfile profile, CompareScenes compare = {});

    //  Adds a frame; times must not go back. Returns the transition this
    //  frame completed, if any.
    std::optional<Transition> push(const TransitionSample& sample);

    TransitionPhase phase() const{
        return m_phase;
    }
    //  The transition in progress, else the last one completed.
    std::optional<Transition> current() const;
    //  Whether the transition in progress has gone on longer than max_hold
    //  at `now`.
    bool timed_out(WallClock now) const;
    void reset();

private:
    enum class Level : uint8_t{
        SCENE,
        PARTIAL_DARK,
        PARTIAL_WHITE,
        DARK,
        WHITE,
    };
    Level level_of(const ImageStats& middle) const;

    std::optional<Transition> on_scene(const TransitionSample& sample);
    std::optional<Transition> on_partial(const TransitionSample& sample, bool white);
    void on_uniform(const TransitionSample& sample, bool white);
    std::optional<Transition> finish(const TransitionSample& sample);
    TransitionKind kind_of(Milliseconds hold, SceneChange change) const;

    FadeProfile m_profile;
    CompareScenes m_compare;
    TransitionPhase m_phase = TransitionPhase::SCENE;
    bool m_white = false;
    //  The scene itself looks partial (a dim floor): partial frames are no fall.
    bool m_scene_is_partial = false;
    std::optional<TransitionSample> m_before;
    Transition m_current;
    std::optional<Transition> m_last;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

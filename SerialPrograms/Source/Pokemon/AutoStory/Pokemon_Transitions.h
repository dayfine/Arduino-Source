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

namespace PokemonAutomation{
namespace Pokemon{


//  Whether the screen differs after a transition from before it. The
//  tracker has only frame statistics; the caller compares the frames, or
//  says it did not.
enum class SceneChange{
    UNKNOWN,
    SAME,
    DIFFERENT,
};


//  A game's fades, as data. Two tiers of the same test: a frame is uniform
//  (the fade's hold) when its middle's stddev sum is at most
//  `uniform_max_stddev_sum`, and partial (on the way down or up) when it is
//  at most `partial_max_stddev_sum`. Either way it is dark or white by its
//  rgb sum.
struct FadeProfile{
    double uniform_max_stddev_sum = 10;
    double partial_max_stddev_sum = 60;
    //  Dark at or below this rgb sum; a partial frame up to `partial_dark_max_sum`.
    double dark_max_sum = 225;
    double partial_dark_max_sum = 300;
    //  White at or above this rgb sum; a partial frame from `partial_white_min_sum`.
    double white_min_sum = 500;
    double partial_white_min_sum = 450;

    //  Partial frames for longer than this, with no hold, are the scene's
    //  own look (a dim cave floor, a street at night), not a fade.
    Milliseconds max_fall = Milliseconds(1000);
    //  A transition still dark or white after this has timed out: the
    //  screen did not come back.
    Milliseconds max_hold = Milliseconds(8000);

    //  Whether the game fades to white between maps (BDSP's cave exits).
    //  Without, every white is an encounter's flash.
    bool has_white_fade = true;
    //  A white held this long or less, back on the same scene, is a flash.
    Milliseconds flash_max_hold = Milliseconds(150);
    //  A white held this long or more, not back on the same scene, is a
    //  white fade. Between the two the white is unsure.
    Milliseconds white_fade_min_hold = Milliseconds(400);
};

//  BDSP. The uniform tier is the shared walk's (FADE_MAX_STDDEV_SUM, the
//  Oreburgh Gate's textured floor is no fade, lab-B 2026-11-08); the partial
//  tier is run_into_door()'s 60, which caught the doorway's dim fade frames.
//  A cave mouth's fade is dim (45 to 64 a channel), hence the dark sum of 225.
//  The flash and white-fade holds are provisional: no console frames of an
//  encounter flash and a cave exit have been timed side by side yet, so the
//  band between them is wide and reports WHITE_UNSURE.
extern const FadeProfile BDSP_FADE_PROFILE;
//  FRLG. The GBA fades to true black, so the dark tiers are the shared
//  BlackScreenDetector's 100 and a little above; it has no white fade between
//  maps, so every white is an encounter's flash.
extern const FadeProfile FRLG_FADE_PROFILE;


//  What a transition was.
enum class TransitionKind : uint8_t{
    FADE_BLACK,     //  A door, a cave mouth, a scene cut, a battle's own fade.
    FADE_WHITE,     //  A cave exit: a long white onto another scene.
    FLASH,          //  An encounter's flash: a short white, back on the scene.
    WHITE_UNSURE,   //  A white that may be either: wait for a battle.
};
//  "black fade", for the log.
const char* transition_kind_name(TransitionKind kind);

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

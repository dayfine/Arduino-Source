/*  BDSP AutoStory - Story simulation scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "PokemonBDSP_AutoStory_StorySim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace detail{
ImageStats sim_screen(double brightness, double stddev){
    return ImageStats(FloatPixel(brightness, brightness, brightness), FloatPixel(stddev, stddev, stddev), 1);
}

std::vector<Pokemon::TransitionSample> sim_fade_frames(double floor, double fade, bool hold_seen){
    constexpr Milliseconds FRAME = 33ms;
    //  The fall and the rise: halfway, in brightness and in texture.
    const ImageStats between = sim_screen((floor + fade) / 2, (SIM_FLOOR_STDDEV + SIM_FADE_STDDEV) / 2);
    std::vector<ImageStats> looks{sim_screen(floor, SIM_FLOOR_STDDEV), between};
    if (hold_seen){
        for (size_t i = 0; i < 3; i++){
            looks.push_back(sim_screen(fade, SIM_FADE_STDDEV));
        }
        looks.push_back(between);
    }
    looks.push_back(sim_screen(floor, SIM_FLOOR_STDDEV));
    std::vector<Pokemon::TransitionSample> frames;
    for (size_t i = 0; i < looks.size(); i++){
        frames.push_back({.time = WallClock() + FRAME * (int64_t)i, .middle = looks[i]});
    }
    return frames;
}

WarpFade sim_watch_fade(const WarpFadeRule& rule, double floor, double fade, bool hold_seen){
    WarpFadeWatch watch(rule, [](const Pokemon::TransitionSample&, const Pokemon::TransitionSample&){
        return Pokemon::SceneChange::DIFFERENT;
    });
    for (const Pokemon::TransitionSample& frame : sim_fade_frames(floor, fade, hold_seen)){
        const WarpFade seen = watch.push(frame);
        if (seen != WarpFade::NONE){
            return seen;
        }
    }
    return WarpFade::NONE;
}

SimBeat sim_line(std::string what){
    return {.what = std::move(what), .line = true};
}
SimBeat sim_pause(std::string what, Milliseconds length, bool moving){
    return {.what = std::move(what), .length = length, .moving = moving};
}
SimBeat sim_walk_past(std::string what, Milliseconds length){
    return {.what = std::move(what), .length = length, .moving = true, .at_player = true};
}
SimBeat sim_fade(std::string what, Milliseconds length, WarpFade fade){
    return {.what = std::move(what), .length = length, .overworld = true, .moving = false, .fade = fade};
}

namespace{

//  run_settle()'s looks and presses on a scripted scene, on a clock of its
//  own: each press, held and released, moves it on.
class SceneSettleIO : public Pokemon::SettleIO{
public:
    explicit SceneSettleIO(const SimScene& scene)
        : m_scene(scene)
    {}

    virtual Pokemon::SettleLook look() override{
        while (m_beat < m_scene.size() && !m_scene[m_beat].line && m_now - m_beat_start >= m_scene[m_beat].length){
            m_beat_start += m_scene[m_beat].length;
            m_beat++;
        }
        ScreenEvents look{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL, ScreenEvent::PLAYER_STILL};
        if (m_beat < m_scene.size()){
            const SimBeat& shown = m_scene[m_beat];
            look = {};
            look.set(ScreenEvent::DIALOG_BOX, shown.line);
            look.set(ScreenEvent::PLAYER_CAN_MOVE, shown.overworld);
            look.set(ScreenEvent::SCREEN_STILL, !shown.moving);
            look.set(ScreenEvent::PLAYER_STILL, !shown.at_player);
            look.merge(fade_events(shown.fade));
        }
        return {look, m_now};
    }
    //  Either press closes a line; nothing else answers one.
    virtual void press(SceneMove, Milliseconds wait) override{
        const Milliseconds press = TALK_OVER_HOLD + wait;
        if (m_beat < m_scene.size() && m_scene[m_beat].line){
            m_beat++;
            m_beat_start = m_now + press;
        }
        m_now += press;
    }
    virtual bool flee_battle() override{
        return false;
    }
    virtual void log(std::string_view) override{}

    //  Where the scene is: "" past its last beat.
    std::string at() const{
        if (m_beat >= m_scene.size()){
            return "";
        }
        return std::format("{} ms in, at beat {} of {} ({})",
            std::chrono::duration_cast<Milliseconds>(m_now - WallClock()).count(), m_beat + 1, m_scene.size(), m_scene[m_beat].what);
    }

private:
    const SimScene& m_scene;
    WallClock m_now = WallClock();
    WallClock m_beat_start = WallClock();
    size_t m_beat = 0;
};

} // namespace

std::string play_scene(const SimScene& scene, const Pokemon::SettleRule& rule){
    SceneSettleIO io(scene);
    const Pokemon::SettleEnd end = Pokemon::run_settle(rule, "scene", io);
    switch (end.result){
    case Pokemon::SettleEnd::Result::QUIET:
        return io.at().empty() ? "" : std::format("judged over {}", io.at());
    case Pokemon::SettleEnd::Result::BATTLE:
        return std::format("read a battle {}", io.at());
    case Pokemon::SettleEnd::Result::TIMED_OUT:
        break;
    }
    return std::format("never judged over: {}, {}", io.at(), end.text);
}
} // namespace detail


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

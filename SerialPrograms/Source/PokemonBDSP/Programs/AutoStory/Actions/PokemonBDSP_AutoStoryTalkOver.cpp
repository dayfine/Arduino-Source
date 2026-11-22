/*  BDSP AutoStory - Talk-over
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <atomic>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/ScreenEvents/ScreenFades.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStoryScreenEvents.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkScreen.h"
#include "PokemonBDSP_AutoStoryTalks.h"
#include "PokemonBDSP_AutoStoryWildBattle.h"
#include "PokemonBDSP_AutoStoryTalkOver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  Watches every frame during a settle press wait so short (< 1 s) map-change
//  fades (e.g. Mom's walk out of PlayerHouse1F in Japanese) cannot hide between
//  1200 ms look() snapshots. Always returns false so press timing is unchanged.
class WaitFadeRecorder : public VisualInferenceCallback{
public:
    WaitFadeRecorder()
        : VisualInferenceCallback("WaitFadeRecorder")
    {}

    virtual void make_overlays(VideoOverlaySet& items) const override{}
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        if (m_fade.load(std::memory_order_relaxed) == WarpFade::NONE){
            const WarpFade seen = fade_of_screen(frame);
            if (seen != WarpFade::NONE){
                m_fade.store(seen, std::memory_order_relaxed);
            }
        }
        return false;
    }

    WarpFade fade() const{
        return m_fade.load(std::memory_order_relaxed);
    }

private:
    std::atomic<WarpFade> m_fade{WarpFade::NONE};
};

//  run_settle()'s looks and presses on the console.
class ConsoleSettleIO : public Pokemon::SettleIO{
public:
    ConsoleSettleIO(VideoStream& stream, ProControllerContext& context, const TalkOverSpec& spec)
        : m_stream(stream)
        , m_context(context)
        , m_spec(spec)
        , m_eyes(spec.watch == TalkOverWatch::TRAINER ? BattleCue::TRAINER : BattleCue::WILD_MENU, spec.banner)
    {}

    virtual Pokemon::SettleLook look() override{
        m_context.wait_for_all_requests();
        const WallClock now = current_time();
        ScreenEvents events = m_eyes.observe(m_stream.video().snapshot(), now);
        if (m_fade_during_wait != WarpFade::NONE && fade_in(events) == WarpFade::NONE){
            events.merge(fade_events(m_fade_during_wait));
        }
        m_fade_during_wait = WarpFade::NONE;
        return {.events = events, .when = now, .player_still = m_eyes.player_still()};
    }
    virtual void press(SceneMove move, Milliseconds wait) override{
        if (move == SceneMove::NEXT){
            m_saw_dialogue = true;
        }
        const Button clear_button = m_spec.clear == BUTTON_A && in_pokemon_center(m_stream)
            ? BUTTON_B : m_spec.clear;
        const Button button = move == SceneMove::NEXT ? m_spec.next : clear_button;
        if (!m_spec.settle.judge.warp || wait <= Milliseconds(0)){
            pbf_press_button(m_context, button, TALK_OVER_HOLD, wait);
            return;
        }
        pbf_press_button(m_context, button, TALK_OVER_HOLD, Milliseconds(0));
        WaitFadeRecorder recorder;
        run_until<ProControllerContext>(
            m_stream, m_context,
            [wait](ProControllerContext& context){
                pbf_wait(context, wait);
            },
            {{recorder}}
        );
        if (recorder.fade() != WarpFade::NONE){
            m_fade_during_wait = recorder.fade();
        }
    }
    bool saw_dialogue() const{
        return m_saw_dialogue;
    }
    virtual bool flee_battle() override{
        if (m_spec.watch == TalkOverWatch::TRAINER){
            return false;
        }
        run_from_wild_battle(m_stream, m_context);
        return true;
    }
    virtual void log(std::string_view line) override{
        m_stream.log(std::string(line));
    }

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    const TalkOverSpec& m_spec;
    BdspScreenObserver m_eyes;
    bool m_saw_dialogue = false;
    WarpFade m_fade_during_wait = WarpFade::NONE;
};

} // namespace


TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec){
    ConsoleSettleIO io(stream, context, spec);
    const Pokemon::SettleEnd end = Pokemon::run_settle(spec.settle, label, io);
    switch (end.result){
    case Pokemon::SettleEnd::Result::QUIET:
        return io.saw_dialogue() ? TalkOverEnd::DIALOGUE : TalkOverEnd::QUIET;
    case Pokemon::SettleEnd::Result::BATTLE:
        return TalkOverEnd::BATTLE;
    case Pokemon::SettleEnd::Result::TIMED_OUT:
        break;
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, end.text, stream);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

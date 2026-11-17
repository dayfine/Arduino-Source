/*  BDSP AutoStory - Talk-over
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_ScreenEvents.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_TalkOver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

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
        return {m_eyes.observe(m_stream.video().snapshot(), now), now};
    }
    virtual void press(SceneMove move, Milliseconds wait) override{
        pbf_press_button(m_context, move == SceneMove::NEXT ? m_spec.next : BUTTON_B, TALK_OVER_HOLD, wait);
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
};

} // namespace


TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec){
    ConsoleSettleIO io(stream, context, spec);
    const Pokemon::SettleEnd end = Pokemon::run_settle(spec.settle, label, io);
    switch (end.result){
    case Pokemon::SettleEnd::Result::QUIET:
        return TalkOverEnd::QUIET;
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

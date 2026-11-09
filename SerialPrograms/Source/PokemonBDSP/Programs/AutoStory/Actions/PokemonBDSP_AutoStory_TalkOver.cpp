/*  BDSP AutoStory - Talk-over
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_TalkOver.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


SceneMove TalkOver::look(const SceneLook& look, WallClock now){
    if (look.battle){
        m_quiet_since = WallClock::max();
        m_calm_since = WallClock::max();
        return SceneMove::BATTLE;
    }
    if (look.fade != WarpFade::NONE){
        m_fade = look.fade;
    }
    m_banner |= look.banner;
    const bool calm = !look.box && look.overworld && warped();
    const bool quiet = calm && look.still;
    if (!calm){
        m_calm_since = WallClock::max();
    }else if (m_calm_since == WallClock::max()){
        m_calm_since = now;
    }
    if (!quiet){
        m_quiet_since = WallClock::max();
    }else if (m_quiet_since == WallClock::max()){
        m_quiet_since = now;
    }
    const bool over =
        (quiet && now - m_quiet_since >= TALK_OVER_QUIET) ||
        (calm && now - m_calm_since >= TALK_OVER_CALM);
    if (over && m_rule.end_on_quiet){
        return SceneMove::DONE;
    }
    return look.box ? SceneMove::NEXT : SceneMove::B;
}

bool TalkOver::warped() const{
    return !m_rule.warp || m_fade != WarpFade::NONE || m_banner;
}

std::string TalkOver::describe() const{
    if (m_fade == WarpFade::NONE && !m_banner){
        return "no fade or banner";
    }
    std::string out = m_fade == WarpFade::NONE ? "" : warp_fade_name(m_fade);
    if (m_banner){
        out += out.empty() ? "banner read" : ", banner read";
    }
    return out;
}


namespace{

//  The detectors talk_over() looks with.
class SceneEyes{
public:
    SceneEyes(TalkOverWatch watch, const std::string& banner)
        : m_watch(watch)
        , m_banner(banner)
        , m_wild_menu(BattleType::STANDARD)
        , m_trainer_menu(BattleType::TRAINER)
    {}

    SceneLook look(const VideoSnapshot& frame, WallClock now){
        SceneLook out;
        if (!frame){
            m_previous = VideoSnapshot();
            return out;
        }
        const ImageViewRGB32& screen = *frame.frame;
        out.battle = m_wild_menu.detect(screen) || (m_watch == TalkOverWatch::TRAINER &&
            (m_trainer_menu.detect(screen) || m_black.detect(screen)));
        out.box = m_dialog.detect(screen);
        out.overworld = m_overworld.detect(screen);
        out.still = m_previous && walk_screen_change(*m_previous.frame, screen) < WALK_TAP_BLOCKED_CHANGE;
        out.fade = fade_of_screen(screen);
        out.banner = !m_banner.empty() && location_banner_matches(m_reads.read(screen, now), m_banner);
        m_previous = frame;
        return out;
    }

private:
    TalkOverWatch m_watch;
    const std::string& m_banner;
    ShortDialogDetector m_dialog;
    OverworldDetector m_overworld;
    BattleMenuDetector m_wild_menu;
    BattleMenuDetector m_trainer_menu;
    BlackScreenDetector m_black;
    LocationBannerReads m_reads;
    VideoSnapshot m_previous;
};

} // namespace

TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec){
    const WallClock deadline = current_time() + TALK_OVER_LIMIT;
    TalkOver judge(spec.rule);
    SceneEyes eyes(spec.watch, spec.banner);
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const WallClock now = current_time();
        switch (judge.look(eyes.look(stream.video().snapshot(), now), now)){
        case SceneMove::DONE:
            stream.log(std::format("[Talk-over] {}: over; {}.", label, judge.describe()));
            return TalkOverEnd::QUIET;
        case SceneMove::BATTLE:
            if (spec.watch == TalkOverWatch::TRAINER){
                stream.log("[Talk-over] " + label + ": a battle started.");
                return TalkOverEnd::BATTLE;
            }
            stream.log("[Talk-over] " + label + ": in a wild battle; fleeing.");
            run_from_wild_battle(stream, context);
            continue;
        case SceneMove::NEXT:
            pbf_press_button(context, spec.next, TALK_OVER_HOLD, TALK_OVER_RELEASE);
            continue;
        case SceneMove::B:
            pbf_press_button(context, BUTTON_B, TALK_OVER_HOLD, TALK_OVER_RELEASE);
            continue;
        }
    }
    const long long seconds = std::chrono::duration_cast<std::chrono::seconds>(TALK_OVER_LIMIT).count();
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        !judge.warped()
            ? std::format("{}: no fade or banner within {} s, so we are not where the step goes (map mismatch).", label, seconds)
            : !spec.rule.end_on_quiet
            ? std::format("{}: no battle within {} s of the talk.", label, seconds)
            : std::format("{}: not back on a quiet overworld within {} s.", label, seconds),
        stream);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

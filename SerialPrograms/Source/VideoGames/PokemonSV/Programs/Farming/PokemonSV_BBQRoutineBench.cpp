/*  BBQ Routine Bench
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <fstream>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQLab.h"
#include "PokemonSV_BBQRoutineBench.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;


std::string bbq_bench_summary(const std::vector<BBQBenchRep>& reps){
    if (reps.empty()){
        return "0/0";
    }
    size_t passes = std::count_if(reps.begin(), reps.end(), [](const BBQBenchRep& rep){ return rep.pass; });
    std::vector<double> seconds;
    for (const BBQBenchRep& rep : reps){
        seconds.push_back(rep.seconds);
    }
    std::sort(seconds.begin(), seconds.end());
    size_t mid = seconds.size() / 2;
    double median = seconds.size() % 2 ? seconds[mid] : (seconds[mid - 1] + seconds[mid]) / 2;
    return std::to_string(passes) + "/" + std::to_string(reps.size())
        + ", median " + std::to_string((int)(median + 0.5)) + " s";
}


std::string bbq_bench_arm_summary(const std::vector<BBQBenchRep>& reps, char arm){
    std::vector<BBQBenchRep> arm_reps;
    for (const BBQBenchRep& rep : reps){
        if (rep.arm == arm){
            arm_reps.push_back(rep);
        }
    }
    return bbq_bench_summary(arm_reps);
}


BBQRoutineBench_Descriptor::BBQRoutineBench_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonSV:BBQRoutineBench",
        STRING_POKEMON + " SV", "BBQ Routine Bench",
        "",
        "Lab: run one BBQ quest routine N times and judge each run.",
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS,
        PanelDeprecation::NOT_DEPRECATED,
        {}
    )
{}
struct BBQRoutineBench_Descriptor::Stats : public StatsTracker{
    Stats()
        : reps(m_stats["Reps"])
        , passes(m_stats["Passes"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Reps");
        m_display_order.emplace_back("Passes");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }
    std::atomic<uint64_t>& reps;
    std::atomic<uint64_t>& passes;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> BBQRoutineBench_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


BBQRoutineBench::BBQRoutineBench()
    : ROUTINE(
        "<b>Routine:</b>",
        {
            {BBQBenchRoutine::SneakUp,  "sneak-up",     "Sneak up (pass: a battle starts)"},
            {BBQBenchRoutine::TeraSelf, "tera-self",    "Tera-self (pass: a battle starts and ends without error)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BBQBenchRoutine::SneakUp
    )
    , REPS(
        "<b>Reps:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        10, 1
    )
    , START_FROM(
        "<b>Start every rep from:</b><br>Back to back runs reps as the farmer does, so a build "
        "that stays away flies its next route directly.",
        {
            {BBQBenchStart::BackToBack, "back-to-back", "Where the last rep ended"},
            {BBQBenchStart::Central,    "central",      "Central Plaza"},
            {BBQBenchStart::PolarRest,  "polar-rest",   "Polar Rest Area (fly in from another region)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BBQBenchStart::BackToBack
    )
    , VARIANT(
        "<b>A/B variant:</b><br>Odd reps run A, even reps run B.",
        {
            {BBQBenchVariant::None,             "none",                 "None"},
            {BBQBenchVariant::TeraSelfSpot,     "tera-self-spot",       "Tera-self: A Pride Rock, B Chargestone court"},
            {BBQBenchVariant::DirectLeaveArea,  "direct-leave-area",    "A return to Central, B fly directly and leave the spot before the day skip"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BBQBenchVariant::None
    )
{
    PA_ADD_OPTION(ROUTINE);
    PA_ADD_OPTION(REPS);
    PA_ADD_OPTION(START_FROM);
    PA_ADD_OPTION(VARIANT);
    PA_ADD_OPTION(BBQ_OPTIONS);
}

namespace{

// Records whether a battle menu showed while a routine ran, without stopping it.
class BattleSeenWatcher : public VisualInferenceCallback{
public:
    BattleSeenWatcher()
        : VisualInferenceCallback("BattleSeenWatcher")
        , m_detector(COLOR_YELLOW)
    {}
    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_detector.make_overlays(items);
    }
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        if (m_detector.detect(frame)){
            m_seen = true;
        }
        return false;
    }
    bool seen() const{
        return m_seen;
    }

private:
    NormalBattleMenuDetector m_detector;
    std::atomic<bool> m_seen{false};
};

// A wild Pokemon often attacks at Pride Rock right after a day skip. Flee it, so the next
// rep or flight starts in the overworld. Returns true if there was a battle.
bool flee_ambush(SingleSwitchProgramEnvironment& env, ProControllerContext& context, uint16_t rep){
    NormalBattleMenuWatcher battle(COLOR_YELLOW);
    context.wait_for_all_requests();
    int ret = wait_until(env.console, context, std::chrono::seconds(2), {battle});
    if (ret < 0){
        return false;
    }
    env.log("BENCH,ambush_before_rep," + std::to_string(rep));
    run_from_battle(env.console, context);
    return true;
}

// What the routine reported during one rep: when each event happened, the sky at the
// end of the route, the opponent, and a screenshot at the moments that decide a rep.
class RepRecorder{
public:
    RepRecorder(SingleSwitchProgramEnvironment& env, Language language, std::string dir, uint16_t rep)
        : m_env(env), m_language(language), m_dir(std::move(dir)), m_rep(rep), m_start(current_time())
    {}

    void on_event(VideoStream& stream, ProControllerContext& context, const std::string& event){
        const double t = std::chrono::duration<double>(current_time() - m_start).count();
        m_events += (m_events.empty() ? "" : ";") + event + "@" + std::to_string((int)(t + 0.5));
        if (event == "battle"){
            m_battle = true;
            try{
                for (const std::string& slug : read_singles_opponent(m_env.program_info(), stream, context, m_language)){
                    m_opponent += (m_opponent.empty() ? "" : "|") + slug;
                }
            }catch (OperationFailedException&){
                m_opponent = "unread";
            }
        }
        if (event == "tera_turn"){
            m_tera_turn = true;
            //Let the Terastallization show before the screenshot.
            context.wait_for(std::chrono::seconds(4));
        }
        if (event == "route_done"){
            VideoSnapshot screen = stream.video().snapshot();
            ImageStats sky = image_stats(extract_box_reference(screen, ImageFloatBox(0.0, 0.0, 1.0, 0.25)));
            m_sky = (int)((sky.average.r + sky.average.g + sky.average.b) / 3 + 0.5);
        }
        if (event == "route_done" || event == "no_battle" || event == "tera_turn" || event == "ambush"){
            save_screenshot(stream, event);
        }
    }

    bool battle() const{ return m_battle; }
    bool tera_turn() const{ return m_tera_turn; }
    const std::string& events() const{ return m_events; }
    const std::string& opponent() const{ return m_opponent; }
    int sky() const{ return m_sky; }

private:
    void save_screenshot(VideoStream& stream, const std::string& event){
        char name[32];
        snprintf(name, sizeof(name), "rep%02u_", (unsigned)m_rep);
        VideoSnapshot screen = stream.video().snapshot();
        if (screen.frame){
            screen.frame->save(m_dir + "/" + name + event + ".png");
        }
    }

    SingleSwitchProgramEnvironment& m_env;
    Language m_language;
    std::string m_dir;
    uint16_t m_rep;
    WallClock m_start;
    std::string m_events;
    std::string m_opponent;
    int m_sky = -1;
    bool m_battle = false;
    bool m_tera_turn = false;
};

// Set the lab flags for one rep's arm.
void set_arm_flags(BBQBenchVariant variant, char arm){
    BBQLabFlags& flags = bbq_lab_flags();
    flags = BBQLabFlags();
    switch (variant){
    case BBQBenchVariant::None:
        return;
    case BBQBenchVariant::TeraSelfSpot:
        flags.tera_self_at_chargestone = arm == 'B';
        return;
    case BBQBenchVariant::DirectLeaveArea:
        flags.fly_directly_override = arm == 'B' ? 1 : 0;
        flags.leave_area_before_day_skip = arm == 'B';
        return;
    }
}

// SHA-256 of the running executable, so every rep names the exact build.
std::string executable_sha256(){
    QFile file(QCoreApplication::applicationFilePath());
    if (!file.open(QIODevice::ReadOnly)){
        return "unknown";
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return hash.result().toHex().toStdString();
}

}


void BBQRoutineBench::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);
    BBQRoutineBench_Descriptor::Stats& stats = env.current_stats<BBQRoutineBench_Descriptor::Stats>();

    if (env.console.state().console_type() == ConsoleType::Unknown){
        throw UserSetupError(env.console, "Console Type (Switch 1 or 2) must be specified.");
    }
    require_player(env.console, context, BUTTON_LCLICK);

    //Start on the Central Plaza fly spot, as the farmer does. Off a fly spot (a block that
    //stopped at Pride Rock), fly there first.
    open_map_from_overworld(env.program_info(), env.console, context);
    if (!fly_to_overworld_from_map(env.program_info(), env.console, context, true)){
        env.log("No fly spot here. Flying to Central Plaza.");
        press_Bs_to_back_to_overworld(env.program_info(), env.console, context);
        return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
    }

    const std::string routine = ROUTINE == BBQBenchRoutine::SneakUp ? "sneak-up" : "tera-self";
    const std::string exe_sha256 = executable_sha256();
    const std::string run_id = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss").toStdString();
    const std::string dir = QDir::currentPath().toStdString() + "/BBQBench/" + run_id;
    QDir().mkpath(QString::fromStdString(dir));
    std::ofstream csv(dir + "/reps.csv");
    csv << "run,exe_sha256,routine,variant,start_from,rep,arm,pass,seconds,reason,battle,tera_turn,opponent,sky,events\n";
    env.log("BENCH,start,routine=" + routine
        + ",reps=" + std::to_string(REPS)
        + ",start_from=" + std::to_string((int)START_FROM.get())
        + ",variant=" + std::to_string((int)VARIANT.get())
        + ",dir=" + dir
        + ",exe_sha256=" + exe_sha256);

    std::vector<BBQBenchRep> reps;
    for (uint16_t i = 0; i < REPS; i++){
        flee_ambush(env, context, i + 1);
        switch (START_FROM){
        case BBQBenchStart::BackToBack:
            break;
        case BBQBenchStart::Central:
            ensure_at_plaza(env.program_info(), env.console, context);
            break;
        case BBQBenchStart::PolarRest:
            //Flies from wherever the player is. Returning to Central first would fail on
            //the first rep, where the plaza's icon is hidden under the player.
            central_to_polar_rest(env.program_info(), env.console, context);
            stay_away_from_plaza(BBQ_OPTIONS.LANGUAGE);
            break;
        }
        const char arm = VARIANT == BBQBenchVariant::None ? '-' : (i % 2 == 0 ? 'A' : 'B');
        set_arm_flags(VARIANT, arm);
        RepRecorder recorder(env, BBQ_OPTIONS.LANGUAGE, dir, i + 1);
        set_bbq_lab_event_hook([&](VideoStream& stream, ProControllerContext& context, const std::string& event){
            recorder.on_event(stream, context, event);
        });
        BattleSeenWatcher battle;
        WallClock start = current_time();
        std::string reason;
        bool error = false;
        try{
            run_until<ProControllerContext>(
                env.console, context,
                [&](ProControllerContext& context){
                    switch (ROUTINE){
                    case BBQBenchRoutine::SneakUp:
                        quest_sneak_up(env.program_info(), env.console, context, BBQ_OPTIONS);
                        break;
                    case BBQBenchRoutine::TeraSelf:
                        quest_tera_self_defeat(env.program_info(), env.console, context, BBQ_OPTIONS);
                        break;
                    }
                },
                {battle}
            );
            reason = recorder.battle() ? "battle" : battle.seen() ? "battle outside the route" : "no battle";
        }catch (OperationFailedException& e){
            error = true;
            reason = "error: " + e.message();
            stats.errors++;
            press_Bs_to_back_to_overworld(env.program_info(), env.console, context);
            return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
        }
        set_bbq_lab_event_hook(nullptr);
        double seconds = std::chrono::duration<double>(current_time() - start).count();
        //The route's own battle counts, not a wild Pokemon attacking elsewhere. Tera-self
        //also needs its Tera turn.
        bool pass = recorder.battle() && !error
            && (ROUTINE != BBQBenchRoutine::TeraSelf || recorder.tera_turn());
        reps.push_back({pass, seconds, arm});

        stats.reps++;
        if (pass){
            stats.passes++;
        }
        env.update_stats();
        std::replace(reason.begin(), reason.end(), ',', ';');
        env.log("BENCH,rep," + std::to_string(i + 1)
            + ",arm=" + std::string(1, arm)
            + ",pass=" + std::string(pass ? "1" : "0")
            + ",seconds=" + std::to_string((int)(seconds + 0.5))
            + ",reason=" + reason
            + ",opponent=" + recorder.opponent()
            + ",sky=" + std::to_string(recorder.sky())
            + ",events=" + recorder.events());
        csv << run_id << "," << exe_sha256 << "," << routine << "," << (int)VARIANT.get() << ","
            << (int)START_FROM.get() << "," << (i + 1) << "," << arm << "," << (pass ? 1 : 0) << ","
            << (int)(seconds + 0.5) << "," << reason << "," << (recorder.battle() ? 1 : 0) << ","
            << (recorder.tera_turn() ? 1 : 0) << "," << recorder.opponent() << "," << recorder.sky() << ","
            << recorder.events() << "\n";
        csv.flush();
    }
    set_arm_flags(BBQBenchVariant::None, '-');
    env.log("BENCH,summary,routine=" + routine + "," + bbq_bench_summary(reps));
    if (VARIANT != BBQBenchVariant::None){
        env.log("BENCH,summary_arm,A," + bbq_bench_arm_summary(reps, 'A'));
        env.log("BENCH,summary_arm,B," + bbq_bench_arm_summary(reps, 'B'));
    }

    //Leave the player at Central Plaza, where the next block starts. A routine that
    //already returned leaves the player there, where a second return can't find the
    //plaza's icon under the player.
    ensure_at_plaza(env.program_info(), env.console, context);
}


}
}
}

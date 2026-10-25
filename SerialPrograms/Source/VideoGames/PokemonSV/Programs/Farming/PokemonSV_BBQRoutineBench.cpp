/*  BBQ Routine Bench
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.h"
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
{
    PA_ADD_OPTION(ROUTINE);
    PA_ADD_OPTION(REPS);
    PA_ADD_OPTION(START_FROM);
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

    //Start on the Central Plaza fly spot, as the farmer does.
    open_map_from_overworld(env.program_info(), env.console, context);
    if (!fly_to_overworld_from_map(env.program_info(), env.console, context, true)){
        throw UserSetupError(env.console, "No fly spot where the player stands. Start the program at Central Plaza.");
    }

    const std::string routine = ROUTINE == BBQBenchRoutine::SneakUp ? "sneak-up" : "tera-self";
    env.log("BENCH,start,routine=" + routine
        + ",reps=" + std::to_string(REPS)
        + ",start_from=" + std::to_string((int)START_FROM.get())
        + ",exe_sha256=" + executable_sha256());

    std::vector<BBQBenchRep> reps;
    for (uint16_t i = 0; i < REPS; i++){
        switch (START_FROM){
        case BBQBenchStart::BackToBack:
            break;
        case BBQBenchStart::Central:
            if (i > 0){
                return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
            }
            break;
        case BBQBenchStart::PolarRest:
            return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
            central_to_polar_rest(env.program_info(), env.console, context);
            stay_away_from_plaza(BBQ_OPTIONS.LANGUAGE);
            break;
        }
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
            reason = battle.seen() ? "battle" : "no battle";
        }catch (OperationFailedException& e){
            error = true;
            reason = "error: " + e.message();
            stats.errors++;
            press_Bs_to_back_to_overworld(env.program_info(), env.console, context);
            return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
        }
        double seconds = std::chrono::duration<double>(current_time() - start).count();
        bool pass = battle.seen() && !error;
        reps.push_back({pass, seconds});

        stats.reps++;
        if (pass){
            stats.passes++;
        }
        env.update_stats();
        std::replace(reason.begin(), reason.end(), ',', ';');
        env.log("BENCH,rep," + std::to_string(i + 1)
            + ",pass=" + std::string(pass ? "1" : "0")
            + ",seconds=" + std::to_string((int)(seconds + 0.5))
            + ",reason=" + reason);
    }
    env.log("BENCH,summary,routine=" + routine + "," + bbq_bench_summary(reps));

    //Leave the player at Central Plaza, where the next block starts.
    return_to_plaza(env.program_info(), env.console, context, BBQ_OPTIONS.LANGUAGE);
}


}
}
}

/*  Program Environment
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/ProgramSession.h"
#include "CommonFramework/Notifications/ProgramInfo.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "ProgramEnvironment.h"


namespace PokemonAutomation{





ProgramEnvironment::ProgramEnvironment(
    const ProgramInfo& program_info,
    ProgramSession& session,
    StatsTracker* current_stats,
    const StatsTracker* historical_stats
)
    : m_program_info(program_info)
    , m_session(session)
    , m_logger(session.logger())
    , m_current_stats(current_stats)
    , m_historical_stats(historical_stats)
{}

const ProgramInfo& ProgramEnvironment::program_info() const{
    return m_program_info;
}


void report_start_check_failure(ProgramEnvironment& env, std::string check, std::string message){
    env.log(std::format("[StartCheck] {}: {}", check, message), COLOR_RED);
    env.m_session.m_transitions.start_check_failed(std::move(check), std::move(message), current_time());
}

void report_run_checkpoint(ProgramEnvironment& env, std::string checkpoint){
    env.m_session.m_transitions.set_checkpoint(std::move(checkpoint));
}


void ProgramEnvironment::update_stats(){
    m_session.report_stats_changed();
    if (m_current_stats){
        m_logger.log("Current Stats: " + m_current_stats->to_str(StatsTracker::PrintMode::DUMP));
    }
}


std::string ProgramEnvironment::historical_stats_str() const{
    return m_historical_stats ? m_historical_stats->to_str(StatsTracker::DISPLAY_ON_SCREEN) : "";
}




}

/*  Step Trace
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The step log of a screen machine (ScreenMachine.h): every change of
 *  recognized screen is logged once, numbered, with its time since the first
 *  screen, so a run's log shows the screens that led to where it ended.
 *
 */

#ifndef PokemonAutomation_CommonTools_StepTrace_H
#define PokemonAutomation_CommonTools_StepTrace_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{


//  Logs the screens a routine recognizes, each change once.
class StepTrace{
public:
    //  Lines go to `log`, each starting "[<label>]".
    StepTrace(std::string label, std::function<void(const std::string& line)> log);

    //  Records the screen the routine recognized at `when`. Logs only when
    //  it differs from the previous one: "[label] screen 2: dialog at 1.5s".
    void screen(const std::string& name, WallClock when);

private:
    std::function<void(const std::string& line)> m_log;
    std::string m_label;
    //  The time of the first screen; the lines' times count from it.
    std::optional<WallClock> m_start;
    size_t m_count = 0;
    std::string m_last;
};



} // namespace PokemonAutomation
#endif

/*  Scripted Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A loop that polls the screen, waits and reads it again, written against
 *  ReadingSource so a test feeds it scripted readings on a virtual clock
 *  instead of a console's frames. The program's source waits on the
 *  controller and reads a snapshot; ScriptedReadings shows each scripted
 *  reading for its time, and the last one after the script ends.
 *
 *  A reading is whatever the loop decides on, read from one frame (a
 *  game's struct of detector results). FrameReplay (Pokemon_FrameReplay.h)
 *  replays recorded frames through the detectors instead; scripted
 *  readings test the loop's decisions without images.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScriptedReadings_H
#define PokemonAutomation_Pokemon_ScriptedReadings_H

#include <optional>
#include <utility>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where a polling loop gets its readings, and its clock.
template <typename Reading>
class ReadingSource{
public:
    virtual ~ReadingSource() = default;

    //  Lets `duration` pass: nothing pressed.
    virtual void wait(Milliseconds duration) = 0;
    //  The screen now; none if no frame came.
    virtual std::optional<Reading> read() = 0;
    //  The source's clock.
    virtual WallClock now() const = 0;
};


//  One step of a script: `reading` for `lasts`. No reading: no frame.
template <typename Reading>
struct ScriptedReading{
    Milliseconds lasts = Milliseconds(0);
    std::optional<Reading> reading = {};
};

//  Scripted readings on a virtual clock that only wait() moves. A reading
//  shows from the end of the one before it for its `lasts`; the last one
//  shows on after the script ends. An empty script reads no frame.
template <typename Reading>
class ScriptedReadings : public ReadingSource<Reading>{
public:
    ScriptedReadings(std::vector<ScriptedReading<Reading>> script, WallClock origin)
        : m_script(std::move(script))
        , m_origin(origin)
    {}

    //  How long the source has waited in all.
    Milliseconds elapsed() const{
        return m_elapsed;
    }

    virtual void wait(Milliseconds duration) override{
        m_elapsed += duration;
    }
    virtual std::optional<Reading> read() override{
        Milliseconds end(0);
        for (const ScriptedReading<Reading>& step : m_script){
            end += step.lasts;
            if (m_elapsed < end){
                return step.reading;
            }
        }
        if (m_script.empty()){
            return std::nullopt;
        }
        return m_script.back().reading;
    }
    virtual WallClock now() const override{
        return m_origin + m_elapsed;
    }

private:
    std::vector<ScriptedReading<Reading>> m_script;
    WallClock m_origin;
    Milliseconds m_elapsed = Milliseconds(0);
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

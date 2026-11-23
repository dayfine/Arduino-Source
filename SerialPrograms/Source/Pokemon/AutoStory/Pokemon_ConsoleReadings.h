/*  Console Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The program's ReadingSource (Pokemon_ScriptedReadings.h): a wait on the
 *  controller, then the console's latest frame through a game's reader.
 *
 */

#ifndef PokemonAutomation_Pokemon_ConsoleReadings_H
#define PokemonAutomation_Pokemon_ConsoleReadings_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon_ScriptedReadings.h"

namespace PokemonAutomation{
namespace Pokemon{


//  `reader` on the console's frames, on the wall clock.
template <typename Reading>
class ConsoleReadings : public ReadingSource<Reading>{
public:
    using Reader = Reading (*)(const ImageViewRGB32& screen);

    ConsoleReadings(NintendoSwitch::ConsoleHandle& console, NintendoSwitch::ProControllerContext& context, Reader reader)
        : m_console(console)
        , m_context(context)
        , m_reader(reader)
    {}

    virtual void wait(Milliseconds duration) override{
        NintendoSwitch::pbf_wait(m_context, duration);
        m_context.wait_for_all_requests();
    }
    virtual std::optional<Reading> read() override{
        VideoSnapshot screen = m_console.video().snapshot();
        if (!screen){
            return std::nullopt;
        }
        return m_reader(screen);
    }
    virtual WallClock now() const override{
        return current_time();
    }

private:
    NintendoSwitch::ConsoleHandle& m_console;
    NintendoSwitch::ProControllerContext& m_context;
    Reader m_reader;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

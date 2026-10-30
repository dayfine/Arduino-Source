/*  BDSP AutoStory - Segment 01 Shared
 *  Helpers used by more than one Segment 01 checkpoint.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment01_Shared_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment01_Shared_H

#include <string>
#include <vector>

#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

[[noreturn]] void fail(VideoStream& stream, const std::string& message);

bool dialogue_opens(VideoStream& stream, ProControllerContext& context, Milliseconds timeout);

bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, bool battle_expected = false, bool doubles = false);

void hold_tiles(VideoStream& stream, ProControllerContext& context, Pokemon::Direction dir, size_t tiles);

WalkResult walk_dir(VideoStream& stream, ProControllerContext& context, Pokemon::Direction dir, Milliseconds limit, const std::string& label);

bool in_pokemon_center(VideoStream& stream);


//  Logs a checkpoint's steps up front, then each step as it starts, with the
//  one after it, in the log and the overlay.
class StepLog{
public:
    StepLog(VideoStream& stream, std::string checkpoint, std::string intro, std::vector<std::string> steps)
        : m_stream(stream), m_checkpoint(std::move(checkpoint)), m_steps(std::move(steps))
    {
        std::string text = "[" + m_checkpoint + "] " + intro + ", the steps are:";
        for (size_t i = 0; i < m_steps.size(); i++){
            text += "\n    " + std::to_string(i + 1) + ". " + m_steps[i];
        }
        m_stream.log(text, COLOR_BLUE);
    }
    void operator()(size_t number){
        const std::string count = std::to_string(number) + "/" + std::to_string(m_steps.size());
        const bool last = number >= m_steps.size();
        m_stream.log("[" + m_checkpoint + " step " + count + "] " + m_steps[number - 1] +
            (last ? std::string(" -- last step") : " -- next: " + m_steps[number]), COLOR_BLUE);
        add_wrapped_log(m_stream.overlay(), m_checkpoint + " step " + count + ": " + m_steps[number - 1], COLOR_WHITE);
        if (!last){
            add_wrapped_log(m_stream.overlay(), "Next: " + m_steps[number], COLOR_GRAY);
        }
    }

private:
    VideoStream& m_stream;
    std::string m_checkpoint;
    std::vector<std::string> m_steps;
};

}
}
}
#endif

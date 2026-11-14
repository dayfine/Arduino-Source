#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonSV/Inference/Picnics/PokemonSV_PicnicDetector.h"
#include "PokemonSV_SandwichEating.h"
#include "PokemonSV_SandwichEatingConsole.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{
class ConsoleSandwichEating : public SandwichEatingActions{
public:
    ConsoleSandwichEating(VideoStream& stream, ProControllerContext& context)
        : m_stream(stream), m_context(context){}
    virtual uint64_t now_ms() const override{
        return (uint64_t)std::chrono::duration_cast<Milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    virtual bool confirm_and_observe(uint64_t observation_ms) override{
        const WallClock deadline = current_time() + Milliseconds(observation_ms);
        const int ret = run_until<ProControllerContext>(m_stream, m_context,
            [&](ProControllerContext& context){
                pbf_press_button(context, BUTTON_A, 160ms, 0ms);
                context.wait_for_all_requests();
                // Command completion alone does not establish elapsed game transition time.
                context.wait_until(deadline);
            }, {m_picnic});
        return ret == 0;
    }
private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    PicnicWatcher m_picnic;
};
}

bool wait_for_sandwich_picnic(VideoStream& stream, ProControllerContext& context){
    ConsoleSandwichEating actions(stream, context);
    return wait_for_sandwich_picnic(actions);
}

}
}
}

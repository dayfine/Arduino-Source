/*  AutoStory Slope Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkMoves.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStorySlope.h"
#include "PokemonFRLG_AutoStorySlopeMessage.h"
#include "PokemonFRLG_AutoStorySlopeRun.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryStep;


namespace{

//  A battle's script and its end, back to the overworld.
constexpr Milliseconds SLOPE_BATTLE_EXIT = std::chrono::seconds(20);

Button slope_buttons(const SlopePress& press){
    Button buttons = BUTTON_NONE;
    if (press.b){
        buttons |= BUTTON_B;
    }
    if (press.a){
        buttons |= BUTTON_A;
    }
    return buttons;
}

//  The press under way `elapsed` after the first of `presses` began.
size_t press_at(const std::vector<SlopePress>& presses, Milliseconds elapsed){
    Milliseconds end(0);
    for (size_t c = 0; c < presses.size(); c++){
        end += presses[c].hold;
        if (elapsed < end){
            return c;
        }
    }
    return presses.empty() ? 0 : presses.size() - 1;
}

//  A white dialog that opens during a batch (SlopeMessageGate).
class SlopeMessageWatcher : public VisualInferenceCallback{
public:
    SlopeMessageWatcher()
        : VisualInferenceCallback("SlopeMessageWatcher")
        , m_detector(COLOR_RED)
    {}
    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_detector.make_overlays(items);
    }
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        return m_gate.opened(m_detector.detect(frame), timestamp);
    }

private:
    WhiteDialogDetector m_detector;
    SlopeMessageGate m_gate;
};

//  Sends `presses` back to back, one batch with no wait between, watching
//  for a battle and, if `message`, for a message. A battle's press is the
//  one under way by the time since the batch began.
SlopeSendResult send_slope_presses(SlopeEnv& env, const std::vector<SlopePress>& presses, bool message){
    SlopeMessageWatcher opened;
    BattleScreenWatcher battle(COLOR_RED);
    std::vector<PeriodicInferenceCallback> watchers{battle};
    if (message){
        watchers.push_back(opened);
    }
    const WallClock start = current_time();
    const int seen = run_until<ProControllerContext>(
        env.console, env.context,
        [&presses](ProControllerContext& context){
            for (const SlopePress& press : presses){
                const DpadPosition dpad = press.dpad ? OverworldWalkSteps::dpad(*press.dpad) : DPAD_NONE;
                pbf_controller_state(context, slope_buttons(press), dpad, {0, 0}, {0, 0}, press.hold);
            }
        },
        watchers
    );
    if (seen == 0){
        const Milliseconds elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start);
        return SlopeSendResult{.sent = SlopeSent::BATTLE, .press = press_at(presses, elapsed)};
    }
    return SlopeSendResult{.sent = seen == 1 ? SlopeSent::MESSAGE : SlopeSent::ALL};
}

std::string slope_end_name(SlopeEnd end){
    switch (end){
    case SlopeEnd::DONE:        return "done";
    case SlopeEnd::NOT_FOUND:   return "not_found";
    case SlopeEnd::BATTLE:      return "battle";
    }
    return "unknown";
}

Pokemon::Outcome ride(SlopeEnv& env, const StoryStep& step, const std::vector<SlopeMove>& moves){
    const SlopeCalls calls{
        .send = [&env](const std::vector<SlopePress>& presses, bool message){
            return send_slope_presses(env, presses, message);
        },
        .fight = [&env]{
            env.console.log("Slope: a Biker's battle; fighting it.", COLOR_ORANGE);
            const BattleEnd end = fight_battle(env.console, env.context, env.supplies, BattleKind::TRAINER, env.checks, env.language);
            mash_until_overworld(env.console, env.context, SLOPE_BATTLE_EXIT);
            return end == BattleEnd::WON;
        },
        .log = [&env](const std::string& text){ env.console.log(text); },
    };
    const SlopeResult result = ride_slope(moves, calls);
    if (result.end != SlopeEnd::DONE){
        return Pokemon::outcome_failed("slope_" + slope_end_name(result.end), std::format("move {}", result.move + 1));
    }
    return step.pose_after ? Pokemon::outcome_ok(Pokemon::pose_of(*step.pose_after)) : Pokemon::outcome_ok();
}

int read_count(const std::vector<BagRow>& rows, const std::string& item){
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const BagRow& data){ return data.item == item; });
    return row == rows.end() ? 0 : row->count;
}

Pokemon::Outcome count_items(SlopeEnv& env, const StoryStep& step){
    const std::vector<std::string>* items = game_arg<std::vector<std::string>>(step.args, "items");
    const bool* found = game_arg<bool>(step.args, "found");
    if (items == nullptr){
        return Pokemon::unsupported_step(step);
    }
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    for (const std::string& item : *items){
        const int count = read_count(rows, item);
        env.console.log(std::format("Bag: {} x{}.", item, count));
        if (count < 0){
            return Pokemon::outcome_failed("count_unread", item);
        }
        if (found != nullptr && *found && count != env.counted_items[item] + 1){
            return Pokemon::outcome_failed("item_not_found", std::format("{}: {}, was {}", item, count, env.counted_items[item]));
        }
        env.counted_items[item] = count;
    }
    return Pokemon::outcome_ok();
}

} // namespace


Pokemon::StoryActionSpec count_items_action_spec(){
    return {.kind = Pokemon::StoryStepKind::GAME, .name = COUNT_ITEMS_VARIANT, .args = {
        required_arg("items", Pokemon::StoryArgType::WORDS, "the items' bag slugs (\"rare-candy\")"),
        optional_arg("found", Pokemon::StoryArgType::BOOLEAN, false, "each must be one more than the last count kept"),
    }};
}

std::optional<Pokemon::Outcome> run_slope_step(SlopeEnv& env, const StoryStep& step){
    if (step.action == COUNT_ITEMS_VARIANT){
        return count_items(env, step);
    }
    const std::optional<std::vector<SlopeMove>> moves = parse_slope_step(step);
    if (!moves){
        return std::nullopt;
    }
    return ride(env, step, *moves);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

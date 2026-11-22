/*  AutoStory Slope Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Controllers/MgbaLink/MgbaLink_Protocol.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkMoves.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
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

using Pokemon::StoryFailure;


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

//  Converts wall-clock elapsed time back into game time so press_at() maps
//  correctly when running on an accelerated GBA emulator.
Milliseconds to_game_duration(Milliseconds wall){
    if (GameSettings::instance().DEVICE != GameSettings::Device::GbaEmulator){
        return wall;
    }
    const double speed = MgbaLink::speed_multiplier();
    if (!std::isfinite(speed) || speed <= 0.0 || speed == 1.0){
        return wall;
    }
    return Milliseconds(std::max<int64_t>(0, (int64_t)std::llround((double)wall.count() * speed)));
}

struct PressAtTime{
    size_t index = 0;
    Milliseconds remaining = Milliseconds(0);
};

//  The press under way `elapsed` after the first of `presses` began, and how
//  much of its hold remained.
PressAtTime press_at(const std::vector<SlopePress>& presses, Milliseconds elapsed){
    Milliseconds end(0);
    for (size_t c = 0; c < presses.size(); c++){
        end += presses[c].hold;
        if (elapsed < end){
            return PressAtTime{.index = c, .remaining = end - elapsed};
        }
    }
    return PressAtTime{.index = presses.empty() ? 0 : presses.size() - 1};
}

//  A white dialog that opens during a batch (SlopeMessageGate).
class SlopeMessageWatcher : public VisualInferenceCallback{
public:
    SlopeMessageWatcher()
        : VisualInferenceCallback("SlopeMessageWatcher")
        , m_detector(COLOR_RED)
        , m_gate(scale_emulator_duration(SlopeMessageGate::HOLD))
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
//  for a battle and, if `message`, for a message. The interrupted press is
//  the one under way by the game time since the batch began.
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
        const Milliseconds elapsed = to_game_duration(std::chrono::duration_cast<Milliseconds>(current_time() - start));
        const PressAtTime at = press_at(presses, elapsed);
        return SlopeSendResult{.sent = SlopeSent::BATTLE, .press = at.index};
    }
    if (seen == 1){
        const Milliseconds elapsed = to_game_duration(std::chrono::duration_cast<Milliseconds>(current_time() - start));
        const Milliseconds opened_at = elapsed > SlopeMessageGate::HOLD
            ? elapsed - SlopeMessageGate::HOLD
            : Milliseconds(0);
        const PressAtTime at = press_at(presses, opened_at);
        return SlopeSendResult{.sent = SlopeSent::MESSAGE, .press = at.index, .press_remaining = at.remaining};
    }
    return SlopeSendResult{.sent = SlopeSent::ALL};
}

//  Clears an unexpected white dialog (e.g. "REPEL's effect wore off!" or a
//  Biker's pre-battle line) while keeping B held so the player does not roll
//  once back in the overworld. Returns true if a battle starts.
bool clear_slope_dialog(SlopeEnv& env){
    BattleScreenWatcher battle(COLOR_RED);
    WhiteDialogOverWatcher closed(COLOR_RED);
    const int ret = run_until<ProControllerContext>(
        env.console, env.context,
        [](ProControllerContext& context){
            for (size_t c = 0; c < 6; c++){
                pbf_controller_state(context, BUTTON_B | BUTTON_A, DPAD_NONE, {0, 0}, {0, 0}, SlopeMechanics::CLOSE_PRESS);
                pbf_controller_state(context, BUTTON_B, DPAD_NONE, {0, 0}, {0, 0}, SlopeMechanics::CLOSE_WAIT);
            }
        },
        {battle, closed}
    );
    if (ret == 0){
        return true;
    }
    BattleScreenWatcher after_battle(COLOR_RED);
    const int after = run_until<ProControllerContext>(
        env.console, env.context,
        [](ProControllerContext& context){
            pbf_controller_state(context, BUTTON_B, DPAD_NONE, {0, 0}, {0, 0}, Milliseconds(2500));
        },
        {after_battle}
    );
    return after == 0;
}

std::string slope_end_name(SlopeEnd end){
    switch (end){
    case SlopeEnd::DONE:        return "done";
    case SlopeEnd::NOT_FOUND:   return "not_found";
    case SlopeEnd::BATTLE:      return "battle";
    }
    return "unknown";
}

std::optional<StoryFailure> ride(SlopeEnv& env, const StoryStep&, const std::vector<SlopeMove>& moves){
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
        .clear_dialog = [&env]{ return clear_slope_dialog(env); },
    };
    const SlopeResult result = ride_slope(moves, calls);
    if (result.end != SlopeEnd::DONE){
        return frlg_failure("slope_" + slope_end_name(result.end), std::format("move {}", result.move + 1));
    }
    return std::nullopt;
}

int read_count(const std::vector<BagRow>& rows, const std::string& item){
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const BagRow& data){ return data.item == item; });
    return row == rows.end() ? 0 : row->count;
}

std::optional<StoryFailure> count_items(SlopeEnv& env, const StoryStep& step){
    const std::vector<std::string>* items = game_arg<std::vector<std::string>>(step.args, "items");
    const bool* found = game_arg<bool>(step.args, "found");
    if (items == nullptr){
        return frlg_unsupported_step(step);
    }
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    //  The counts the story keeps (Potions, Super Potions, ...) from the bag
    //  too: a Mart step's fill_to buys against them.
    if (!rows.empty()){
        set_from_bag(env.supplies, rows);
    }
    for (const std::string& item : *items){
        const int count = read_count(rows, item);
        env.console.log(std::format("Bag: {} x{}.", item, count));
        if (count < 0){
            return frlg_failure("count_unread", item);
        }
        if (found != nullptr && *found && count != env.counted_items[item] + 1){
            return frlg_failure("item_not_found", std::format("{}: {}, was {}", item, count, env.counted_items[item]));
        }
        env.counted_items[item] = count;
    }
    return std::nullopt;
}

} // namespace


std::optional<std::optional<StoryFailure>> run_slope_step(SlopeEnv& env, const StoryStep& step){
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

/*  AutoStory Slope Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <format>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
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
        end += scale_emulator_duration(presses[c].hold);
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
        return m_gate.opened(m_detector.detect(frame), timestamp, scale_emulator_duration(SlopeMessageGate::HOLD));
    }

private:
    WhiteDialogDetector m_detector;
    SlopeMessageGate m_gate;
};

bool is_repel_wore_off_message(Logger& logger, Language language, const ImageViewRGB32& screen){
    if (language == Language::None){
        return false;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageRGB32 ready = preprocess_for_ocr(extract_box_reference(game, ImageFloatBox{0.08, 0.73, 0.84, 0.09}));
    for (const OCR::TextColorRange& filter : DARK_TEXT_FILTERS()){
        const ImageRGB32 bw = to_blackwhite_rgb32_range(ready, true, filter.mins, filter.maxs);
        std::string text = OCR::ocr_read(language, bw, OCR::PageSegMode::SINGLE_LINE);
        for (char& ch : text){
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        logger.log("Slope: white dialog OCR: " + text);
        if (text.find("repel") != std::string::npos || text.find("wore") != std::string::npos){
            return true;
        }
    }
    return false;
}

//  Sends `presses` back to back, one batch with no wait between, watching
//  for a battle and, if `message`, for a message. A battle's press is the
//  one under way by the time since the batch began.
SlopeSendResult send_slope_presses(SlopeEnv& env, const std::vector<SlopePress>& presses, bool message){
    std::vector<SlopePress> pending = presses;
    size_t offset = 0;
    while (!pending.empty()){
        SlopeMessageWatcher opened;
        BattleScreenWatcher battle(COLOR_RED);
        std::vector<PeriodicInferenceCallback> watchers{battle};
        if (message){
            watchers.push_back(opened);
        }
        const WallClock start = current_time();
        const int seen = run_until<ProControllerContext>(
            env.console, env.context,
            [&pending](ProControllerContext& context){
                for (const SlopePress& press : pending){
                    const DpadPosition dpad = press.dpad ? OverworldWalkSteps::dpad(*press.dpad) : DPAD_NONE;
                    pbf_controller_state(context, slope_buttons(press), dpad, {0, 0}, {0, 0}, press.hold);
                }
            },
            watchers
        );
        const Milliseconds elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start);
        const size_t local = press_at(pending, elapsed);
        if (seen == 0){
            return SlopeSendResult{.sent = SlopeSent::BATTLE, .press = offset + local};
        }
        if (seen == 1){
            const VideoSnapshot snapshot = env.console.video().snapshot();
            if (is_repel_wore_off_message(env.console, env.language, *snapshot.frame)){
                env.console.log("Slope: Repel wore off; closing message and resuming.", COLOR_ORANGE);
                for (const SlopePress& press : close_message_presses()){
                    const DpadPosition dpad = press.dpad ? OverworldWalkSteps::dpad(*press.dpad) : DPAD_NONE;
                    pbf_controller_state(env.context, slope_buttons(press), dpad, {0, 0}, {0, 0}, press.hold);
                }
                env.context.wait_for_all_requests();
                Milliseconds spent(0);
                for (size_t c = 0; c < local; c++){
                    spent += scale_emulator_duration(pending[c].hold);
                }
                const Milliseconds full_scaled = scale_emulator_duration(pending[local].hold);
                const Milliseconds used_in_local = std::max(Milliseconds(0), elapsed - spent);
                const Milliseconds rem_scaled = std::max(Milliseconds(0), full_scaled - used_in_local);
                std::vector<SlopePress> remaining;
                if (rem_scaled > Milliseconds(0) && full_scaled > Milliseconds(0)){
                    SlopePress partial = pending[local];
                    partial.hold = Milliseconds(
                        pending[local].hold.count() * rem_scaled.count() / full_scaled.count()
                    );
                    if (partial.hold > Milliseconds(0)){
                        remaining.push_back(partial);
                    }
                    offset += local;
                }else{
                    offset += local + 1;
                }
                if (local + 1 < pending.size()){
                    remaining.insert(remaining.end(), pending.begin() + static_cast<std::ptrdiff_t>(local + 1), pending.end());
                }
                pending = std::move(remaining);
                continue;
            }
            return SlopeSendResult{.sent = SlopeSent::MESSAGE};
        }
        return SlopeSendResult{.sent = SlopeSent::ALL};
    }
    return SlopeSendResult{.sent = SlopeSent::ALL};
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
    //  The counts the story keeps (Potions, Super Potions, ...) from the bag
    //  too: a Mart step's fill_to buys against them.
    if (!rows.empty()){
        set_from_bag(env.supplies, rows);
    }
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

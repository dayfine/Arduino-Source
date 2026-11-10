/*  AutoStory Teach
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_AutoStoryTeach.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


bool is_hm_move(std::string_view move){
    //  HM01-HM07, FRLG's HMs.
    for (std::string_view hm : {"cut", "fly", "surf", "strength", "flash", "rock-smash", "waterfall"}){
        if (move == hm){
            return true;
        }
    }
    return false;
}

const char* teach_end_name(TeachEnd end){
    switch (end){
    case TeachEnd::LEARNED:         return "learned";
    case TeachEnd::ALREADY_KNOWN:   return "already_known";
    case TeachEnd::REFUSED:         return "refused";
    case TeachEnd::UNREAD:          return "unread";
    case TeachEnd::NO_ITEM:         return "no_item";
    case TeachEnd::FAILED:          return "failed";
    }
    return "?";
}


Pokemon::StoryArgs teach_args(const TeachPlan& plan){
    Pokemon::StoryArgs args{
        {"item", plan.item},
        {"move", plan.move},
        {"slot", static_cast<int64_t>(plan.member + 1)},
    };
    if (!plan.forget_move.empty()){
        args["forget"] = plan.forget_move;
    }else if (plan.forget_slot){
        args["forget_slot"] = static_cast<int64_t>(*plan.forget_slot + 1);
    }
    return args;
}

namespace{

bool is_tm_or_hm(std::string_view item){
    return item.size() == 4 && (item.starts_with("tm") || item.starts_with("hm"))
        && variant_number(item.substr(2), 2).has_value();
}

} // namespace

std::optional<TeachPlan> parse_teach_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, TEACH_VARIANT, TEACH_ARGS);
    if (args == nullptr
        || !arg_number_within(*args, "slot", 1, static_cast<int64_t>(PARTY_SLOTS))
        || !arg_number_within(*args, "forget_slot", 1, static_cast<int64_t>(MOVE_SLOTS))
    ){
        return std::nullopt;
    }
    TeachPlan plan{
        .item = std::string(*Pokemon::story_arg_text(*args, "item")),
        .move = std::string(*Pokemon::story_arg_text(*args, "move")),
        .member = static_cast<size_t>(*Pokemon::story_arg_number(*args, "slot")) - 1,
    };
    if (!(is_tm_or_hm(plan.item) || plan.item == TEACH_TUTOR) || plan.move.empty()){
        return std::nullopt;
    }
    if (const std::optional<std::string_view> forget = Pokemon::story_arg_text(*args, "forget")){
        if (forget->empty()){
            return std::nullopt;
        }
        plan.forget_move = std::string(*forget);
    }
    if (const std::optional<int64_t> forget_slot = Pokemon::story_arg_number(*args, "forget_slot")){
        plan.forget_slot = static_cast<size_t>(*forget_slot) - 1;
    }
    return plan;
}

Pokemon::StoryStep teach_step(std::string name, std::string why, const TeachPlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = TEACH_VARIANT;
    step.args = teach_args(plan);
    step.once_per_save = true;
    return step;
}


TeachDecision decide_teach(const TeachPlan& plan, const std::vector<std::string>& moves){
    if (std::find(moves.begin(), moves.end(), plan.move) != moves.end()){
        return {.already_known = true};
    }
    if (moves.size() < MOVE_SLOTS){
        return {};
    }
    std::optional<size_t> slot = plan.forget_slot;
    if (!plan.forget_move.empty()){
        const auto found = std::find(moves.begin(), moves.end(), plan.forget_move);
        if (found == moves.end()){
            return {.error = std::format("it doesn't know {}, the move to forget", plan.forget_move)};
        }
        slot = size_t(found - moves.begin());
    }
    if (!slot){
        return {.error = "it knows four moves and the table names none to forget"};
    }
    if (is_hm_move(moves[*slot])){
        return {.error = std::format("{} is an HM move, which can't be forgotten", moves[*slot])};
    }
    return {.forget = slot};
}


namespace{

//  One TEACH step under way.
class TeachRun{
public:
    TeachRun(TeachActions& actions, const MemberReader& reader, const TeachPlan& plan)
        : m_actions(actions)
        , m_reader(reader)
        , m_plan(plan)
    {}

    TeachResult run(){
        const std::optional<MemberInfo> member = m_reader(m_plan.member);
        if (!member || member->moves.empty()){
            return end(TeachEnd::UNREAD, "couldn't read the member's moves");
        }
        const TeachDecision decision = decide_teach(m_plan, member->moves);
        if (decision.already_known){
            return end(TeachEnd::ALREADY_KNOWN, "");
        }
        if (!decision.error.empty()){
            return end(TeachEnd::REFUSED, decision.error);
        }
        if (decision.forget){
            m_result.forgot = member->moves[*decision.forget];
        }
        switch (m_actions.open_item(m_plan.item, m_plan.move)){
        case ItemOpen::OPENED:
            break;
        case ItemOpen::NOT_IN_CASE:
            return leave(TeachEnd::NO_ITEM, std::format("no {} in the TM Case", m_plan.item));
        case ItemOpen::FAILED:
            m_actions.dump_screen("open-failed");
            return leave(TeachEnd::FAILED, std::format("couldn't open {} from the TM Case", m_plan.item));
        }
        if (!teach(decision.forget)){
            return leave(m_result.end, m_result.detail);
        }
        if (!m_actions.leave()){
            return failed("leave-failed", "the overworld didn't come back");
        }
        return confirm(decision.forget);
    }

private:
    TeachResult end(TeachEnd end, std::string detail){
        m_result.end = end;
        m_result.detail = std::move(detail);
        return m_result;
    }
    //  FAILED, with the screen saved as `label`.
    TeachResult failed(std::string_view label, std::string detail){
        m_actions.dump_screen(label);
        return end(TeachEnd::FAILED, std::move(detail));
    }
    TeachResult leave(TeachEnd end, std::string detail){
        if (!m_actions.leave()){
            return failed("leave-failed", "the overworld didn't come back");
        }
        return this->end(end, std::move(detail));
    }
    //  Sets the result, saves the screen and gives up teaching. False.
    bool give_up(TeachEnd end, std::string detail){
        m_actions.dump_screen(std::string("gave-up-") + teach_end_name(end));
        m_actions.stop_teaching();
        m_result.end = end;
        m_result.detail = std::move(detail);
        return false;
    }

    //  On "Teach which POKEMON?": the member and its prompts. False if
    //  it wasn't learned (the result is set).
    bool teach(std::optional<size_t> forget){
        switch (m_actions.choose_member(m_plan.member)){
        case TeachPrompt::LEARNED:
            if (forget){
                return give_up(TeachEnd::FAILED, "it learned without forgetting: the moves read were wrong");
            }
            return true;
        case TeachPrompt::DELETE_PROMPT:
            if (!forget){
                return give_up(TeachEnd::FAILED, "it asked to forget a move, but the moves read had room");
            }
            return forget_move(*forget);
        case TeachPrompt::NOT_COMPATIBLE:
            return give_up(TeachEnd::REFUSED, std::format("it can't learn {}", m_plan.move));
        case TeachPrompt::ALREADY_KNOWS:
            return give_up(TeachEnd::FAILED, std::format("the game says it knows {} already", m_plan.move));
        case TeachPrompt::FAILED:
            break;
        }
        return give_up(TeachEnd::FAILED, "the party menu's answer wasn't read");
    }

    bool forget_move(size_t slot){
        switch (m_actions.forget(slot)){
        case ForgetEnd::LEARNED:
            return true;
        case ForgetEnd::HM_REFUSED:
            return give_up(TeachEnd::REFUSED, "the game won't forget an HM move");
        case ForgetEnd::FAILED:
            break;
        }
        return give_up(TeachEnd::FAILED, "the moves page didn't answer");
    }

    TeachResult confirm(std::optional<size_t> forget){
        const std::optional<MemberInfo> after = m_reader(m_plan.member);
        if (!after || after->moves.empty()){
            return failed("moves-unread", "couldn't read the moves after teaching");
        }
        const std::vector<std::string>& moves = after->moves;
        const bool landed = forget
            ? *forget < moves.size() && moves[*forget] == m_plan.move
            : std::find(moves.begin(), moves.end(), m_plan.move) != moves.end();
        if (!landed){
            return failed("not-learned", std::format("{} isn't among its moves after teaching", m_plan.move));
        }
        return end(TeachEnd::LEARNED, "");
    }

    TeachActions& m_actions;
    const MemberReader& m_reader;
    const TeachPlan& m_plan;
    TeachResult m_result;
};

} // namespace

TeachResult run_teach(Logger& logger, TeachActions& actions, const MemberReader& reader, const TeachPlan& plan){
    TeachResult result = TeachRun(actions, reader, plan).run();
    const Color color = result.end == TeachEnd::LEARNED ? COLOR_BLACK : result.end == TeachEnd::NO_ITEM ? COLOR_ORANGE : COLOR_RED;
    logger.log(teach_message(plan, result), color);
    return result;
}

std::string teach_message(const TeachPlan& plan, const TeachResult& result){
    const std::string what = std::format("Teach {} ({}) to slot {}", plan.move, plan.item, plan.member + 1);
    switch (result.end){
    case TeachEnd::LEARNED:
        return result.forgot.empty()
            ? std::format("{}: learned.", what)
            : std::format("{}: learned, forgot {}.", what, result.forgot);
    case TeachEnd::ALREADY_KNOWN:
        return std::format("{}: it knows it already; nothing done.", what);
    case TeachEnd::NO_ITEM:
        return std::format("{}: {}; skipped.", what, result.detail);
    default:
        return std::format("{}: {}: {}.", what, teach_end_name(result.end), result.detail);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

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


std::string teach_variant(const TeachPlan& plan){
    std::string variant = std::format("{}{} {} slot={}", TEACH_VARIANT_PREFIX, plan.item, plan.move, plan.member + 1);
    if (!plan.forget_move.empty()){
        variant += " forget=" + plan.forget_move;
    }else if (plan.forget_slot){
        variant += std::format(" forget_slot={}", *plan.forget_slot + 1);
    }
    return variant;
}

namespace{

bool is_tm_or_hm(std::string_view item){
    return item.size() == 4 && (item.starts_with("tm") || item.starts_with("hm"))
        && variant_number(item.substr(2), 2).has_value();
}

bool set_teach_word(TeachPlan& plan, std::string_view word){
    const std::optional<VariantPair> pair = variant_pair(word);
    if (!pair){
        return false;
    }
    if (pair->key == "forget" && !pair->value.empty()){
        plan.forget_move = std::string(pair->value);
        return true;
    }
    const std::optional<size_t> number = variant_number(pair->value, 1);
    if (pair->key == "slot" && number && *number >= 1 && *number <= PARTY_SLOTS){
        plan.member = *number - 1;
        return true;
    }
    if (pair->key == "forget_slot" && number && *number >= 1 && *number <= MOVE_SLOTS){
        plan.forget_slot = *number - 1;
        return true;
    }
    return false;
}

}

std::optional<TeachPlan> parse_teach_variant(std::string_view variant){
    if (!variant.starts_with(TEACH_VARIANT_PREFIX)){
        return std::nullopt;
    }
    const std::vector<std::string_view> words = variant_words(variant.substr(TEACH_VARIANT_PREFIX.size()));
    if (words.size() < 3 || !is_tm_or_hm(words[0]) || variant_pair(words[1])){
        return std::nullopt;
    }
    TeachPlan plan{.item = std::string(words[0]), .move = std::string(words[1])};
    bool slot_seen = false;
    for (size_t c = 2; c < words.size(); c++){
        if (!set_teach_word(plan, words[c])){
            return std::nullopt;
        }
        slot_seen |= words[c].starts_with("slot=");
    }
    if (!slot_seen){
        return std::nullopt;
    }
    return plan;
}

Pokemon::StoryStep teach_step(std::string name, std::string why, const TeachPlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = teach_variant(plan);
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
            return leave(TeachEnd::FAILED, std::format("couldn't open {} from the TM Case", m_plan.item));
        }
        if (!teach(decision.forget)){
            return leave(m_result.end, m_result.detail);
        }
        if (!m_actions.leave()){
            return end(TeachEnd::FAILED, "the overworld didn't come back");
        }
        return confirm(decision.forget);
    }

private:
    TeachResult end(TeachEnd end, std::string detail){
        m_result.end = end;
        m_result.detail = std::move(detail);
        return m_result;
    }
    TeachResult leave(TeachEnd end, std::string detail){
        if (!m_actions.leave()){
            return this->end(TeachEnd::FAILED, "the overworld didn't come back");
        }
        return this->end(end, std::move(detail));
    }
    //  Sets the result and gives up teaching. False.
    bool give_up(TeachEnd end, std::string detail){
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
            return end(TeachEnd::FAILED, "couldn't read the moves after teaching");
        }
        const std::vector<std::string>& moves = after->moves;
        const bool landed = forget
            ? *forget < moves.size() && moves[*forget] == m_plan.move
            : std::find(moves.begin(), moves.end(), m_plan.move) != moves.end();
        if (!landed){
            return end(TeachEnd::FAILED, std::format("{} isn't among its moves after teaching", m_plan.move));
        }
        return end(TeachEnd::LEARNED, "");
    }

    TeachActions& m_actions;
    const MemberReader& m_reader;
    const TeachPlan& m_plan;
    TeachResult m_result;
};

}

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


}
}
}

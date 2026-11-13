/*  AutoStory Teach
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryTeach.h"

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

std::string_view teach_end_name(TeachEnd end){
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


namespace{

bool is_tm_or_hm(std::string_view item){
    return item.size() == 4 && (item.starts_with("tm") || item.starts_with("hm"))
        && item[2] >= '0' && item[2] <= '9' && item[3] >= '0' && item[3] <= '9';
}

//  The slot of `plan`'s move to forget, its first choice the member knows.
std::optional<size_t> first_known(const std::vector<std::string>& moves, const TeachPlan& plan){
    std::vector<std::string> choices{plan.forget_move};
    choices.insert(choices.end(), plan.forget_else.begin(), plan.forget_else.end());
    for (const std::string& move : choices){
        const auto found = std::find(moves.begin(), moves.end(), move);
        if (found != moves.end()){
            return size_t(found - moves.begin());
        }
    }
    return std::nullopt;
}

//  "whirlwind", or "whirlwind or sand-attack".
std::string forget_names(const TeachPlan& plan){
    std::string names = plan.forget_move;
    for (const std::string& move : plan.forget_else){
        names += std::format(" or {}", move);
    }
    return names;
}

//  "bite, water-pulse".
std::string joined(const std::vector<std::string>& moves){
    std::string names;
    for (const std::string& move : moves){
        names += names.empty() ? move : ", " + move;
    }
    return names;
}

//  The decision for a plan with a keep list: forget the one known move
//  outside it.
TeachDecision decide_keep(const TeachPlan& plan, const std::vector<std::string>& moves){
    std::vector<size_t> outside;
    for (size_t slot = 0; slot < moves.size(); slot++){
        const bool kept = std::find(plan.keep.begin(), plan.keep.end(), moves[slot]) != plan.keep.end();
        if (!kept && !is_hm_move(moves[slot])){
            outside.push_back(slot);
        }
    }
    if (outside.size() != 1){
        return {.error = std::format(
            "it knows {} and the keep list ({}) leaves {} moves to forget, not one",
            joined(moves), joined(plan.keep), outside.size()
        )};
    }
    return {.forget = outside[0]};
}

} // namespace

Pokemon::StoryActionArgs teach_args(const TeachPlan& plan){
    Pokemon::StoryActionArgs args{{"item", plan.item}, {"move", plan.move}};
    if (plan.species.empty()){
        args.emplace("slot", int64_t(plan.member + 1));
    }else{
        args.emplace("species", plan.species);
    }
    if (!plan.keep.empty()){
        args.emplace("keep", plan.keep);
    }
    if (!plan.forget_move.empty()){
        args.emplace("forget", plan.forget_move);
        if (!plan.forget_else.empty()){
            args.emplace("or_forget", plan.forget_else);
        }
    }else if (plan.forget_slot){
        args.emplace("forget_slot", int64_t(*plan.forget_slot + 1));
    }
    return args;
}

std::optional<TeachPlan> teach_plan_of(const Pokemon::StoryActionArgs& args){
    const std::string* item = game_arg<std::string>(args, "item");
    const std::string* move = game_arg<std::string>(args, "move");
    const std::string* species = game_arg<std::string>(args, "species");
    const std::optional<size_t> member = args.contains("slot") ? counted_arg(args, "slot", PARTY_SLOTS) : std::nullopt;
    const bool one_member = species == nullptr ? member.has_value() : !species->empty() && !args.contains("slot");
    if (item == nullptr || !(is_tm_or_hm(*item) || *item == TEACH_TUTOR) || move == nullptr || move->empty() || !one_member){
        return std::nullopt;
    }
    TeachPlan plan{.item = *item, .move = *move, .member = member.value_or(0)};
    if (species != nullptr){
        plan.species = *species;
    }
    if (const std::vector<std::string>* keep = game_arg<std::vector<std::string>>(args, "keep")){
        plan.keep = *keep;
    }
    if (const std::string* forget = game_arg<std::string>(args, "forget")){
        if (forget->empty()){
            return std::nullopt;
        }
        plan.forget_move = *forget;
    }
    if (const std::vector<std::string>* forget_else = game_arg<std::vector<std::string>>(args, "or_forget")){
        if (plan.forget_move.empty()){
            return std::nullopt;
        }
        plan.forget_else = *forget_else;
    }
    if (args.contains("forget_slot")){
        plan.forget_slot = counted_arg(args, "forget_slot", MOVE_SLOTS);
        if (!plan.forget_slot){
            return std::nullopt;
        }
    }
    return plan;
}

Pokemon::StoryStep teach_step(std::string name, std::string why, const TeachPlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = TEACH_ACTION;
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
    if (!plan.keep.empty()){
        return decide_keep(plan, moves);
    }
    std::optional<size_t> slot = plan.forget_slot;
    if (!plan.forget_move.empty()){
        const std::optional<size_t> known = first_known(moves, plan);
        if (!known){
            return {.error = std::format("it doesn't know {}, the move to forget", forget_names(plan))};
        }
        slot = known;
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
        m_actions.dump_screen(std::string("gave-up-") + std::string(teach_end_name(end)));
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

std::optional<size_t> find_party_species(const MemberReader& reader, std::string_view species){
    for (size_t slot = 0; slot < PARTY_SLOTS; slot++){
        const std::optional<MemberInfo> member = reader(slot);
        if (!member){
            return std::nullopt;
        }
        if (member->species == species){
            return slot;
        }
    }
    return std::nullopt;
}

TeachResult run_teach(
    Logger& logger, TeachActions& actions, const MemberReader& reader, const TeachPlan& plan,
    const SpeciesFinder& find_species
){
    TeachPlan found = plan;
    if (!plan.species.empty()){
        const std::optional<size_t> slot = find_species ? find_species(plan.species) : find_party_species(reader, plan.species);
        if (!slot){
            const TeachResult result{.end = TeachEnd::UNREAD, .detail = std::format("no {} read in the party", plan.species)};
            logger.log(std::format("Teach {} ({}) to the {}: {}.", plan.move, plan.item, plan.species, result.detail), COLOR_RED);
            return result;
        }
        found.member = *slot;
    }
    TeachResult result = TeachRun(actions, reader, found).run();
    const Color color = result.end == TeachEnd::LEARNED ? COLOR_BLACK : result.end == TeachEnd::NO_ITEM ? COLOR_ORANGE : COLOR_RED;
    logger.log(teach_message(found, result), color);
    return result;
}

std::string teach_message(const TeachPlan& plan, const TeachResult& result){
    const std::string to = plan.species.empty()
        ? std::format("slot {}", plan.member + 1)
        : std::format("the {} (slot {})", plan.species, plan.member + 1);
    const std::string what = std::format("Teach {} ({}) to {}", plan.move, plan.item, to);
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

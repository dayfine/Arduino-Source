/*  AutoStory Grind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;


const char* grind_end_name(GrindEnd end){
    switch (end){
    case GrindEnd::REACHED:             return "reached";
    case GrindEnd::LEVEL_CAP:           return "level_cap";
    case GrindEnd::NO_ESTIMATE:         return "no_estimate";
    case GrindEnd::OUT_OF_ENCOUNTERS:   return "out_of_encounters";
    case GrindEnd::NO_ENCOUNTER:        return "no_encounter";
    case GrindEnd::SHINY:               return "shiny";
    case GrindEnd::PLAYER_FAINTED:      return "player_fainted";
    case GrindEnd::UNREAD:              return "unread";
    case GrindEnd::REFUSED:             return "refused";
    case GrindEnd::FAILED:              return "failed";
    }
    return "?";
}


Pokemon::StoryArgs grind_args(const GrindPlan& plan){
    Pokemon::StoryArgs args{
        {"slot", static_cast<int64_t>(plan.member + 1)},
        {"level", static_cast<int64_t>(plan.level)},
        {"encounters", static_cast<int64_t>(plan.max_encounters)},
        {"pace", std::string(direction_name(plan.pace))},
        {"heal_every", static_cast<int64_t>(plan.heal_every)},
    };
    if (!plan.trainer.empty()){
        args["beat"] = plan.trainer;
        args["chance"] = static_cast<int64_t>(std::llround(100 * plan.min_chance));
    }
    return args;
}

namespace{

//  The most a count names: four digits.
constexpr int64_t MAX_COUNT = 9999;

std::optional<Direction> direction_from_name(std::string_view name){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
        if (name == direction_name(direction)){
            return direction;
        }
    }
    return std::nullopt;
}

//  `args` (checked against GRIND_ARGS) as a plan; nothing if a value is out
//  of range, or a chance has no trainer to be against.
std::optional<GrindPlan> grind_plan(const Pokemon::StoryArgs& args){
    using Pokemon::story_arg_number;
    if (!arg_number_within(args, "slot", 1, static_cast<int64_t>(PARTY_SLOTS)) || !arg_number_within(args, "level", 2, 100)
        || !arg_number_within(args, "encounters", 1, MAX_COUNT) || !arg_number_within(args, "heal_every", 0, MAX_COUNT)
        || !arg_number_within(args, "chance", 1, 100)
    ){
        return std::nullopt;
    }
    const std::optional<Direction> pace = direction_from_name(*Pokemon::story_arg_text(args, "pace"));
    const std::optional<std::string_view> trainer = Pokemon::story_arg_text(args, "beat");
    const std::optional<int64_t> chance = story_arg_number(args, "chance");
    if (!pace || (trainer && trainer->empty()) || (chance && !trainer)){
        return std::nullopt;
    }
    GrindPlan plan;
    plan.member = static_cast<size_t>(*story_arg_number(args, "slot")) - 1;
    plan.level = static_cast<unsigned>(*story_arg_number(args, "level"));
    plan.max_encounters = static_cast<size_t>(*story_arg_number(args, "encounters"));
    plan.pace = *pace;
    plan.heal_every = static_cast<size_t>(story_arg_number(args, "heal_every").value_or(0));
    plan.trainer = std::string(trainer.value_or(""));
    if (chance){
        plan.min_chance = static_cast<double>(*chance) / 100;
    }
    return plan;
}

} // namespace

std::optional<GrindPlan> parse_grind_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, GRIND_VARIANT, GRIND_ARGS);
    return args == nullptr ? std::nullopt : grind_plan(*args);
}

Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::Pose nurse){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = GRIND_VARIANT;
    step.args = grind_args(plan);
    step.target = std::move(nurse);
    return step;
}

std::optional<GrindPlan> parse_grind_check_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, GRIND_CHECK_VARIANT, GRIND_ARGS);
    return args == nullptr ? std::nullopt : grind_plan(*args);
}

Pokemon::StoryStep grind_check_step(std::string name, std::string why, const GrindPlan& plan){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = GRIND_CHECK_VARIANT;
    step.args = grind_args(plan);
    return step;
}


namespace{

//  One GRIND step under way.
class GrindRun{
public:
    GrindRun(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan)
        : m_logger(logger)
        , m_actions(actions)
        , m_reader(reader)
        , m_plan(plan)
    {}

    GrindResult run(){
        if (m_plan.member != 0){
            return end(GrindEnd::REFUSED, "only the lead grinds: the battle handler fights with it alone");
        }
        if (!read_level()){
            return end(GrindEnd::UNREAD, "couldn't read the member's level");
        }
        if (over()){
            return m_result;
        }
        while (true){
            if (m_result.encounters >= m_plan.max_encounters){
                return end(GrindEnd::OUT_OF_ENCOUNTERS, "");
            }
            if (heal_due() && !heal()){
                return end(GrindEnd::FAILED, "the trip to the nurse and back failed");
            }
            if (!battle()){
                return m_result;
            }
            if (over()){
                if (m_result.end != GrindEnd::REACHED || heal()){
                    return m_result;
                }
                return end(GrindEnd::FAILED, "the last trip to the nurse failed");
            }
        }
    }
    //  The member, read, has what the plan grinds for. Nothing fought.
    bool reached_already(){
        return m_plan.member == 0 && read_level() && over() && m_result.end == GrindEnd::REACHED;
    }

private:
    GrindResult end(GrindEnd end, std::string detail){
        m_result.end = end;
        m_result.detail = std::move(detail);
        return m_result;
    }
    bool read_level(){
        std::optional<MemberInfo> member = m_reader(m_plan.member);
        if (!member || !member->level){
            return false;
        }
        m_result.level = member->level;
        m_member = std::move(*member);
        return true;
    }
    bool reached() const{
        return m_result.level && *m_result.level >= m_plan.level;
    }
    //  Whether the step ends at the member just read, its end set if so:
    //  the level reached; or with a trainer, the win chance reached, else
    //  the level cap.
    bool over(){
        if (!m_plan.trainer.empty()){
            return chance_over();
        }
        if (!reached()){
            return false;
        }
        end(GrindEnd::REACHED, "");
        return true;
    }
    bool chance_over(){
        const std::optional<WinEstimate> estimate = m_actions.win_chance(m_member, m_plan.trainer);
        if (!estimate){
            end(GrindEnd::NO_ESTIMATE, std::format("the sim has no win chance against {}", m_plan.trainer));
            return true;
        }
        m_result.chance = estimate->chance;
        m_logger.log(std::format(
            "Grind: level {} against {}: the sim wins {:.0f}% with {}.",
            *m_result.level, m_plan.trainer, 100 * estimate->chance, estimate->inputs
        ));
        if (estimate->chance >= m_plan.min_chance){
            end(GrindEnd::REACHED, "");
            return true;
        }
        if (reached()){
            end(GrindEnd::LEVEL_CAP, std::format("Lv{} is the cap, short of the win chance", m_plan.level));
            return true;
        }
        return false;
    }
    bool heal_due(){
        return m_actions.needs_heal() || (m_plan.heal_every > 0 && m_since_heal >= m_plan.heal_every);
    }
    bool heal(){
        m_logger.log(std::format("Grind: to the nurse after {} battles.", m_since_heal));
        m_since_heal = 0;
        m_result.heals++;
        return m_actions.heal_trip();
    }

    //  One wild battle, from the patch back to it. False if the step ends
    //  (the result is set).
    bool battle(){
        if (!m_actions.find_encounter(m_plan.pace)){
            end(GrindEnd::NO_ENCOUNTER, "");
            return false;
        }
        m_result.encounters++;
        const EncounterSeen seen = m_actions.read_encounter();
        if (seen.shiny){
            m_logger.log("Grind: a shiny! Stopping in its battle.", COLOR_BLUE);
            end(GrindEnd::SHINY, seen.species);
            return false;
        }
        switch (m_actions.fight()){
        case GrindFight::OVER:
            break;
        case GrindFight::LOST:
            end(GrindEnd::PLAYER_FAINTED, "");
            return false;
        case GrindFight::FAILED:
            end(GrindEnd::FAILED, "the battle didn't end on the overworld");
            return false;
        }
        m_since_heal++;
        if (!read_level()){
            end(GrindEnd::FAILED, "couldn't read the level after a battle");
            return false;
        }
        m_logger.log(std::format("Grind: battle {} ({}), level {}.", m_result.encounters, seen.species, *m_result.level));
        return true;
    }

    Logger& m_logger;
    GrindActions& m_actions;
    const MemberReader& m_reader;
    const GrindPlan& m_plan;
    size_t m_since_heal = 0;
    MemberInfo m_member;
    GrindResult m_result;
};

} // namespace

GrindResult run_grind(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan){
    GrindResult result = GrindRun(logger, actions, reader, plan).run();
    logger.log(grind_message(plan, result), result.end == GrindEnd::REACHED ? COLOR_BLACK : COLOR_RED);
    return result;
}

bool grind_reached_already(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan){
    const bool reached = GrindRun(logger, actions, reader, plan).reached_already();
    logger.log(reached ? "Grind: reached already; no walk out to the patch." : "Grind: not reached yet; out to the patch.");
    return reached;
}

std::string grind_message(const GrindPlan& plan, const GrindResult& result){
    const std::string level = result.level ? std::to_string(*result.level) : "?";
    const std::string chance = plan.trainer.empty() ? "" : std::format(
        ", win chance {} of {:.0f}% against {}",
        result.chance ? std::format("{:.0f}%", 100 * *result.chance) : "?", 100 * plan.min_chance, plan.trainer
    );
    const std::string tally = std::format(
        "level {} of {}{}, {} encounters, {} trips to the nurse", level, plan.level, chance, result.encounters, result.heals
    );
    if (result.end == GrindEnd::SHINY){
        return std::format(
            "Grind: a shiny {} in encounter {}. Stopped in its battle without running, for you to decide.",
            result.detail.empty() ? "Pokemon" : result.detail, result.encounters
        );
    }
    if (result.detail.empty()){
        return std::format("Grind slot {}: {}: {}.", plan.member + 1, grind_end_name(result.end), tally);
    }
    return std::format("Grind slot {}: {}: {}: {}.", plan.member + 1, grind_end_name(result.end), result.detail, tally);
}

PartyMember lead_as_read(const MemberInfo& read, const PartyMember& tracked){
    std::vector<SummaryMove> moves;
    for (const std::string& move : read.moves){
        const auto known = std::find_if(tracked.moves.begin(), tracked.moves.end(), [&](const KnownMove& item){
            return item.move == move;
        });
        const int full = max_pp(move);
        moves.emplace_back(SummaryMove{.move = move, .pp = known == tracked.moves.end() ? full : known->pp, .max_pp = full});
    }
    const uint8_t level = read.level ? static_cast<uint8_t>(std::min(*read.level, 100u)) : battle_level(tracked);
    return member_from_summary(read.species.empty() ? tracked.species : read.species, level, moves, read.stats);
}

std::optional<WinEstimate> healed_win_chance(PartyMember lead, const SimItems& items, std::string_view trainer){
    restore_pp(lead);
    return story_win_chance(lead, items, 1.0, trainer);
}


ConsoleGrindActions::ConsoleGrindActions(
    ConsoleHandle& console, ProControllerContext& context, Language language,
    Supplies& supplies, StoryChecks checks, HealTrip heal_trip, PaceTrainerBattle trainer_battle
)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_supplies(supplies)
    , m_checks(checks)
    , m_encounters(console, context, language, trainer_battle)
    , m_heal_trip(std::move(heal_trip))
    , m_trainer_battle(std::move(trainer_battle))
{}

bool ConsoleGrindActions::find_encounter(Direction pace){
    //  As ConsoleCaptureActions::find_encounter(), keeping track of the
    //  tile: a battle starts on the tile just stepped onto.
    ConsolePatchSteps steps(m_console, m_context, pace, m_spot, m_trainer_battle);
    return pace_until_battle(m_console.logger(), steps, PACE_MAX_STEPS);
}

EncounterSeen ConsoleGrindActions::read_encounter(){
    return m_encounters.read_encounter();
}

GrindFight ConsoleGrindActions::fight(){
    try{
        const BattleEnd end = fight_battle(m_console, m_context, m_supplies, BattleKind::WILD_FIGHT, m_checks, m_language);
        return end == BattleEnd::WON ? GrindFight::OVER : GrindFight::LOST;
    }catch (OperationFailedException& e){
        m_console.log("Grind: " + e.message(), COLOR_RED);
        return GrindFight::FAILED;
    }
}

bool ConsoleGrindActions::needs_heal(){
    return m_supplies.hurt || m_supplies.status != StatusCondition::NONE || attacks_low_on_pp(m_supplies);
}

std::optional<WinEstimate> ConsoleGrindActions::win_chance(const MemberInfo& member, std::string_view trainer){
    m_supplies.lead = lead_as_read(member, m_supplies.lead);
    return healed_win_chance(m_supplies.lead, SimItems{.potions = m_supplies.potions, .awakenings = m_supplies.awakenings, .antidotes = m_supplies.antidotes}, trainer);
}

bool ConsoleGrindActions::heal_trip(){
    if (!m_heal_trip || !m_heal_trip(m_spot)){
        return false;
    }
    //  Back on the patch's tile, facing as the route left it.
    m_spot = {};
    return true;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

/*  AutoStory Grind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryGrind.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;


std::string_view grind_end_name(GrindEnd end){
    switch (end){
    case GrindEnd::REACHED:             return "reached";
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


Pokemon::StoryActionArgs grind_args(const GrindPlan& plan, Pokemon::GridPose nurse){
    Pokemon::StoryActionArgs args{
        {"slot", int64_t(plan.member + 1)},
        {"level", int64_t(plan.level)},
        {"encounters", int64_t(plan.max_encounters)},
        {"pace", plan.pace},
        {"heal_every", int64_t(plan.heal_every)},
        {"nurse", std::move(nurse)},
    };
    return args;
}

std::optional<GrindPlan> grind_plan_of(const Pokemon::StoryActionArgs& args){
    const std::optional<size_t> member = counted_arg(args, "slot", PARTY_SLOTS);
    const std::optional<size_t> level = number_arg(args, "level", 2, 100);
    const std::optional<size_t> encounters = number_arg(args, "encounters", 1, 9999);
    const std::optional<size_t> heal_every = number_arg(args, "heal_every", 0, 9999);
    const Direction* pace = game_arg<Direction>(args, "pace");
    if (!member || !level || !encounters || !heal_every || pace == nullptr){
        return std::nullopt;
    }
    return GrindPlan{
        .member = *member, .level = (unsigned)*level, .max_encounters = *encounters,
        .pace = *pace, .heal_every = *heal_every,
    };
}

Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::GridPose nurse){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = GRIND_ACTION;
    step.args = grind_args(plan, std::move(nurse));
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
        return true;
    }
    bool reached() const{
        return m_result.level && *m_result.level >= m_plan.level;
    }
    //  Whether the step ends at the member just read, its end set if so:
    //  the level reached.
    bool over(){
        if (!reached()){
            return false;
        }
        end(GrindEnd::REACHED, "");
        return true;
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
    GrindResult m_result;
};

} // namespace

GrindResult run_grind(Logger& logger, GrindActions& actions, const MemberReader& reader, const GrindPlan& plan){
    GrindResult result = GrindRun(logger, actions, reader, plan).run();
    logger.log(grind_message(plan, result), result.end == GrindEnd::REACHED ? COLOR_BLACK : COLOR_RED);
    return result;
}

std::string grind_message(const GrindPlan& plan, const GrindResult& result){
    const std::string level = result.level ? std::to_string(*result.level) : "?";
    const std::string tally = std::format(
        "level {} of {}, {} encounters, {} trips to the nurse", level, plan.level, result.encounters, result.heals
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

ConsoleGrindActions::ConsoleGrindActions(
    ConsoleHandle& console, ProControllerContext& context, Language language,
    Supplies& supplies, Pokemon::StoryCheckLevel checks, HealTrip heal_trip, PaceTrainerBattle trainer_battle
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

/*  AutoStory Grind
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleFlashWatcher.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;


namespace{

Direction opposite_direction(Direction direction){
    switch (direction){
    case Direction::NORTH:  return Direction::SOUTH;
    case Direction::EAST:   return Direction::WEST;
    case Direction::SOUTH:  return Direction::NORTH;
    case Direction::WEST:   return Direction::EAST;
    }
    return direction;
}

}


const char* grind_end_name(GrindEnd end){
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


std::string grind_variant(const GrindPlan& plan){
    return std::format(
        "{}slot={} level={} encounters={} pace={} heal_every={}",
        GRIND_VARIANT_PREFIX, plan.member + 1, plan.level, plan.max_encounters,
        direction_name(plan.pace), plan.heal_every
    );
}

namespace{

bool set_grind_word(GrindPlan& plan, std::string_view word){
    const std::optional<VariantPair> pair = variant_pair(word);
    if (!pair){
        return false;
    }
    if (pair->key == "pace"){
        for (Direction direction : Pokemon::ALL_DIRECTIONS){
            if (pair->value == direction_name(direction)){
                plan.pace = direction;
                return true;
            }
        }
        return false;
    }
    const std::optional<size_t> number = variant_number(pair->value, 4);
    if (!number){
        return false;
    }
    if (pair->key == "slot" && *number >= 1 && *number <= PARTY_SLOTS){
        plan.member = *number - 1;
    }else if (pair->key == "level" && *number >= 2 && *number <= 100){
        plan.level = (unsigned)*number;
    }else if (pair->key == "encounters" && *number >= 1){
        plan.max_encounters = *number;
    }else if (pair->key == "heal_every"){
        plan.heal_every = *number;
    }else{
        return false;
    }
    return true;
}

}

std::optional<GrindPlan> parse_grind_variant(std::string_view variant){
    if (!variant.starts_with(GRIND_VARIANT_PREFIX)){
        return std::nullopt;
    }
    GrindPlan plan;
    bool level_seen = false;
    for (std::string_view word : variant_words(variant.substr(GRIND_VARIANT_PREFIX.size()))){
        if (!set_grind_word(plan, word)){
            return std::nullopt;
        }
        level_seen |= word.starts_with("level=");
    }
    if (!level_seen){
        return std::nullopt;
    }
    return plan;
}

Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::Pose nurse){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = grind_variant(plan);
    step.target = std::move(nurse);
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
        if (reached()){
            return end(GrindEnd::REACHED, "");
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
            if (reached()){
                return heal() ? end(GrindEnd::REACHED, "") : end(GrindEnd::FAILED, "the last trip to the nurse failed");
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
        const std::optional<MemberInfo> member = m_reader(m_plan.member);
        if (!member || !member->level){
            return false;
        }
        m_result.level = member->level;
        return true;
    }
    bool reached() const{
        return m_result.level && *m_result.level >= m_plan.level;
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

}

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
    Supplies& supplies, StoryChecks checks, HealTrip heal_trip
)
    : m_console(console)
    , m_context(context)
    , m_language(language)
    , m_supplies(supplies)
    , m_checks(checks)
    , m_encounters(console, context, language)
    , m_heal_trip(std::move(heal_trip))
{}

bool ConsoleGrindActions::find_encounter(Direction pace){
    //  As ConsoleCaptureActions::find_encounter(), keeping track of the
    //  tile: a battle starts on the tile just stepped onto.
    constexpr size_t MAX_STEPS = 200;
    for (size_t c = 0; c < MAX_STEPS; c++){
        const Direction way = m_spot.displaced ? opposite_direction(pace) : pace;
        BattleFlashWatcher battle(COLOR_RED);
        const int ret = run_until<ProControllerContext>(
            m_console, m_context,
            [&](ProControllerContext& context){
                step_once(context, way, m_spot.facing);
            },
            {battle}
        );
        m_spot = {.displaced = !m_spot.displaced, .facing = way};
        if (ret == 0){
            return true;
        }
    }
    return false;
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
    return m_supplies.hurt || m_supplies.status != StatusCondition::NONE;
}

bool ConsoleGrindActions::heal_trip(){
    if (!m_heal_trip || !m_heal_trip(m_spot)){
        return false;
    }
    //  Back on the patch's tile, facing as the route left it.
    m_spot = {};
    return true;
}



}
}
}

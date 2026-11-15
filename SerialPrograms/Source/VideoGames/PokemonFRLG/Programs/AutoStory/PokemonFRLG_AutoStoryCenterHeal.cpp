/*  AutoStory Center Heal
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PokemonCenterFloorReader.h"
#include "PokemonFRLG_AutoStoryCenterHeal.h"
#include "PokemonFRLG_AutoStoryRoomLegs.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::CenterPlace;
using Pokemon::Outcome;


Outcome run_center_step(const Pokemon::StoryStep& step, Pokemon::CenterVisitIO& io){
    const Outcome walked = step.action == CENTER_EXIT_VARIANT
        ? Pokemon::leave_center(step.name, io)
        : Pokemon::enter_center(step.name, io);
    if (!walked.succeeded() || !step.pose_after){
        return walked;
    }
    return Pokemon::outcome_ok(Pokemon::pose_of(*step.pose_after));
}

Outcome heal_at_nurse(std::string_view label, Pokemon::CenterVisitIO& io){
    if (io.place() != CenterPlace::INSIDE){
        io.log(std::format("[Center] {}: no Center floor before the nurse.", label));
        return Pokemon::outcome_failed(
            std::string(Pokemon::CENTER_NOT_ENTERED),
            "Heal: no Pokemon Center floor on screen; the player is not in a Pokemon Center."
        );
    }
    io.heal();
    io.log(std::format("[Center] {}: healed.", label));
    return Pokemon::outcome_ok();
}


namespace{

class ConsoleCenterVisit : public FrlgCenterVisit{
public:
    explicit ConsoleCenterVisit(StoryEnv& env)
        : m_env(env)
    {}

    //  Both legs start facing north, at the door or at the nurse. A door
    //  with no fade fails in wait_out_warp(), so a walk that returns saw one.
    virtual Pokemon::WarpSeen walk_in() override{
        walk_room_legs(m_env.console, m_env.context, center_enter_legs(), Pokemon::Direction::NORTH, m_env.player_gender);
        return {.fade = WarpFade::BLACK};
    }
    //  Not reached: walk_in() sees a fade or fails.
    virtual void line_up_and_walk_in() override{
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT, "Center: no fade at its door.", m_env.console
        );
    }
    virtual CenterPlace place() override{
        m_env.context.wait_for_all_requests();
        const VideoSnapshot frame = m_env.console.video().snapshot();
        if (!frame){
            return CenterPlace::UNKNOWN;
        }
        return shows_pokemon_center_floor(frame) ? CenterPlace::INSIDE : CenterPlace::OUTSIDE;
    }
    virtual void heal() override{
        heal_at_pokecenter(m_env.console, m_env.context);
        healed_by_nurse(m_env.supplies);
    }
    virtual Pokemon::WarpSeen walk_out() override{
        walk_room_legs(m_env.console, m_env.context, center_exit_legs(), Pokemon::Direction::NORTH, m_env.player_gender);
        return {.fade = WarpFade::BLACK};
    }
    virtual void log(std::string_view line) override{
        m_env.console.log(std::string(line));
    }

private:
    StoryEnv& m_env;
};

} // namespace


Outcome center_step(StoryEnv& env, const Pokemon::StoryStep& step){
    ConsoleCenterVisit visit(env);
    return run_center_step(step, visit);
}

Outcome heal_in_center(StoryEnv& env, std::string_view label){
    ConsoleCenterVisit visit(env);
    return heal_at_nurse(label, visit);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

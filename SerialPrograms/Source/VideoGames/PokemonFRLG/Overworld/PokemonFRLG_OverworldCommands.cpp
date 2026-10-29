/*  Overworld Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_OverworldCommands.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

std::optional<MapDirection> parse_direction(const std::string& name){
    for (MapDirection direction : {MapDirection::NORTH, MapDirection::EAST, MapDirection::SOUTH, MapDirection::WEST}){
        if (name == direction_name(direction)){
            return direction;
        }
    }
    throw ParseException("Unknown direction: " + name);
}

std::vector<RouteStep> plan(const std::vector<std::string>& words, std::ostream& out){
    if (words.size() < 7){
        throw ParseException(words[0] + ": expected FROM_MAP x y TO_MAP x y [facing]");
    }
    const MapPosition start{words[1], {std::stoi(words[2]), std::stoi(words[3])}};
    const RouteGoal goal{{words[4], {std::stoi(words[5]), std::stoi(words[6])}}, words.size() > 7 ? parse_direction(words[7]) : std::nullopt};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(overworld_maps(post_game_flags()), start, goal);
    if (!route){
        throw ParseException(words[0] + ": no route from " + to_string(start) + " to " + to_string(goal.position));
    }
    for (const RouteStep& step : *route){
        out << "  " << to_string(step) << "\n";
    }
    return *route;
}

}


void register_overworld_commands(){
    static const bool registered = []{
        register_command_file_command(
            "frlg_plan", "frlg_plan FROM_MAP x y TO_MAP x y [facing]   (print an FRLG route)",
            [](CommandFileSession& session, const std::vector<std::string>& words){
                plan(words, session.out);
            }
        );
        register_command_file_command(
            "frlg_route", "frlg_route FROM_MAP x y TO_MAP x y [facing] [start facing]   (plan and walk an FRLG route)",
            [](CommandFileSession& session, const std::vector<std::string>& words){
                //  frlg_route ... [goal facing] [facing the player starts with]
                const std::optional<MapDirection> start = words.size() > 8 ? parse_direction(words[8]) : std::nullopt;
                walk_overworld_route(session.env.console, session.context, plan(words, session.out), start);
            }
        );
        return true;
    }();
    (void)registered;
}

//  Register at startup: the panel list that also calls this is only built
//  when FRLG's programs are first shown.
static const bool REGISTERED_AT_STARTUP = (register_overworld_commands(), true);



}
}
}

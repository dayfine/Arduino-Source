/*  Overworld Commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <set>
#include <string>
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

std::optional<Direction> parse_direction(const std::string& name){
    for (Direction direction : Pokemon::ALL_DIRECTIONS){
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
    //  Plan with the Hall of Fame's flags set, which clear the way to
    //  Cerulean Cave; they hide no one on the earlier maps.
    const std::set<std::string> flags = map_flag_set(overworld_maps(), "after_hall_of_fame");
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(overworld_maps(flags), start, goal);
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
            "frlg_route", "frlg_route FROM_MAP x y TO_MAP x y [facing] [start facing] [Max Repel slot]   (plan and walk an FRLG route)",
            [](CommandFileSession& session, const std::vector<std::string>& words){
                //  frlg_route ... [goal facing] [facing the player starts with]
                //  [Items pocket slot of the Max Repel to use when one wears off, from 1]
                const std::optional<Direction> start = words.size() > 8 ? parse_direction(words[8]) : std::nullopt;
                const std::vector<RouteStep> route = plan(words, session.out);
                if (words.size() > 9){
                    const OverworldInterrupts interrupts = overworld_interrupts_with_repel(std::stoi(words[9]) - 1);
                    walk_overworld_route(session.console, session.context, route, start, interrupts);
                }else{
                    walk_overworld_route(session.console, session.context, route, start);
                }
            }
        );
        register_command_file_command(
            "frlg_max_repel", "frlg_max_repel SLOT   (use the Max Repel in that Items pocket slot, from 1)",
            [](CommandFileSession& session, const std::vector<std::string>& words){
                if (words.size() < 2){
                    throw ParseException("frlg_max_repel: expected SLOT");
                }
                use_max_repel_from_bag(session.console, session.context, std::stoi(words[1]) - 1);
            }
        );
        return true;
    }();
    (void)registered;
}



}
}
}

/*  Blueberry Quests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BlueberryQuests_H
#define PokemonAutomation_PokemonSV_BlueberryQuests_H

#include <optional>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonSV/Programs/ShinyHunting/PokemonSV_LetsGoTools.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h"

namespace PokemonAutomation{
    struct ProgramInfo;
namespace NintendoSwitch{
namespace PokemonSV{

//Basic blue quests
const std::set<BBQuests> blue_quests = {
    BBQuests::auto_10, BBQuests::make_tm, BBQuests::pickup_10, BBQuests::sneak_up, BBQuests::photo_fly, BBQuests::photo_swim, BBQuests::photo_canyon, BBQuests::photo_coastal, BBQuests::photo_polar, BBQuests::photo_savanna, BBQuests::tera_self_defeat, 
    BBQuests::travel_500, BBQuests::catch_any, BBQuests::catch_normal, BBQuests::catch_fighting, BBQuests::catch_flying, BBQuests::catch_poison, BBQuests::catch_ground, BBQuests::catch_rock, BBQuests::catch_bug, BBQuests::catch_ghost, BBQuests::catch_steel, 
    BBQuests::catch_fire, BBQuests::catch_water, BBQuests::catch_grass, BBQuests::catch_electric, BBQuests::catch_psychic, BBQuests::catch_ice, BBQuests::catch_dragon, BBQuests::catch_dark, BBQuests::catch_fairy
};

//Red quests that appear at the top when a player completes 10 blues.
//In multiplayer, check if picnic is active before attempting some as only one player can picnic at a time.
const std::set<BBQuests> red_quests = {
    BBQuests::wash_pokemon, BBQuests::wild_tera, BBQuests::auto_30, BBQuests::tera_raid, BBQuests::sandwich_three, BBQuests::bitter_sandwich, BBQuests::sweet_sandwich, BBQuests::salty_sandwich, BBQuests::sour_sandwich, BBQuests::spicy_sandwich, BBQuests::hatch_egg, 
    BBQuests::photo_normal, BBQuests::photo_fighting, BBQuests::photo_flying, BBQuests::photo_poison, BBQuests::photo_ground, BBQuests::photo_rock, BBQuests::photo_bug, BBQuests::photo_ghost, BBQuests::photo_steel, BBQuests::photo_fire, BBQuests::photo_water, 
    BBQuests::photo_grass, BBQuests::photo_electric, BBQuests::photo_psychic, BBQuests::photo_ice, BBQuests::photo_dragon, BBQuests::photo_dark, BBQuests::photo_fairy
};

//Multiplayer only. Appears at the top when 3 red quests are completed.
const std::set<BBQuests> gold_quests = {
    BBQuests::ditto_central, BBQuests::ditto_canyon, BBQuests::ditto_coastal, BBQuests::ditto_polar, BBQuests::ditto_savanna, BBQuests::group_canyon, BBQuests::group_coastal, BBQuests::group_polar, BBQuests::group_savanna, BBQuests::group_eyewear, BBQuests::group_nonuniform, 
    BBQuests::group_masks, BBQuests::sandwich_four, BBQuests::catch_hint, BBQuests::catch_hint2
};

//Quests that are not currently supported. Gold quests currently excluded as this is singleplayer only right now.
const std::set<BBQuests> not_possible_quests = {
    BBQuests::UnableToDetect, BBQuests::pickup_10, BBQuests::photo_flying, BBQuests::catch_flying
};


// Open the BBQ panel and read the current amount of BP
int read_BP(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

//Open the quest panel and read in the current quests.
//Currently only supports singleplayer.
//For multiplayer, we will want keep track of which quest are gold/red and scroll cursor down until all of the current player's blue quests are in.
std::vector<BBQuests> read_quests(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//Determine which quests are possible and which quests are not. (ex. no eggs, reroll egg hatcher.)
//Quests not possible are removed from the list. If the list is empty, then reroll all items.
std::vector<BBQuests> process_quest_list(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    std::vector<BBQuests>& quest_list,
    uint8_t& eggs_hatched
);

//Take the current quest and calls the function to do it, then checks the quest was successful. Returns true if so.
bool process_and_do_quest(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS,
    BBQuests current_quest,
    uint8_t& eggs_hatched
);

//Iterate through TMs until a craftable one is found. Make the TM and return to position.
void quest_make_tm(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context
);

//Do laps in central plaza.
void quest_travel_500(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context
);

//Run around until you encounter a pokemon. Tera, then defeat it by spamming your first move.
void quest_tera_self_defeat(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//Sneak up on a pokemon
void quest_sneak_up(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//What a Tera route does once its encounter attempt has ended.
enum class TeraEncounterAction{
    StopForShiny,
    Battle,
    Recover,
};
//The route can run out of time without reaching a battle (e.g. the target did not spawn).
//Battle commands sent outside a battle would wander the overworld, so only a battle menu seen
//by the encounter watcher (callback 0) or by the route's own wait counts as entry.
//A shiny always stops the program, even without entry, so it is never lost.
TeraEncounterAction tera_encounter_action(
    bool shiny, int encounter_watcher_result, bool battle_menu_detected
);

//Kill a tera pokemon
void quest_wild_tera(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//Give bath
void quest_wash_pokemon(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context
);

//Withdraw and hatch an egg
void quest_hatch_egg(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//Make a sandwich of type flavor
void quest_sandwich(
    ProgramEnvironment& env,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);

//Complete a tera raid battle
void quest_tera_raid(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS
);

//Defeat 10/30 pokemon in autobattle
void quest_auto_battle(
    ProgramEnvironment& env,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);

//The game steps of one Auto Battle pass. They are injected so that the loop's decisions (when
//to go again, when to stop) run in tests without a Switch.
class BBQAutoBattleActions{
public:
    virtual ~BBQAutoBattleActions() = default;

    //Kills heard since the quest started. Kills are counted as they are heard, in any step.
    virtual uint64_t kills() const = 0;

    //The current time, to time the passes.
    virtual WallClock now() const = 0;

    //Fly from Central Plaza to Chargestone Cavern, wait for spawns, and walk into the battle court.
    virtual void fly_to_battle_area() = 0;

    //Send the lead out with Let's Go until no kill has been heard for 6 seconds, or until
    //kills() reaches `stop_at_kills`, whichever comes first. Returns the lead's last known HP,
    //from 0 to 1, or nothing if it could not be read.
    virtual std::optional<double> clear(uint64_t stop_at_kills) = 0;

    //Fly back to Central Plaza. Leaving the cave respawns its Pokemon.
    virtual void return_to_plaza() = 0;

    //Walk to the nurse and heal, then fly to the Central Plaza fly point again: the flight to
    //Chargestone moves the map cursor from there.
    virtual void heal() = 0;
};

struct BBQAutoBattlePolicy{
    uint64_t kill_target = 10;
};

//Why run_auto_battle() returned.
enum class BBQAutoBattleResult{
    TargetReached,
};

//Clear Chargestone Cavern, one pass at a time, until the kills reach the target, and return why
//it stopped. It ends at Central Plaza, where the next quest starts. Logs one line per pass: its
//kills, the total, the lead's HP, the time spent, and what happens next.
BBQAutoBattleResult run_auto_battle(
    Logger& logger,
    BBQAutoBattleActions& actions,
    const BBQAutoBattlePolicy& policy
);

}
}
}
#endif

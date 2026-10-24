/*  Blueberry Quests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BlueberryQuests_H
#define PokemonAutomation_PokemonSV_BlueberryQuests_H

#include <chrono>
#include <functional>
#include <optional>
#include <string>
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

BBQuests BBQuests_string_to_enum(const std::string& token);
//The slug of a quest, as quest reads log it (e.g. "pickup-10"), or "unknown".
std::string BBQuests_enum_to_string(BBQuests quest);

enum class CameraAngle{
    none,
    up,
    down
};

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



// Open the BBQ panel and read the current amount of BP
int read_BP(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

//Open the quest panel and read in the current quests.
//Currently only supports singleplayer.
//For multiplayer, we will want keep track of which quest are gold/red and scroll cursor down until all of the current player's blue quests are in.
//If BP is not null, also read the BP balance from the same screenshot (-1 on OCR failure).
std::vector<BBQuests> read_quests(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    int* BP = nullptr
);

//What to do with the quests read from the panel. Slot 0 is the top quest on the panel.
struct BBQuestPlan{
    std::vector<BBQuests> to_do;        //Quests to attempt, in panel order.
    std::vector<size_t> reroll_slots;   //Slots to reroll, in panel order.
    bool reroll_all = false;            //Nothing to do and nothing rerolled: reroll every slot.
};
//BP it costs to reroll one quest: 50 for a red quest, 10 for a blue one.
int reroll_cost(BBQuests quest);
//Decide what to do with each quest from the options alone, without touching the game, so
//the decision can be tested. Quests not possible are never run. Those not in the exclusions
//table are rerolled, red or blue, so they do not hold a slot, as long as BP (the balance
//read from the panel, if any) still covers their cost after the other planned rerolls; the
//rest are skipped, and the doable quests run. A table row for one decides instead.
//Out-of-egg hatch quests and quests set to reroll in the exclusions table are rerolled. A
//quest with several table rows gets the action of each row, in table order. With nothing to
//do, every quest not possible and not in the table is rerolled whatever the BP, or else
//every quest.
BBQuestPlan plan_quests(
    Logger& logger,
    const BBQOption& BBQ_OPTIONS,
    const std::vector<BBQuests>& quest_list,
    uint8_t eggs_hatched,
    std::optional<int> bp
);

//Work selected from a panel, or a complete observed panel to replan after a reroll.
//The observation is consumed once, before any further game actions.
struct BBQuestBatch{
    std::vector<BBQuests> to_do;
    std::optional<std::vector<BBQuests>> observed_panel;
};
//Expected seconds per completed quest (time per attempt / success rate), measured in lab
//runs R3-R5, or nothing for a quest that was not measured.
std::optional<double> expected_seconds_per_success(BBQuests quest);
//Approximate BP a quest pays.
int quest_reward(BBQuests quest);
//Reorder quests so the most BP per expected second (quest_reward() over
//expected_seconds_per_success()) run first. Quests that
//were not measured keep their places; ties keep their order.
void order_quests_by_cost(Logger& logger, std::vector<BBQuests>& quests);

//The steps of rerolling one quest on the quest panel. They are injected so that the reroll
//(which quest it pays for, when it gives up) runs in tests without a Switch.
class BBQRerollActions{
public:
    virtual ~BBQRerollActions() = default;

    //Open the quest panel from the overworld. Returns false if it did not open.
    virtual bool open_panel() = 0;

    //Press A on the quest under the cursor, which opens its reroll menu. Returns the slot of
    //the quest the menu opened next to, or nothing if no menu was seen.
    virtual std::optional<size_t> open_reroll_menu() = 0;

    //Close the reroll menu without rerolling.
    virtual void close_reroll_menu() = 0;

    //Move the cursor down this many slots, or up if negative.
    virtual void move_cursor(int slots) = 0;

    //Choose to reroll in the open menu. This spends BP.
    virtual void confirm_reroll() = 0;

    //Close the quest panel and return to the overworld.
    virtual void close_panel() = 0;

    //Open the quest panel and read the quests on it, as read_quests() does.
    virtual std::vector<BBQuests> read_quests() = 0;

    //With the panel still open after a reroll, read only the quest in `slot`.
    virtual BBQuests read_slot_in_panel(size_t slot) = 0;
};

enum class BBQRerollResult{
    Rerolled,       //The quest in the slot was replaced.
    PanelNotOpened, //Nothing was spent.
    MenuNotFound,   //Nothing was spent.
    NotReplaced,    //The reroll was confirmed, but the slot still shows the same quest.
};

//Reroll the quest in `slot` of `quests`, the quests last read from the panel. Only pays once
//the reroll menu is open next to that slot, then reads the panel again to check the quest was
//replaced. On Rerolled, `quests[slot]` is updated. observed_panel is populated
//only when every slot is known and no other slot changed, so scheduling can
//reuse it. A 3-slot read is only reused when `quests` had 3 slots.
//If reroll_again returns true for the new quest, it is rerolled again without leaving the panel.
BBQRerollResult reroll_quest(
    Logger& logger,
    BBQRerollActions& actions,
    std::vector<BBQuests>& quests,
    size_t slot,
    std::optional<std::vector<BBQuests>>& observed_panel,
    const std::function<bool(BBQuests)>& reroll_again = {}
);

//Reroll the quest in `slot` of `quests` on the console, with reroll_quest(). Stops the program
//if the reroll fails. Returns the observed panel when reusable.
//If record_BP is set, it gets the BP read from each verification read's screenshot.
std::optional<std::vector<BBQuests>> reroll_quest_slot(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    std::vector<BBQuests>& quests,
    size_t slot,
    const std::function<void(int BP)>& record_BP = {},
    const std::function<bool(BBQuests)>& reroll_again = {}
);

//Determine which quests are possible and which quests are not. (ex. no eggs, reroll egg hatcher.)
//Quests not possible are removed from the list. If the list is empty, then reroll all items.
//record_BP is passed to reroll_quest_slot().
BBQuestBatch process_quest_list(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    const std::vector<BBQuests>& quest_list,
    uint8_t eggs_hatched,
    const std::function<void(int BP)>& record_BP = {}
);

//Take the current quest and calls the function to do it, then checks the quest was successful. Returns true if so.
//If panel_after is given, it receives the quest panel read that checked the last attempt.
bool process_and_do_quest(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS,
    BBQuests current_quest,
    uint8_t& eggs_hatched,
    std::vector<BBQuests>* panel_after = nullptr
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

//End a quest away from Central Plaza. With FLY_DIRECTLY, stay there and let the next
//flight go straight to its destination. Otherwise return to Central Plaza.
void leave_quest_area(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//After a quest battle, back at Central Plaza: heal every third battle (and fly back
//onto the fly point), otherwise do nothing.
void heal_after_battle_if_due(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context
);

//Run around until you encounter a pokemon. Tera, then defeat it by spamming your first move.
// Send out the lead at whatever is in front and wait for a battle. If none starts,
// turn the camera about a quarter at a time, up to three times, and try again.
// Returns true once the battle menu shows.
bool send_out_and_look_around(
    ConsoleHandle& console, ProControllerContext& context,
    std::chrono::seconds first_wait, const std::string& target
);

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
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
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

    //The current time, for the pass log and the time limit.
    virtual WallClock now() const = 0;

    //Fly from Central Plaza to Chargestone Cavern, wait for spawns, and walk into the battle court.
    virtual void fly_to_battle_area() = 0;

    //Send the lead out with Let's Go until no kill has been heard for 6 seconds, or until
    //kills() reaches `stop_at_kills`, whichever comes first. Returns the lead's last known HP,
    //from 0 to 1, or nothing if it could not be read.
    virtual std::optional<double> clear(uint64_t stop_at_kills) = 0;

    //Fly back to Central Plaza. Leaving the cave respawns its Pokemon.
    virtual void return_to_plaza() = 0;

    //At Central Plaza, read the quest panel. Returns whether this quest is still on it: the game
    //removes a quest once it is completed.
    virtual bool quest_on_board() = 0;

    //Walk to the nurse and heal, then fly to the Central Plaza fly point again: the flight to
    //Chargestone moves the map cursor from there.
    virtual void heal() = 0;
};

struct BBQAutoBattlePolicy{
    uint64_t kill_target = 10;
    //Give up after this many passes in a row with no kill.
    uint64_t max_empty_passes = 3;
    //Give up after the pass that ends past this time.
    std::chrono::minutes time_limit = std::chrono::minutes(35);
    //Between passes, heal only if the lead's HP is below this percentage or unknown.
    uint8_t heal_below_hp_percent = 50;
    //After a pass that ends this close to the target, read the board before going again, and
    //stop if the quest is gone. 0 never reads it.
    uint64_t check_board_within_kills = 2;
};

enum class BBQAutoBattleResult{
    TargetReached,
    NoKills,        //max_empty_passes passes in a row had no kill.
    TimeLimit,
    QuestDone,      //The quest left the board before the heard kills reached the target.
};

//Clear Chargestone Cavern until the kills reach the target. Between passes, heals when the lead's
//HP is below `policy.heal_below_hp_percent` or unknown. Gives up when a limit in `policy` is
//reached; the board check then decides the quest failed. Either way it ends at Central Plaza.
//Logs one line per pass: its kills (heard since the previous pass's line), the total, the lead's
//HP, the time spent, and what happens next. Kills heard on the way back that reach the target end
//it without healing. After a pass that ends within `policy.check_board_within_kills` of the target,
//reads the board and stops if the quest is gone.
BBQAutoBattleResult run_auto_battle(
    Logger& logger,
    BBQAutoBattleActions& actions,
    const BBQAutoBattlePolicy& policy
);

}
}
}
#endif

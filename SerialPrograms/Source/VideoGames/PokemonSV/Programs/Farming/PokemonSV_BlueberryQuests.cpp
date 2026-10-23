/*  Blueberry Quests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_RollDateForward1.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/PokemonSV_Settings.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_GradientArrowDetector.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldSensors.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_LetsGoHpReader.h"
#include "VideoGames/PokemonSV/Inference/Boxes/PokemonSV_BoxDetection.h"
#include "VideoGames/PokemonSV/Programs/Boxes/PokemonSV_BoxRoutines.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryQuestDetector.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryPointsReader.h"
#include "VideoGames/PokemonSV/Programs/Eggs/PokemonSV_EggRoutines.h"
#include "VideoGames/PokemonSV/Programs/Boxes/PokemonSV_BoxRelease.h"
#include "VideoGames/PokemonSV/Inference/Boxes/PokemonSV_BoxEggDetector.h"
#include "VideoGames/PokemonSV/Programs/Sandwiches/PokemonSV_SandwichRoutines.h"
#include "VideoGames/PokemonSV/Programs/ShinyHunting/PokemonSV_LetsGoTools.h"
#include "VideoGames/PokemonSV/Programs/TeraRaids/PokemonSV_TeraRoutines.h"
#include "VideoGames/PokemonSV/Programs/TeraRaids/PokemonSV_TeraBattler.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryCatchPhoto.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "PokemonSV_BlueberryQuests.h"

#include<cmath>
#include<optional>
#include<vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

const std::map<std::string, BBQuests>& BBQuests_TOKEN_TO_ENUM(){
    static std::map<std::string, BBQuests> data{
        {"auto-10",             BBQuests::auto_10},
        {"make-tm",             BBQuests::make_tm },
        {"pickup-10",           BBQuests::pickup_10},
        {"sneak-up",            BBQuests::sneak_up},
        {"photo-fly",           BBQuests::photo_fly},
        {"photo-swim",          BBQuests::photo_swim},
        {"photo-canyon",        BBQuests::photo_canyon},
        {"photo-coastal",       BBQuests::photo_coastal},
        {"photo-polar",         BBQuests::photo_polar},
        {"photo-savanna",       BBQuests::photo_savanna},
        {"tera-self-defeat",    BBQuests::tera_self_defeat},
        {"travel-500",          BBQuests::travel_500},
        {"catch-any",           BBQuests::catch_any},
        {"catch-normal",        BBQuests::catch_normal},
        {"catch-fighting",      BBQuests::catch_fighting},
        {"catch-flying",        BBQuests::catch_flying},
        {"catch-poison",        BBQuests::catch_poison},
        {"catch-ground",        BBQuests::catch_ground},
        {"catch-rock",          BBQuests::catch_rock},
        {"catch-bug",           BBQuests::catch_bug},
        {"catch-ghost",         BBQuests::catch_ghost},
        {"catch-steel",         BBQuests::catch_steel},
        {"catch-fire",          BBQuests::catch_fire},
        {"catch-water",         BBQuests::catch_water},
        {"catch-grass",         BBQuests::catch_grass},
        {"catch-electric",      BBQuests::catch_electric},
        {"catch-psychic",       BBQuests::catch_psychic},
        {"catch-ice",           BBQuests::catch_ice},
        {"catch-dragon",        BBQuests::catch_dragon},
        {"catch-dark",          BBQuests::catch_dark},
        {"catch-fairy",         BBQuests::catch_fairy},
        {"wash-pokemon",        BBQuests::wash_pokemon},
        {"wild-tera",           BBQuests::wild_tera},
        {"auto-30",             BBQuests::auto_30},
        {"tera-raid",           BBQuests::tera_raid},
        {"sandwich-three",      BBQuests::sandwich_three},
        {"bitter-sandwich",     BBQuests::bitter_sandwich},
        {"sweet-sandwich",      BBQuests::sweet_sandwich},
        {"salty-sandwich",      BBQuests::salty_sandwich},
        {"sour-sandwich",       BBQuests::sour_sandwich},
        {"spicy-sandwich",      BBQuests::spicy_sandwich},
        {"hatch-egg",           BBQuests::hatch_egg},
        {"photo-normal",        BBQuests::photo_normal},
        {"photo-fighting",      BBQuests::photo_fighting},
        {"photo-flying",        BBQuests::photo_flying},
        {"photo-poison",        BBQuests::photo_poison},
        {"photo-ground",        BBQuests::photo_ground},
        {"photo-rock",          BBQuests::photo_rock},
        {"photo-bug",           BBQuests::photo_bug},
        {"photo-ghost",         BBQuests::photo_ghost},
        {"photo-steel",         BBQuests::photo_steel},
        {"photo-fire",          BBQuests::photo_fire},
        {"photo-water",         BBQuests::photo_water},
        {"photo-grass",         BBQuests::photo_grass},
        {"photo-electric",      BBQuests::photo_electric},
        {"photo-psychic",       BBQuests::photo_psychic},
        {"photo-ice",           BBQuests::photo_ice},
        {"photo-dragon",        BBQuests::photo_dragon},
        {"photo-dark",          BBQuests::photo_dark},
        {"photo-fairy",         BBQuests::photo_fairy},
        {"ditto-central",       BBQuests::ditto_central},
        {"ditto-canyon",        BBQuests::ditto_canyon},
        {"ditto-coastal",       BBQuests::ditto_coastal},
        {"ditto-polar",         BBQuests::ditto_polar},
        {"ditto-savanna",       BBQuests::ditto_savanna},
        {"group-canyon",        BBQuests::group_canyon},
        {"group-coastal",       BBQuests::group_coastal},
        {"group-polar",         BBQuests::group_polar},
        {"group-savanna",       BBQuests::group_savanna},
        {"group-eyewear",       BBQuests::group_eyewear},
        {"group-nonuniform",    BBQuests::group_nonuniform},
        {"group-masks",         BBQuests::group_masks},
        {"sandwich-four",       BBQuests::sandwich_four},
        {"catch-hint",          BBQuests::catch_hint},
        {"catch-hint2",         BBQuests::catch_hint2},
        {"",                    BBQuests::UnableToDetect},
    };
    return data;
}

BBQuests BBQuests_string_to_enum(const std::string& token){
    auto iter = BBQuests_TOKEN_TO_ENUM().find(token);
    if (iter == BBQuests_TOKEN_TO_ENUM().end()){
        return BBQuests::UnableToDetect;
    }
    return iter->second;
}

std::string BBQuests_enum_to_string(BBQuests quest){
    for (const auto& [token, value] : BBQuests_TOKEN_TO_ENUM()){
        if (value == quest && !token.empty()){
            return token;
        }
    }
    return "unknown";
}

int read_BP(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    WhiteButtonWatcher right_panel(COLOR_BLUE, WhiteButton::ButtonB, {0.484, 0.117, 0.022, 0.037});
    int result = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            for (int i = 0; i < 6; i++){ //try 6 times
                pbf_press_dpad(context, DPAD_RIGHT, 400ms, 160ms);
                pbf_wait(context, 1600ms);
                context.wait_for_all_requests();
            }
        },
        {{ right_panel }}
    );
    if (result == 0){
        stream.log("Found quest panel.");
    }
    context.wait_for_all_requests();
    
    VideoSnapshot screen = stream.video().snapshot();

    //Close panel
    pbf_mash_button(context, BUTTON_B, 800ms);
    context.wait_for_all_requests();

    return read_BP_from_image(stream.logger(), screen);
}

std::vector<BBQuests> read_quests(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    int* BP
){
    std::vector<BBQuests> quest_list;

    //Open quest list. Wait for it to open.
    WhiteButtonWatcher right_panel(COLOR_BLUE, WhiteButton::ButtonB, {0.484, 0.117, 0.022, 0.037});
    int result = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            for (int i = 0; i < 6; i++){ //try 6 times
                pbf_press_dpad(context, DPAD_RIGHT, 400ms, 160ms);
                pbf_wait(context, 1600ms);
                context.wait_for_all_requests();
            }
        },
        {{ right_panel }}
    );
    if (result == 0){
        stream.log("Found quest panel.");
    }
    context.wait_for_all_requests();

    //Read in the initial 4 quests.
    stream.log("Reading quests.");
    VideoSnapshot screen = stream.video().snapshot();
    BlueberryQuestDetector first_quest_detector(stream.logger(), COLOR_GREEN, BBQ_OPTIONS.LANGUAGE, BlueberryQuestDetector::QuestPosition::FIRST);
    BlueberryQuestDetector second_quest_detector(stream.logger(), COLOR_GREEN, BBQ_OPTIONS.LANGUAGE, BlueberryQuestDetector::QuestPosition::SECOND);
    BlueberryQuestDetector third_quest_detector(stream.logger(), COLOR_GREEN, BBQ_OPTIONS.LANGUAGE, BlueberryQuestDetector::QuestPosition::THIRD);
    BlueberryQuestDetector fourth_quest_detector(stream.logger(), COLOR_GREEN, BBQ_OPTIONS.LANGUAGE, BlueberryQuestDetector::QuestPosition::FOURTH);

    quest_list.push_back(BBQuests_string_to_enum(first_quest_detector.detect_quest(screen)));
    quest_list.push_back(BBQuests_string_to_enum(second_quest_detector.detect_quest(screen)));
    quest_list.push_back(BBQuests_string_to_enum(third_quest_detector.detect_quest(screen)));
    
    std::string fourth_quest = fourth_quest_detector.detect_quest(screen);
    if (fourth_quest != ""){
        quest_list.push_back(BBQuests_string_to_enum(fourth_quest));
    }

    //The balance is on the same panel, so reading it costs no extra button presses.
    if (BP != nullptr){
        *BP = read_BP_from_image(stream.logger(), screen);
    }

    //Close quest list
    pbf_mash_button(context, BUTTON_B, 800ms);
    context.wait_for_all_requests();

    return quest_list;
}

namespace{

//Out of eggs, the egg quest cannot be done. OUT_OF_EGGS decides what happens to it.
void plan_out_of_eggs(Logger& logger, const BBQOption& BBQ_OPTIONS, size_t slot, BBQuestPlan& plan){
    switch (BBQ_OPTIONS.OUT_OF_EGGS){
    case BBQOption::OOEggs::Reroll:
        logger.log("Reroll selected. Rerolling Egg quest. New quest will be run in the next batch of quests.");
        //logger.log("Warning: This does not handle/check being out of BP!", COLOR_RED);
        plan.reroll_slots.push_back(slot);
        break;
    case BBQOption::OOEggs::KeepGoing:
        logger.log("Keep Going selected. Ignoring quest.");
        break;
    default:
        //This case is handled in BBQSoloFarmer.
        throw InternalProgramError(&logger, PA_CURRENT_FUNCTION, "OOEggs is Stop in process_quest_list().");
    }
}

//Apply each exclusions table row for the quest, in table order. Returns false if no row is for it.
//A quest the program cannot do is never run, but can still be rerolled or skipped.
bool plan_from_exclusions(
    Logger& logger,
    const std::vector<std::unique_ptr<BBQuestTableRow>>& exclusions_table,
    BBQuests quest, size_t slot, bool possible,
    BBQuestPlan& plan
){
    bool quest_in_table = false;
    for (const std::unique_ptr<BBQuestTableRow>& row : exclusions_table){
        if (quest != row->quest){
            continue;
        }
        logger.log("Quest found in inclusions/exclusions table.");
        quest_in_table = true;
        switch (row->action){
        case BBQAction::run:
            if (!possible){
                logger.log("Run selected, but this quest is not possible. Skipping quest.");
                break;
            }
            logger.log("Run selected. Adding quest to list.");
            plan.to_do.push_back(quest);
            break;
        case BBQAction::reroll:
            logger.log("Reroll selected. Rerolling quest. New quest will be run in the next batch of quests.");
            plan.reroll_slots.push_back(slot);
            break;
        case BBQAction::skip:
            logger.log("Skip selected. Skipping quest.");
            break;
        }
    }
    return quest_in_table;
}

//Open the quest panel from the overworld. Returns false if it did not open.
bool open_quest_panel(VideoStream& stream, ProControllerContext& context){
    WhiteButtonWatcher panel(COLOR_BLUE, WhiteButton::ButtonB, {0.484, 0.117, 0.022, 0.037});
    int result = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            for (int i = 0; i < 6; i++){
                pbf_press_dpad(context, DPAD_RIGHT, 400ms, 160ms);
                pbf_wait(context, 1600ms);
                context.wait_for_all_requests();
            }
        },
        {{ panel }}
    );
    if (result == 0){
        stream.log("Found quest panel.");
    }
    context.wait_for_all_requests();
    return result == 0;
}

class ConsoleBBQRerollActions : public BBQRerollActions{
public:
    ConsoleBBQRerollActions(
        const ProgramInfo& info,
        VideoStream& stream, ProControllerContext& context,
        const BBQOption& BBQ_OPTIONS,
        const std::function<void(int BP)>& record_BP
    )
        : m_info(info), m_stream(stream), m_context(context), m_options(BBQ_OPTIONS)
        , m_record_BP(record_BP)
    {}

    virtual bool open_panel() override{
        return open_quest_panel(m_stream, m_context);
    }
    virtual std::optional<size_t> open_reroll_menu() override{
        //The menu opens next to the quest, with an arrow on its top choice. Slots are 0.177
        //apart; the boxes for the top 3 slots were checked on a Switch, the 4th is spaced
        //the same way.
        GradientArrowWatcher slot0(COLOR_RED, GradientArrowType::RIGHT, {0.475, 0.218, 0.070, 0.095});
        GradientArrowWatcher slot1(COLOR_RED, GradientArrowType::RIGHT, {0.475, 0.395, 0.070, 0.095});
        GradientArrowWatcher slot2(COLOR_RED, GradientArrowType::RIGHT, {0.475, 0.572, 0.070, 0.095});
        GradientArrowWatcher slot3(COLOR_RED, GradientArrowType::RIGHT, {0.475, 0.749, 0.070, 0.095});
        pbf_press_button(m_context, BUTTON_A, 160ms, 400ms);
        m_context.wait_for_all_requests();
        int ret = wait_until(
            m_stream, m_context, std::chrono::seconds(3),
            {slot0, slot1, slot2, slot3}
        );
        if (ret < 0){
            return std::nullopt;
        }
        return (size_t)ret;
    }
    virtual void close_reroll_menu() override{
        pbf_press_button(m_context, BUTTON_B, 160ms, 400ms);
        m_context.wait_for_all_requests();
    }
    virtual void move_cursor(int slots) override{
        for (int i = 0; i < std::abs(slots); i++){
            pbf_press_dpad(m_context, slots > 0 ? DPAD_DOWN : DPAD_UP, 160ms, 160ms);
            pbf_wait(m_context, 800ms);
        }
        m_context.wait_for_all_requests();
    }
    virtual void confirm_reroll() override{
        pbf_press_button(m_context, BUTTON_A, 160ms, 400ms);
        pbf_wait(m_context, 800ms);
        m_context.wait_for_all_requests();
    }
    virtual void close_panel() override{
        press_Bs_to_back_to_overworld(m_info, m_stream, m_context);
    }
    virtual std::vector<BBQuests> read_quests() override{
        m_stream.log("Reading quest panel to verify reroll.");
        if (!m_record_BP){
            return PokemonSV::read_quests(m_info, m_stream, m_context, m_options);
        }
        int BP = -1;
        std::vector<BBQuests> quests = PokemonSV::read_quests(m_info, m_stream, m_context, m_options, &BP);
        m_record_BP(BP);
        return quests;
    }

private:
    const ProgramInfo& m_info;
    VideoStream& m_stream;
    ProControllerContext& m_context;
    const BBQOption& m_options;
    const std::function<void(int BP)>& m_record_BP;
};


//This does not handle out of BP.
void reroll_all_slots(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    size_t slots
){
    open_quest_panel(stream, context);

    for (size_t i = 0; i < slots; i++){
        pbf_press_button(context, BUTTON_A, 160ms, 400ms);
        pbf_press_button(context, BUTTON_A, 160ms, 400ms); //Yes.
        pbf_wait(context, 800ms);
        context.wait_for_all_requests();
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 160ms);
        pbf_wait(context, 800ms);
        context.wait_for_all_requests();
    }
    //Close quest panel - mash b
    press_Bs_to_back_to_overworld(info, stream, context);
}

}

BBQRerollResult reroll_quest(
    Logger& logger,
    BBQRerollActions& actions,
    std::vector<BBQuests>& quests,
    size_t slot,
    std::optional<std::vector<BBQuests>>& observed_panel
){
    observed_panel.reset();
    //Every slot reroll logs this one line, whatever decided it, so a log shows each BP spent.
    logger.log(
        "Rerolling quest " + std::to_string(slot + 1) + " (" +
        (slot < quests.size() ? BBQuests_enum_to_string(quests[slot]) : "unknown") + ")."
    );
    if (!actions.open_panel()){
        return BBQRerollResult::PanelNotOpened;
    }

    //The cursor is not always on the top quest when the panel opens; it can be left where it
    //was. So open the menu wherever the cursor is, see which quest it opened next to, and move
    //from there. Moving can drop a press, so this is tried a few times.
    bool menu_on_slot = false;
    for (int attempt = 0; attempt < 3 && !menu_on_slot; attempt++){
        std::optional<size_t> menu_slot = actions.open_reroll_menu();
        if (!menu_slot){
            //Without a menu, B would close the panel instead, so stop here.
            break;
        }
        if (*menu_slot == slot){
            menu_on_slot = true;
            break;
        }
        logger.log("Reroll menu opened on quest " + std::to_string(*menu_slot + 1) + ". Moving to quest " + std::to_string(slot + 1) + ".");
        actions.close_reroll_menu();
        actions.move_cursor((int)slot - (int)*menu_slot);
    }
    if (!menu_on_slot){
        actions.close_panel();
        return BBQRerollResult::MenuNotFound;
    }
    actions.confirm_reroll();
    actions.close_panel();

    //A read can miss the quest in the slot, so read up to 3 times before deciding it is
    //still the old quest.
    for (int read = 0; read < 3; read++){
        std::vector<BBQuests> after = actions.read_quests();
        if (slot < after.size() && after[slot] != quests[slot] && after[slot] != BBQuests::UnableToDetect){
            logger.log("Quest " + std::to_string(slot + 1) + " rerolled.");
            quests[slot] = after[slot];
            //Reuse only a read whose slots are all known and, apart from the
            //rerolled one, agree with the panel used for this reroll. Three
            //slots can mean a missed fourth quest, so a 3-slot read is only
            //reused when that panel had 3 slots too. Both reads could have
            //missed the same fourth quest, as a fresh read also could. That
            //quest is then left out of one plan, which costs no BP, and a
            //3-slot read never proves a failed quest left the panel.
            if (after.size() >= 3 && after == quests
                && std::find(after.begin(), after.end(), BBQuests::UnableToDetect) == after.end()){
                observed_panel = std::move(after);
            }
            return BBQRerollResult::Rerolled;
        }
    }
    return BBQRerollResult::NotReplaced;
}

std::optional<std::vector<BBQuests>> reroll_quest_slot(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    std::vector<BBQuests>& quest_list,
    size_t slot,
    const std::function<void(int BP)>& record_BP
){
    ConsoleBBQRerollActions actions(info, stream, context, BBQ_OPTIONS, record_BP);
    std::optional<std::vector<BBQuests>> observed_panel;
    switch (reroll_quest(stream.logger(), actions, quest_list, slot, observed_panel)){
    case BBQRerollResult::Rerolled:
        return observed_panel;
    case BBQRerollResult::PanelNotOpened:
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Unable to open the quest panel to reroll a quest.",
            stream
        );
    case BBQRerollResult::MenuNotFound:
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Unable to open the reroll menu for quest " + std::to_string(slot + 1) + ". No BP was spent.",
            stream
        );
    case BBQRerollResult::NotReplaced:
        //Most likely out of BP, which is not a program error.
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Quest " + std::to_string(slot + 1) + " was not replaced after rerolling it. Check that there is enough BP.",
            stream
        );
    }
}

int reroll_cost(BBQuests quest){
    return red_quests.contains(quest) ? 50 : 10;
}

BBQuestPlan plan_quests(
    Logger& logger,
    const BBQOption& BBQ_OPTIONS,
    const std::vector<BBQuests>& quest_list,
    uint8_t eggs_hatched,
    std::optional<int> bp
){
    BBQuestPlan plan;
    std::vector<std::unique_ptr<BBQuestTableRow>> exclusions_table = BBQ_OPTIONS.QUEST_EXCLUSIONS.copy_snapshot();

    //Quests not possible and not in the table, in panel order. Rerolled below if BP allows.
    std::vector<size_t> not_possible_slots;

    logger.log("Processing quests.");
    for (size_t slot = 0; slot < quest_list.size(); slot++){
        const BBQuests n = quest_list[slot];
        if (n == BBQuests::UnableToDetect){
            logger.log("Quest not detected.");
            continue;
        }
        //Check eggs remaining.
        if (n == BBQuests::hatch_egg && eggs_hatched >= BBQ_OPTIONS.NUM_EGGS){
            logger.log("Out of eggs! Quest not possible.");
            plan_out_of_eggs(logger, BBQ_OPTIONS, slot, plan);
            continue;
        }
        const bool possible = !not_possible_quests.contains(n);
        if (plan_from_exclusions(logger, exclusions_table, n, slot, possible, plan)){
            continue;
        }
        if (!possible){
            logger.log("Quest not possible.");
            not_possible_slots.push_back(slot);
            continue;
        }
        logger.log("Quest not in inclusion/exclusions table. Adding to list.");
        plan.to_do.push_back(n);
    }

    if (plan.to_do.empty()){
        //Nothing else to do: a quest not possible only changes by rerolling it.
        for (size_t slot : not_possible_slots){
            logger.log("No possible quests! Rerolling quest " + std::to_string(slot + 1) + ", which is not possible.");
            plan.reroll_slots.push_back(slot);
        }
    }else{
        //Left on the panel, a quest not possible holds a slot until something else is rerolled.
        //A red quest costs 50 BP to reroll instead of 10, but pays 100-150 BP and keeps the
        //next red quest from appearing, so it is rerolled too. Without the BP for it, it is
        //skipped and the doable quests run, rather than stopping the program on a reroll
        //that does not happen.
        int budget = bp.value_or(0);
        for (size_t slot : plan.reroll_slots){
            budget -= reroll_cost(quest_list[slot]);
        }
        for (size_t slot : not_possible_slots){
            const int cost = reroll_cost(quest_list[slot]);
            if (!bp || budget < cost){
                logger.log(
                    "Quest " + std::to_string(slot + 1) + " not possible, but BP (" +
                    (bp ? std::to_string(*bp) : "unknown") + ") does not cover its " +
                    std::to_string(cost) + " BP reroll. Skipping it."
                );
                continue;
            }
            logger.log("Quest " + std::to_string(slot + 1) + " not possible. Rerolling quest. New quest will be run in the next batch of quests.");
            budget -= cost;
            plan.reroll_slots.push_back(slot);
        }
    }
    std::sort(plan.reroll_slots.begin(), plan.reroll_slots.end());

    //Check that quests_to_do is not empty (after completing all quests on the list, be sure to erase it.
    //Lag might be a problem in multi - look into making slots like menu-left navigation
    if (plan.reroll_slots.empty() && plan.to_do.empty()){
        plan.reroll_all = true;
    }
    return plan;
}

BBQuestBatch process_quest_list(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    const std::vector<BBQuests>& quest_list,
    uint8_t eggs_hatched,
    const std::function<void(int BP)>& record_BP
){
    //Read BP only when a quest not possible may be rerolled while others are doable.
    std::optional<int> bp;
    if (std::any_of(quest_list.begin(), quest_list.end(), [](BBQuests quest){
        return quest != BBQuests::UnableToDetect && not_possible_quests.contains(quest);
    })){
        const int read = read_BP(info, stream, context);
        if (read >= 0){
            bp = read;
        }
        stream.log("BP: " + (bp ? std::to_string(*bp) : "unable to read"));
    }
    BBQuestPlan plan = plan_quests(stream.logger(), BBQ_OPTIONS, quest_list, eggs_hatched, bp);

    //Each reroll updates the panel the next one checks against.
    std::vector<BBQuests> panel = quest_list;
    std::optional<std::vector<BBQuests>> observed_panel;
    for (size_t slot : plan.reroll_slots){
        observed_panel = reroll_quest_slot(info, stream, context, BBQ_OPTIONS, panel, slot, record_BP);
    }
    if (plan.reroll_all){
        stream.log("No possible quests! Rerolling all quests.");
        reroll_all_slots(info, stream, context, quest_list.size());
    }

    //Disabled since 2025-05, when the exclusions table gained its reroll action. Before that,
    //nothing doable stopped the program here, because it usually meant the game language
    //option did not match the game and no quest could be read. Now all quests are rerolled
    //above instead, which also covers a panel of only excluded quests.
    /*
    if (plan.reroll_all){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "No possible quests! Check language selection.",
            stream
        );
    }
    */
    //A mutation invalidates queued work, even when other quests remain runnable.
    //A complete replacement observation can seed the next selection instead.
    if (!plan.reroll_slots.empty() || plan.reroll_all){
        return {{}, std::move(observed_panel)};
    }
    return {std::move(plan.to_do), std::nullopt};
}

bool process_and_do_quest(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS,
    BBQuests current_quest,
    uint8_t& eggs_hatched
){
    bool quest_completed = false;
    int quest_attempts = 0;

    while (!quest_completed){
        //  A failed fly fails this attempt, not the program: the board
        //  check below counts it, and the retry limit applies as usual.
        try{
            switch (current_quest){
            case BBQuests::make_tm:
                quest_make_tm(env.program_info(), console, context);
                break;
            case BBQuests::travel_500:
                quest_travel_500(env.program_info(), console, context);
                break;
            case BBQuests::tera_self_defeat:
                quest_tera_self_defeat(env.program_info(), console, context, BBQ_OPTIONS);
                break;
            case BBQuests::sneak_up:
                quest_sneak_up(env.program_info(), console, context, BBQ_OPTIONS);
                break;
            case BBQuests::wild_tera:
                quest_wild_tera(env.program_info(), console, context, BBQ_OPTIONS);
                break;
            case BBQuests::wash_pokemon:
                quest_wash_pokemon(env.program_info(), console, context, BBQ_OPTIONS);
                break;
            case BBQuests::hatch_egg:
                quest_hatch_egg(env, console, context, BBQ_OPTIONS);
                break;
            case BBQuests::bitter_sandwich:
            case BBQuests::salty_sandwich:
            case BBQuests::sour_sandwich:
            case BBQuests::spicy_sandwich:
            case BBQuests::sweet_sandwich:
            case BBQuests::sandwich_three:
                quest_sandwich(env, console, context, BBQ_OPTIONS, current_quest);
                break;
            //case BBQuests::pickup_10:
            //    quest_pickup(env, env.program_info(), console, context, BBQ_OPTIONS);
            //    break;
            case BBQuests::tera_raid:
                quest_tera_raid(env, console, context, BBQ_OPTIONS);
                break;
            case BBQuests::auto_10:
            case BBQuests::auto_30:
                quest_auto_battle(env, console, context, BBQ_OPTIONS, current_quest);
                break;
            //All involve taking pictures
            case BBQuests::photo_fly:
            case BBQuests::photo_swim:
            case BBQuests::photo_canyon:
            case BBQuests::photo_coastal:
            case BBQuests::photo_polar:
            case BBQuests::photo_savanna:
            case BBQuests::photo_normal:
            case BBQuests::photo_fighting:
            case BBQuests::photo_flying:
            case BBQuests::photo_poison:
            case BBQuests::photo_ground:
            case BBQuests::photo_rock:
            case BBQuests::photo_bug:
            case BBQuests::photo_ghost:
            case BBQuests::photo_steel:
            case BBQuests::photo_fire:
            case BBQuests::photo_water:
            case BBQuests::photo_grass:
            case BBQuests::photo_electric:
            case BBQuests::photo_psychic:
            case BBQuests::photo_ice:
            case BBQuests::photo_dragon:
            case BBQuests::photo_dark:
            case BBQuests::photo_fairy:
                quest_photo(env.program_info(), console, context, BBQ_OPTIONS, current_quest);
                break;
            //All involve catching a pokemon
            case BBQuests::catch_any:
            case BBQuests::catch_normal:
            case BBQuests::catch_fighting:
            case BBQuests::catch_flying:
            case BBQuests::catch_poison:
            case BBQuests::catch_ground:
            case BBQuests::catch_rock:
            case BBQuests::catch_bug:
            case BBQuests::catch_ghost:
            case BBQuests::catch_steel:
            case BBQuests::catch_fire:
            case BBQuests::catch_water:
            case BBQuests::catch_grass:
            case BBQuests::catch_electric:
            case BBQuests::catch_psychic:
            case BBQuests::catch_ice:
            case BBQuests::catch_dragon:
            case BBQuests::catch_dark:
            case BBQuests::catch_fairy:
                quest_catch(env.program_info(), console, context, BBQ_OPTIONS, current_quest);
                break;
            default:
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Unknown quest selection.",
                    console
                );
                break;
            }
        }catch (TerariumFlyFailedException&){
            console.log("Quest attempt failed: unable to fly. Returning to Central Plaza.", COLOR_RED);
            return_to_plaza(env.program_info(), console, context, BBQ_OPTIONS.LANGUAGE);
        }

        //Validate quest was completed by checking list
        console.log("Reading quest panel after attempt.");
        std::vector<BBQuests> quest_list = read_quests(env.program_info(), console, context, BBQ_OPTIONS);
        if (std::find(quest_list.begin(), quest_list.end(), current_quest) != quest_list.end()){
            console.log("Current quest exists on list. Quest did not complete.");
            quest_attempts++;
        }else{
            console.log("Current quest was not found. Quest completed!");
            if (current_quest == BBQuests::hatch_egg){
                eggs_hatched++;
                console.log("Eggs hatched: " + std::to_string(eggs_hatched));
            }

            quest_completed = true;
        }

        if (quest_attempts > BBQ_OPTIONS.NUM_RETRIES){
            console.log("Failed to complete a quest multiple times. Skipping it for now.", COLOR_RED);
            break;
        }
    }
    return quest_completed;
}

void quest_make_tm(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    stream.log("Quest: Make TM");

    //Mount and then dismount in case you're crouched
    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    context.wait_for_all_requests();

    GradientArrowWatcher machine(COLOR_BLUE, GradientArrowType::DOWN, {0.181, 0.127, 0.045, 0.070});
    PromptDialogWatcher makeTM(COLOR_RED);
    OverworldWatcher overworld(stream.logger(), COLOR_BLUE);

    pbf_move_left_joystick(context, {+1, +1}, 800ms, 160ms);
    pbf_press_button(context, BUTTON_L, 80ms, 400ms);

    int enter_machine = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            for (int i = 0; i < 10; i++){
                pbf_press_button(context, BUTTON_A, 160ms, 400ms);
                pbf_wait(context, 1600ms);
                context.wait_for_all_requests();
            }
        },
        {{ machine }}
    );
    context.wait_for_all_requests();

    if (enter_machine == 0){
        stream.log("TM machine entered. Finding TM to make.");

        int make_tm = run_until<ProControllerContext>(
            stream, context,
            [&](ProControllerContext& context){
                for (int i = 0; i < 229; i++){ //229 is max number of TMs
                    //click on tm
                    pbf_press_button(context, BUTTON_A, 160ms, 400ms);
                    pbf_wait(context, 800ms);
                    context.wait_for_all_requests();

                    //not craftable, close and move on to next
                    pbf_press_button(context, BUTTON_A, 160ms, 400ms);
                    pbf_press_dpad(context, DPAD_RIGHT, 160ms, 160ms);
                    pbf_wait(context, 800ms);
                    context.wait_for_all_requests();
                }
            },
            {{ makeTM }}
        );
        context.wait_for_all_requests();

        if (make_tm == 0){
            stream.log("Craftable TM found. Making TM");

            pbf_mash_button(context, BUTTON_A, 1760ms);
            context.wait_for_all_requests();
        }else{
            stream.log("Failed to find craftable TM!");
        }
    }else{
        stream.log("Failed to enter TM machine!");
    }
    
    int exit = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 16000ms);
        },
        {{ overworld }}
    );
    if (exit == 0){
        stream.log("Overworld detected.");
    }
    context.wait_for_all_requests();
    
    open_map_from_overworld(info, stream, context);
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    fly_to_overworld_from_map(info, stream, context);
    context.wait_for_all_requests();
}

void quest_travel_500(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context){
    stream.log("Quest: Travel 500 meters.");

    //Mount and then dismount in case you're crouched
    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    context.wait_for_all_requests();

    pbf_move_left_joystick(context, {-1, +1}, 800ms, 160ms);
    pbf_move_left_joystick(context, {0, +1}, 1200ms, 160ms);
    pbf_move_left_joystick(context, {-1, 0}, 1120ms, 160ms);

    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

    for (int j = 0; j < 60; j++){ //One min just to be safe.
        pbf_controller_state(
            context, BUTTON_LCLICK, DPAD_NONE,
            {0, +1}, {+1, 0}, 1000ms
        );
    }
    context.wait_for_all_requests();

    open_map_from_overworld(info, stream, context);
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    fly_to_overworld_from_map(info, stream, context);
    context.wait_for_all_requests();
}

TeraEncounterAction tera_encounter_action(
    bool shiny, int encounter_watcher_result, bool battle_menu_detected
){
    if (shiny){
        return TeraEncounterAction::StopForShiny;
    }
    //Callback 1 is the audio side of the encounter watcher. It does not see the battle menu.
    if (encounter_watcher_result == 0 || battle_menu_detected){
        return TeraEncounterAction::Battle;
    }
    return TeraEncounterAction::Recover;
}

void quest_tera_self_defeat(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    EncounterWatcher encounter_watcher(console, COLOR_RED);
    bool battle_menu_detected = false;
    console.log("Quest: Tera-self and defeat any wild.");
    //Navigate to target and start battle
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.606, +0.5}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            //Drop on top of Kleavor (plenty of Scyther in the area as well)
            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 13200ms, 2400ms);
            }else{ //All switch 2s
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 12800ms, 2400ms);
            }

            ssf_press_button(context, BUTTON_ZR, 0ms, 1600ms);
            ssf_press_button(context, BUTTON_ZL, 800ms, 400ms);
            ssf_press_button(context, BUTTON_ZL, 1200ms, 400ms);

            pbf_wait(context, 2400ms);
            context.wait_for_all_requests();

            NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
            int ret2 = wait_until(
                console, context,
                std::chrono::seconds(15),
                { battle_menu }
            );
            battle_menu_detected = ret2 == 0;
            if (!battle_menu_detected){
                console.log("Did not enter battle. Did Kleavor spawn?");
            }
        },
        {
            static_cast<VisualInferenceCallback&>(encounter_watcher),
            static_cast<AudioInferenceCallback&>(encounter_watcher),
        }
        );
    encounter_watcher.throw_if_no_sound();

    switch (tera_encounter_action((bool)encounter_watcher.shiny_screenshot(), ret, battle_menu_detected)){
    case TeraEncounterAction::StopForShiny:
        console.log("Shiny detected!");
        pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
        throw ProgramFinishedException();
    case TeraEncounterAction::Battle:{
        console.log("Battle menu detected.");
        bool tera_self = true;
        wild_battle_tera(info, console, context, tera_self);
        break;
    }
    case TeraEncounterAction::Recover:
        console.log("Battle entry not confirmed. Skipping battle handling.");
        break;
    }
    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    return_to_plaza(info, console, context, BBQ_OPTIONS.LANGUAGE);

    //Day skip and attempt to respawn fixed encounters
    pbf_press_button(context, BUTTON_HOME, 160ms, GameSettings::instance().GAME_TO_HOME_DELAY1);
    home_to_date_time(console, context, true);
    roll_date_forward_1(console, context, true);
    pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    resume_game_from_home(console, context);

    //Heal up and then reset position again.
    OverworldWatcher done_healing(console.logger(), COLOR_BLUE);
    pbf_move_left_joystick(context, {0, +1}, 800ms, 160ms);

    pbf_mash_button(context, BUTTON_A, 2400ms);
    context.wait_for_all_requests();

    int exit = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 16000ms);
        },
        {{ done_healing }}
    );
    if (exit == 0){
        console.log("Overworld detected.");
    }
    open_map_from_overworld(info, console, context);
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    fly_to_overworld_from_map(info, console, context);
}

void quest_sneak_up(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    EncounterWatcher encounter_watcher(console, COLOR_RED);
    console.log("Quest: Sneak up.");
    //Navigate to target and start battle
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            //Savanna Plaza - Pride Rock
            central_to_savanna_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.724, -1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 4800ms, 3200ms, 3200ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {+1, 0}, 160ms, 400ms);

            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {0, +1}, 800ms, 400ms);

            //Turn slightly for switch 1
            if (console.state().console_type() == ConsoleType::Switch1){
                pbf_move_left_joystick(context, {-1, +1}, 160ms, 400ms);
                pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            }

            ssf_press_button(context, BUTTON_ZR, 0ms, 1600ms);
            ssf_press_button(context, BUTTON_ZL, 800ms, 400ms);
            ssf_press_button(context, BUTTON_ZL, 1200ms, 400ms);

            pbf_wait(context, 2400ms);
            context.wait_for_all_requests();

            NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
            int ret2 = wait_until(
                console, context,
                std::chrono::seconds(15),
                { battle_menu }
            );
            if (ret2 != 0){
                console.log("Did not enter battle. Did target spawn?");
            }
        },
        {
            static_cast<VisualInferenceCallback&>(encounter_watcher),
            static_cast<AudioInferenceCallback&>(encounter_watcher),
        }
        );
    encounter_watcher.throw_if_no_sound();

    if (ret >= 0){
        console.log("Battle menu detected.");

        bool is_shiny = (bool)encounter_watcher.shiny_screenshot();
        if (is_shiny){
            console.log("Shiny detected!");
            pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
            throw ProgramFinishedException();
        }else{
            OverworldWatcher overworld(console.logger(), COLOR_BLUE);

            int ret2 = run_until<ProControllerContext>(
                console, context,
                [&](ProControllerContext& context){
                    while (true){
                        //Flee immediately. Keep trying to flee.
                        NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                        int ret2 = wait_until(
                            console, context,
                            std::chrono::seconds(60),
                            { battle_menu }
                        );
                        switch (ret2){
                        case 0:
                            battle_menu.move_to_slot(console, context, 3);
                            pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                            break;
                        default:
                            console.log("Invalid state quest_sneak_up(). Smoke Ball equipped?");
                            OperationFailedExceptionWithScreenshot::fire(
                                ErrorReportMode::SEND_ERROR_REPORT,
                                "Invalid state quest_sneak_up(). Smoke Ball equipped?",
                                console
                            );
                        }
                    }
                },
                { overworld }
                );

            switch (ret2){
            case 0:
                console.log("Overworld detected.");
                break;
            default:
                console.log("Invalid state in run_battle().");
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Invalid state in run_battle().",
                    console
                );
            }
        }
    }
    return_to_plaza(info, console, context, BBQ_OPTIONS.LANGUAGE);

    //Day skip and attempt to respawn fixed encounters - in case quest failed
    pbf_press_button(context, BUTTON_HOME, 160ms, GameSettings::instance().GAME_TO_HOME_DELAY1);
    home_to_date_time(console, context, true);
    roll_date_forward_1(console, context, true);
    pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    resume_game_from_home(console, context);
    context.wait_for_all_requests();
}

void quest_wild_tera(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    EncounterWatcher encounter_watcher(console, COLOR_RED);
    bool battle_menu_detected = false;
    console.log("Quest: Defeat a wild tera.");
    //Navigate to target and start battle
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            //Canyon Rest Area
            central_to_canyon_rest(info, console, context);

            pbf_move_left_joystick(context, {+1, -0.409}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 4000ms, 10400ms, 1200ms);

            //Skarmory is likely to attack but sometimes there is a different pokemon
            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                pbf_move_left_joystick(context, {-0.609, +1}, 160ms, 840ms);
                pbf_press_button(context, BUTTON_L, 160ms, 400ms);
                pbf_move_left_joystick(context, {0, +1}, 800ms, 400ms);
            }else{ //Switch 2
                pbf_move_left_joystick(context, {-0.844, +1}, 160ms, 840ms);
                pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            }

            ssf_press_button(context, BUTTON_ZR, 0ms, 1600ms);
            ssf_press_button(context, BUTTON_ZL, 800ms, 400ms);
            ssf_press_button(context, BUTTON_ZL, 1200ms, 400ms);

            pbf_wait(context, 2400ms);
            context.wait_for_all_requests();

            NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
            int ret2 = wait_until(
                console, context,
                std::chrono::seconds(15),
                { battle_menu }
            );
            battle_menu_detected = ret2 == 0;
            if (!battle_menu_detected){
                console.log("Did not enter battle.");
            }
        },
        {
            static_cast<VisualInferenceCallback&>(encounter_watcher),
            static_cast<AudioInferenceCallback&>(encounter_watcher),
        }
    );
    encounter_watcher.throw_if_no_sound();

    switch (tera_encounter_action((bool)encounter_watcher.shiny_screenshot(), ret, battle_menu_detected)){
    case TeraEncounterAction::StopForShiny:
        console.log("Shiny detected!");
        pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
        throw ProgramFinishedException();
    case TeraEncounterAction::Battle:{
        console.log("Battle menu detected.");
        bool tera_self = false;
        wild_battle_tera(info, console, context, tera_self);
        break;
    }
    case TeraEncounterAction::Recover:
        console.log("Battle entry not confirmed. Skipping battle handling.");
        break;
    }
    return_to_plaza(info, console, context, BBQ_OPTIONS.LANGUAGE);

    //Day skip and attempt to respawn fixed encounters
    pbf_press_button(context, BUTTON_HOME, 160ms, GameSettings::instance().GAME_TO_HOME_DELAY1);
    home_to_date_time(console, context, true);
    roll_date_forward_1(console, context, true);
    pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    resume_game_from_home(console, context);

    //Heal up and then reset position again.
    OverworldWatcher done_healing(console.logger(), COLOR_BLUE);
    pbf_move_left_joystick(context, {0, +1}, 800ms, 160ms);

    pbf_mash_button(context, BUTTON_A, 2400ms);
    context.wait_for_all_requests();

    int exit = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 16000ms);
        },
        {{ done_healing }}
    );
    if (exit == 0){
        console.log("Overworld detected.");
    }
    open_map_from_overworld(info, console, context);
    pbf_press_button(context, BUTTON_ZL, 320ms, 800ms);
    fly_to_overworld_from_map(info, console, context);
}

void quest_wash_pokemon(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, const BBQOption& BBQ_OPTIONS){
    stream.log("Quest: Give your pokemon a bath!");

    //Fly to savanna plaza
    central_to_savanna_plaza(info, stream, context);

    //Turn around, open picnic
    pbf_move_left_joystick(context, {0, -1}, 160ms, 400ms);
    pbf_press_button(context, BUTTON_L, 400ms, 320ms);

    picnic_from_overworld(info, stream, context);

    WallClock start = current_time();
    WhiteButtonWatcher wash_button(COLOR_BLUE, WhiteButton::ButtonX, {0.027, 0.548, 0.022, 0.032});
    WhiteButtonWatcher wash_exit_button(COLOR_BLUE, WhiteButton::ButtonB, {0.027, 0.923, 0.021, 0.033});
    WhiteButtonWatcher shower_switch(COLOR_BLUE, WhiteButton::ButtonY, {0.486, 0.870, 0.027, 0.045});
    bool rinsed_once = false;
    while (!rinsed_once){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(3)){
            stream.log("Failed to get to rinse after 3 minutes.", COLOR_RED);
            break;
        }
        int ret = wait_until(
            stream, context, std::chrono::seconds(60),
            {
                wash_button,
                shower_switch,
                wash_exit_button,
            }
        );
        switch (ret){
        case 0:
            stream.log("Wash button found!");
            pbf_mash_button(context, BUTTON_X, 1200ms);
            pbf_wait(context, 1600ms);
            context.wait_for_all_requests();
            break;
        case 1:
            stream.log("Rinse button found. Switching to rinse.");
            pbf_press_button(context, BUTTON_Y, 320ms, 400ms);
            rinsed_once = true;
            //Move slightly right, as the showerhead is at an angle
            pbf_move_left_joystick(context, {+1, +1}, 240ms, 240ms);
            context.wait_for_all_requests();
            break;
        case 2:
            stream.log("In wash. Scrubbing.");

            ssf_press_button(context, BUTTON_A, 0ms, 1600ms, 0ms);
            ssf_press_left_joystick(context, {-1, 0}, 0ms, 400ms);
            ssf_press_left_joystick(context, {+1, 0}, 400ms, 800ms);
            ssf_press_left_joystick(context, {-1, 0}, 1200ms, 400ms);

            ssf_press_button(context, BUTTON_A, 0ms, 1600ms, 0ms);
            ssf_press_left_joystick(context, {0, +1}, 0ms, 400ms);
            ssf_press_left_joystick(context, {0, -1}, 400ms, 800ms);
            ssf_press_left_joystick(context, {0, +1}, 1200ms, 400ms);
            pbf_wait(context, 3200ms);
            context.wait_for_all_requests();
            break;
        }
    }

    WhiteButtonWatcher rinse_done(COLOR_BLUE, WhiteButton::ButtonY, {0.028, 0.923, 0.020, 0.034});
    WallClock start2 = current_time();
    int ret3 = run_until<ProControllerContext>(
        stream, context,
        [&](ProControllerContext& context){
            while (true){
                if (current_time() - start2 > std::chrono::minutes(1)){
                    stream.log("Failed to finish rinse after 1 minute.", COLOR_RED);
                    break;
                }
                ssf_press_button(context, BUTTON_A, 0ms, 1600ms, 0ms);
                ssf_press_left_joystick(context, {-1, 0}, 0ms, 400ms);
                ssf_press_left_joystick(context, {+1, 0}, 400ms, 800ms);
                ssf_press_left_joystick(context, {-1, 0}, 1200ms, 400ms);

                ssf_press_button(context, BUTTON_A, 0ms, 1600ms, 0ms);
                ssf_press_left_joystick(context, {0, +1}, 0ms, 400ms);
                ssf_press_left_joystick(context, {0, -1}, 400ms, 800ms);
                ssf_press_left_joystick(context, {0, +1}, 1200ms, 400ms);
                pbf_wait(context, 3200ms);
                context.wait_for_all_requests();
            }
        },
        { {rinse_done} }
    );
    if (ret3 == 0){
        stream.log("Shower completed successfully.");
    }else{
        stream.log("Shower did not complete. Backing out.");
        pbf_press_button(context, BUTTON_B, 320ms, 400ms);
    }

    leave_picnic(info, stream, context);

    return_to_plaza(info, stream, context, BBQ_OPTIONS.LANGUAGE);
    context.wait_for_all_requests();
}

void quest_hatch_egg(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    console.log("Quest: Hatch an Egg");

    //Fix time before hatching
    if (BBQ_OPTIONS.FIX_TIME_FOR_HATCH){
        go_home(console, context);
        home_to_date_time(console, context, false);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
        resume_game_from_home(console, context);
    }

    //Fly to Savanna Plaza and navigate to the battle court
    central_to_savanna_plaza(env.program_info(), console, context);

    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
    pbf_move_left_joystick(context, {0, +1}, 4000ms, 400ms);

    pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);

    //Do this after navigating to prevent egg from hatchining enroute
    //Enter box system, navigate to left box, find the first egg, swap it with first pokemon in party
    enter_box_system_from_overworld(env.program_info(), console, context);
    context.wait_for(std::chrono::milliseconds(400));
    
    //move_to_left_box(context);
    BoxCurrentEggDetector egg_detector;
    SomethingInBoxSlotDetector sth_in_box_detector(COLOR_RED);
    bool egg_found = false;
    uint8_t row = 0;
    uint8_t col = 0;
    for ( ; row < 5; row++){
        for (uint8_t j_col = 0; j_col < 6; j_col++){
            col = (row % 2 == 0 ? j_col : 5 - j_col);
            move_box_cursor(env.program_info(), console, context, BoxCursorLocation::SLOTS, row, col);
            context.wait_for_all_requests();
            auto snapshot = console.video().snapshot();
            if (sth_in_box_detector.detect(snapshot) && egg_detector.detect(snapshot)){
                console.log("Found egg.");
                egg_found = true;
                break;
            }
        }
        if (egg_found){
            break;
        }
    }

    if (!egg_found){
        console.log("No egg found during egg hatching quest!", COLOR_RED);
    }else{
        swap_two_box_slots(env.program_info(), console, context,
            BoxCursorLocation::SLOTS, row, col,
            BoxCursorLocation::PARTY, 0, 0);

        leave_box_system_to_overworld(env.program_info(), console, context);

        hatch_eggs_anywhere(env, console, context, true, 1);

        enter_box_system_from_overworld(env.program_info(), console, context);
        context.wait_for(std::chrono::milliseconds(400));

        swap_two_box_slots(env.program_info(), console, context,
            BoxCursorLocation::PARTY, 0, 0,
            BoxCursorLocation::SLOTS, row, col);

        leave_box_system_to_overworld(env.program_info(), console, context);

        pbf_press_button(context, BUTTON_PLUS, 160ms, 400ms);

        return_to_plaza(env.program_info(), console, context, BBQ_OPTIONS.LANGUAGE);
        context.wait_for_all_requests();
    }
}

void quest_sandwich(
    ProgramEnvironment& env,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    stream.log("Quest: Make a singleplayer sandwich");

    //Polar Plaza - egg basket gets stuck under table in Savanna/Canyon Plaza
    central_to_polar_plaza(env.program_info(), stream, context);

    picnic_from_overworld(env.program_info(), stream, context);

    pbf_move_left_joystick(context, {0, +1}, 240ms, 320ms);
    context.wait_for_all_requests();

    pbf_move_left_joystick(context, {0, +1}, 560ms, 0ms);
    enter_sandwich_recipe_list(env.program_info(), stream, context);

    std::map<std::string, uint8_t> fillings;
    std::map<std::string, uint8_t> condiments;

    //Avoiding Encounter Power
    switch (current_quest){
    case BBQuests::sour_sandwich:
        //Apple, Marmalade: Catch Flying 1, Item Drop Ice 1, Egg 1
        fillings = {{"apple", (uint8_t)1}};
        condiments = {{"marmalade", (uint8_t)1}};
        break;
    case BBQuests::sweet_sandwich:
        //Apple, Butter: Egg 1, Item Drop Ice 1, Raid Steel 1
        fillings = {{"apple", (uint8_t)1}};
        condiments = {{"butter", (uint8_t)1}};
        break;
    case BBQuests::spicy_sandwich:
        //Apple, Curry Powder: Raid Steel 1, Item Drop Ice 1, Teensy Flying 1
        fillings = {{"apple", (uint8_t)1}};
        condiments = {{"curry-powder", (uint8_t)1}};
        break;
    case BBQuests::salty_sandwich:
        //Cheese, Salt: Exp Point Electric 1, Raid Normal 1, Catching Psychic 1
        fillings = {{"cheese", (uint8_t)1}};
        condiments = {{"salt", (uint8_t)1}};
        break;
    case BBQuests::bitter_sandwich:
        //Watercress, Pepper: Item Drop Normal 1, Raid Power Flying 1, Exp Point Fighting 1
        fillings = {{"watercress", (uint8_t)1}};
        condiments = {{"pepper", (uint8_t)1}};
        break;
    case BBQuests::sandwich_three:
        //Fried Fillet, Noodles, Rice, Chili Sauce: Huge Water 1, Raid Flying 1, Catching Normal 1
        fillings = { {"fried-fillet", (uint8_t)1}, {"noodles", (uint8_t)1}, {"rice", (uint8_t)1} };
        condiments = {{"chili-sauce", (uint8_t)1}};
        break;
    default:
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Invalid sandwich selection.",
            stream
        );
        break;
    }

    make_sandwich_preset(env, stream, context, BBQ_OPTIONS.LANGUAGE, fillings, condiments);

    leave_picnic(env.program_info(), stream, context);

    return_to_plaza(env.program_info(), stream, context, BBQ_OPTIONS.LANGUAGE);
    context.wait_for_all_requests();

}

void quest_tera_raid(
    ProgramEnvironment& env,
    ConsoleHandle& console, ProControllerContext& context,
    BBQOption& BBQ_OPTIONS
){
    console.log("Quest: Tera Raid");

    bool started_tera_raid = false;
    while (!started_tera_raid){
        EncounterWatcher encounter_watcher(console, COLOR_RED);
        int ret = run_until<ProControllerContext>(
            console, context,
            [&](ProControllerContext& context){
                //Target is a tera raid crystal near the canyon plaza
                console.log("Navigating to tera crystal.");

                central_to_canyon_plaza(env.program_info(), console, context);

                pbf_move_left_joystick(context, {-1, 0}, 3000ms, 160ms);
                pbf_press_button(context, BUTTON_L, 80ms, 400ms);
                pbf_move_left_joystick(context, {-1, 0}, 720ms, 160ms);
                pbf_press_button(context, BUTTON_L, 80ms, 400ms);

                //Keep rolling until we get a raid
                uint64_t rerolls = 0;
                while (!open_raid(console, context) && rerolls < 150){
                    env.log("No Tera raid found.", COLOR_ORANGE);
                    day_skip_from_overworld(console, context);
                    pbf_wait(context, GameSettings::instance().RAID_SPAWN_DELAY0);
                    rerolls++;
                }
                started_tera_raid = true;
            },
            {
                static_cast<VisualInferenceCallback&>(encounter_watcher),
                static_cast<AudioInferenceCallback&>(encounter_watcher),
            }
        );
        encounter_watcher.throw_if_no_sound();

        if (ret >= 0){
            console.log("Battle menu detected.");

            bool is_shiny = (bool)encounter_watcher.shiny_screenshot();
            if (is_shiny){
                console.log("Shiny detected!");
                pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
                throw ProgramFinishedException();
            }else{
                console.log("Detected battle. Running from battle.");
                try{
                    NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                    battle_menu.move_to_slot(console, context, 3);
                    pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                    press_Bs_to_back_to_overworld(env.program_info(), console, context);
                    return_to_plaza(env.program_info(), console, context, BBQ_OPTIONS.LANGUAGE);
                }catch (...){
                    console.log("Unable to flee.");
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Unable to flee!",
                        console
                    );
                }
            }
        }
    }

    //Swap to the second pokemon in your party
    pbf_press_dpad(context, DPAD_DOWN, 80ms, 80ms);
    pbf_press_dpad(context, DPAD_DOWN, 80ms, 80ms);
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for(std::chrono::milliseconds(400));
    move_box_cursor(env.program_info(), console, context, BoxCursorLocation::PARTY, 1, 0);
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    pbf_press_dpad(context, DPAD_UP, 80ms, 80ms);
    pbf_mash_button(context, BUTTON_A, 2000ms);

    bool win = run_tera_battle(env, console, context, BBQ_OPTIONS.BATTLE_AI);
    if (win){
        env.log("Won tera raid.");
        if (!BBQ_OPTIONS.CATCH_ON_WIN.enabled()){
            exit_tera_win_without_catching(env.program_info(), console, context, 0);
        }else{
            if (BBQ_OPTIONS.CATCH_ON_WIN.FIX_TIME_ON_CATCH){
                go_home(console, context);
                home_to_date_time(console, context, false);
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
                resume_game_from_home(console, context);
            }
            exit_tera_win_by_catching(
                env, console, context,
                BBQ_OPTIONS.LANGUAGE,
                BBQ_OPTIONS.CATCH_ON_WIN.BALL_SELECT.slug(),
                0
            );
        }
    }else{
        env.log("Lost tera raid.");
        context.wait_for(std::chrono::seconds(3));
    }

    context.wait_for_all_requests();
    return_to_plaza(env.program_info(), console, context, BBQ_OPTIONS.LANGUAGE);
    day_skip_from_overworld(console, context);
}

BBQAutoBattleResult run_auto_battle(
    Logger& logger,
    BBQAutoBattleActions& actions,
    const BBQAutoBattlePolicy& policy
){
    const WallClock start = actions.now();
    uint64_t pass = 0;
    uint64_t empty_passes = 0;
    while (actions.kills() < policy.kill_target){
        pass++;
        const uint64_t kills_before = actions.kills();

        actions.fly_to_battle_area();
        const std::optional<double> hp = actions.clear(policy.kill_target);

        const uint64_t kills = actions.kills();
        empty_passes = kills == kills_before ? empty_passes + 1 : 0;
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(actions.now() - start);

        //Unknown HP heals, as every pass did before the threshold existed. The log rounds HP
        //down, so it shows below the threshold exactly when it is.
        const bool heal = !hp || *hp * 100 < policy.heal_below_hp_percent;
        const std::string hp_text = hp ? std::to_string((int)std::floor(*hp * 100)) + "%" : "?";
        const std::string threshold_text = std::to_string(policy.heal_below_hp_percent) + "%";

        std::optional<BBQAutoBattleResult> end;
        std::string next;
        if (!hp){
            next = "Below the target and HP unknown: return, heal and go again.";
        }else if (heal){
            next = "Below the target and HP below " + threshold_text + ": return, heal and go again.";
        }else{
            next = "Below the target and HP at or above " + threshold_text + ": return and go again without healing.";
        }
        if (kills >= policy.kill_target){
            end = BBQAutoBattleResult::TargetReached;
            next = "Target reached: return without healing.";
        }else if (empty_passes >= policy.max_empty_passes){
            end = BBQAutoBattleResult::NoKills;
            next = "No kill in " + std::to_string(empty_passes) + " passes in a row: return and give up.";
        }else if (elapsed >= policy.time_limit){
            end = BBQAutoBattleResult::TimeLimit;
            next = "Time limit of " + std::to_string(policy.time_limit.count()) + " min reached: return and give up.";
        }
        logger.log(
            "Auto Battle pass " + std::to_string(pass) + ": "
            + std::to_string(kills - kills_before) + " kills, "
            + std::to_string(kills) + "/" + std::to_string(policy.kill_target) + " in total, "
            + "HP " + hp_text + ", "
            + std::to_string(elapsed.count()) + " s elapsed. "
            + next
        );

        //The next quest starts from Central Plaza, so return even when done.
        actions.return_to_plaza();
        if (end){
            return *end;
        }
        //Skipping the heal also skips its re-fly: return_to_plaza() already lands on the Central
        //Plaza fly point, which is where the flight to Chargestone starts from.
        if (heal){
            actions.heal();
        }
    }
    return BBQAutoBattleResult::TargetReached;
}

namespace{

class ConsoleBBQAutoBattleActions : public BBQAutoBattleActions{
public:
    ConsoleBBQAutoBattleActions(
        ProgramEnvironment& env,
        VideoStream& stream, ProControllerContext& context,
        LetsGoEncounterBotStats& stats,
        LetsGoEncounterBotTracker& tracker,
        LetsGoHpWatcher& hp_watcher,
        Language language
    )
        : m_env(env), m_stream(stream), m_context(context)
        , m_stats(stats), m_tracker(tracker), m_hp_watcher(hp_watcher)
        , m_language(language)
    {}

    virtual uint64_t kills() const override{
        return m_stats.m_kills;
    }
    virtual WallClock now() const override{
        return current_time();
    }
    virtual void fly_to_battle_area() override{
        central_to_chargestone(m_env.program_info(), m_stream, m_context);

        //Wait for spawns
        pbf_wait(m_context, 3000ms);
        m_context.wait_for_all_requests();

        //Forward and right, stay in the battle court - safe zone
        pbf_press_button(m_context, BUTTON_L, 160ms, 400ms);
        pbf_move_left_joystick(m_context, {0, +1}, 2000ms, 400ms);
        pbf_move_left_joystick(m_context, {+1, 0}, 1440ms, 400ms);
        pbf_press_button(m_context, BUTTON_L, 160ms, 400ms);
    }
    virtual std::optional<double> clear(uint64_t stop_at_kills) override{
        //The watcher runs for the whole quest. Forget what it read before this clear, so a reading
        //from an earlier pass is never reported as this one's.
        m_hp_watcher.clear();

        use_lets_go_to_clear_in_front(m_stream, m_context, m_tracker, false, [&](ProControllerContext& context){
            pbf_move_left_joystick(context, {0, -1}, 1440ms, 400ms);
            pbf_wait(context, 12000ms);
            context.wait_for_all_requests();
            },
            [&]{ return m_stats.m_kills >= stop_at_kills; }
        );

        m_context.wait_for_all_requests();

        //Read before leaving: the HP bar is only on screen in the overworld with the lead out.
        const double hp = m_hp_watcher.last_known_value();
        if (hp <= 0){
            return std::nullopt;
        }
        return hp;
    }
    virtual void return_to_plaza() override{
        PokemonSV::return_to_plaza(m_env.program_info(), m_stream, m_context, m_language);
    }
    virtual void heal() override{
        OverworldWatcher done_healing(m_stream.logger(), COLOR_BLUE);
        pbf_move_left_joystick(m_context, {0, +1}, 800ms, 160ms);

        pbf_mash_button(m_context, BUTTON_A, 2400ms);
        m_context.wait_for_all_requests();

        int exit = run_until<ProControllerContext>(
            m_stream, m_context,
            [&](ProControllerContext& context){
                pbf_mash_button(context, BUTTON_B, 16000ms);
            },
            { { done_healing } }
            );
        if (exit == 0){
            m_stream.log("Overworld detected.");
        }
        open_map_from_overworld(m_env.program_info(), m_stream, m_context);
        pbf_press_button(m_context, BUTTON_ZL, 320ms, 800ms);
        fly_to_overworld_from_map(m_env.program_info(), m_stream, m_context);
    }

private:
    ProgramEnvironment& m_env;
    VideoStream& m_stream;
    ProControllerContext& m_context;
    LetsGoEncounterBotStats& m_stats;
    LetsGoEncounterBotTracker& m_tracker;
    LetsGoHpWatcher& m_hp_watcher;
    const Language m_language;
};

}

void quest_auto_battle(
    ProgramEnvironment& env,
    VideoStream& stream, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    stream.log("Quest: Auto Battle 10/30");


    OverworldSensors sensors(
        env.logger(), stream, context
    );

    LetsGoEncounterBotStats stats;
    LetsGoEncounterBotTracker tracker(
        env, stream,
        stats,
        sensors.lets_go_kill
    );

    //The time limits are about 3x the slowest attempts seen with a correct lead.
    BBQAutoBattlePolicy policy;
    policy.kill_target = 10;
    policy.time_limit = std::chrono::minutes(35);

    if (current_quest == BBQuests::auto_30){
        policy.kill_target = 30;
        policy.time_limit = std::chrono::minutes(60);
    }
    policy.heal_below_hp_percent = BBQ_OPTIONS.AUTO_BATTLE_HEAL_PERCENT;

    ConsoleBBQAutoBattleActions actions(env, stream, context, stats, tracker, sensors.lets_go_hp, BBQ_OPTIONS.LANGUAGE);
    run_auto_battle(stream.logger(), actions, policy);
}



}
}
}

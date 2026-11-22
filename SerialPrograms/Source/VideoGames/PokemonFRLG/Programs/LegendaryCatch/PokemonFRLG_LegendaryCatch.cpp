/*  Legendary Catch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <chrono>
#include <set>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "Pokemon/Inference/Pokemon_PokeballNameReader.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_BagDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_PartyMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_StartMenuNavigation.h"
#include "PokemonFRLG_LegendaryCatch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

std::string normalize_item_slug(std::string text){
    std::string slug;
    bool prev_dash = false;
    for (char ch : text){
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)){
            slug.push_back(static_cast<char>(std::tolower(uch)));
            prev_dash = false;
        }else if (!slug.empty() && !prev_dash){
            slug.push_back('-');
            prev_dash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-'){
        slug.pop_back();
    }
    return slug;
}

std::string join_errors(const std::vector<std::string>& errors){
    std::string out;
    for (size_t i = 0; i < errors.size(); i++){
        if (i > 0){
            out += "\n";
        }
        out += "- " + errors[i];
    }
    return out;
}

//  In FRLG's Bag menu, 6 item rows are visible on the right panel.
//  Each row has the item name on the left and "x<qty>" on the right.
constexpr int BAG_VISIBLE_ROWS = 6;
constexpr double BAG_ROW_X = 0.435;
constexpr double BAG_ROW_NAME_W = 0.360;
constexpr double BAG_ROW_QTY_X = 0.815;
constexpr double BAG_ROW_QTY_W = 0.130;
constexpr double BAG_ROW_Y0 = 0.082;
constexpr double BAG_ROW_DY = 0.098;
constexpr double BAG_ROW_H = 0.075;

std::string read_bag_row_name(
    Logger& logger,
    Language language,
    const ImageViewRGB32& game_screen,
    int row,
    BagPocket pocket
){
    ImageFloatBox name_box(BAG_ROW_X, BAG_ROW_Y0 + row * BAG_ROW_DY, BAG_ROW_NAME_W, BAG_ROW_H);
    ImageViewRGB32 cropped = extract_box_reference(game_screen, name_box);
    ImageRGB32 processed = preprocess_for_ocr(cropped);

    if (pocket == BagPocket::PokeBalls){
        OCR::StringMatchResult match = Pokemon::PokeballNameReader::instance().read_substring(
            logger, language, processed, DARK_TEXT_FILTERS()
        );
        if (!match.results.empty()){
            return match.results.begin()->second.token;
        }
    }

    std::string raw = OCR::ocr_read(language, processed);
    return normalize_item_slug(raw);
}

unsigned read_bag_row_qty(
    Logger& logger,
    Language language,
    const ImageViewRGB32& game_screen,
    int row
){
    ImageFloatBox qty_box(BAG_ROW_QTY_X, BAG_ROW_Y0 + row * BAG_ROW_DY, BAG_ROW_QTY_W, BAG_ROW_H);
    ImageViewRGB32 cropped = extract_box_reference(game_screen, qty_box);
    ImageRGB32 processed = preprocess_for_ocr(cropped);
    int val = OCR::read_number_waterfill(logger, processed, 0xff000000, 0xff808080);
    if (val > 0){
        return static_cast<unsigned>(val);
    }
    val = OCR::read_number(logger, processed, language);
    return val > 0 ? static_cast<unsigned>(val) : 0;
}

}  // namespace


LegendaryCatch_Descriptor::LegendaryCatch_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:LegendaryCatch",
        Pokemon::STRING_POKEMON + " FRLG",
        "Legendary Catch",
        "",
        "Select target legendaries, verify party members/stats and bag items, "
        "and catch them in Luxury Balls.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


LegendaryCatch::LegendaryCatch()
    : LANGUAGE(
        "<b>Game Language:</b>",
        Pokemon::PokemonNameReader::instance().languages(),
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , STATIC_HEADER("<b>Static Legendaries:</b>")
    , CATCH_ARTICUNO("<b>Articuno</b> (Seafoam Islands, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_ZAPDOS("<b>Zapdos</b> (Power Plant, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_MOLTRES("<b>Moltres</b> (Mt. Ember, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_MEWTWO("<b>Mewtwo</b> (Cerulean Cave, Lv. 70)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_DEOXYS("<b>Deoxys</b> (Birth Island, Lv. 30)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_LUGIA("<b>Lugia</b> (Navel Rock, Lv. 70)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_HO_OH("<b>Ho-oh</b> (Navel Rock, Lv. 70)", LockMode::LOCK_WHILE_RUNNING, false)
    , ROAMER_HEADER("<b>Roaming Legendaries:</b>")
    , CATCH_RAIKOU("<b>Raikou</b> (Kanto Roamer, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_ENTEI("<b>Entei</b> (Kanto Roamer, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , CATCH_SUICUNE("<b>Suicune</b> (Kanto Roamer, Lv. 50)", LockMode::LOCK_WHILE_RUNNING, false)
    , PRECONDITION_HEADER("<b>Precondition Settings:</b>")
    , LUXURY_BALLS_PER_LEGENDARY(
        "<b>Luxury Balls per Legendary:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        50, 1, 200
    )
    , REQUIRE_TYPE_WALLS(
        "<b>Require Type Walls:</b><br>Require Dark/Ground/Flash Fire support walls for Mewtwo/Zapdos/Moltres/Ho-oh.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , CHECK_BAG_ITEMS(
        "<b>Check Bag Items:</b><br>Verify Luxury Balls, Max Repels, Leftovers, and Smoke Ball before starting.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(LANGUAGE);

    PA_ADD_OPTION(STATIC_HEADER);
    PA_ADD_OPTION(CATCH_ARTICUNO);
    PA_ADD_OPTION(CATCH_ZAPDOS);
    PA_ADD_OPTION(CATCH_MOLTRES);
    PA_ADD_OPTION(CATCH_MEWTWO);
    PA_ADD_OPTION(CATCH_DEOXYS);
    PA_ADD_OPTION(CATCH_LUGIA);
    PA_ADD_OPTION(CATCH_HO_OH);

    PA_ADD_OPTION(ROAMER_HEADER);
    PA_ADD_OPTION(CATCH_RAIKOU);
    PA_ADD_OPTION(CATCH_ENTEI);
    PA_ADD_OPTION(CATCH_SUICUNE);

    PA_ADD_OPTION(PRECONDITION_HEADER);
    PA_ADD_OPTION(LUXURY_BALLS_PER_LEGENDARY);
    PA_ADD_OPTION(REQUIRE_TYPE_WALLS);
    PA_ADD_OPTION(CHECK_BAG_ITEMS);

    PA_ADD_OPTION(NOTIFICATIONS);
}


std::vector<LegendaryTarget> LegendaryCatch::selected_targets() const{
    std::vector<LegendaryTarget> targets;
    if (CATCH_ARTICUNO){
        targets.push_back(LegendaryTarget::Articuno);
    }
    if (CATCH_ZAPDOS){
        targets.push_back(LegendaryTarget::Zapdos);
    }
    if (CATCH_MOLTRES){
        targets.push_back(LegendaryTarget::Moltres);
    }
    if (CATCH_MEWTWO){
        targets.push_back(LegendaryTarget::Mewtwo);
    }
    if (CATCH_DEOXYS){
        targets.push_back(LegendaryTarget::Deoxys);
    }
    if (CATCH_LUGIA){
        targets.push_back(LegendaryTarget::Lugia);
    }
    if (CATCH_HO_OH){
        targets.push_back(LegendaryTarget::HoOh);
    }
    if (CATCH_RAIKOU){
        targets.push_back(LegendaryTarget::Raikou);
    }
    if (CATCH_ENTEI){
        targets.push_back(LegendaryTarget::Entei);
    }
    if (CATCH_SUICUNE){
        targets.push_back(LegendaryTarget::Suicune);
    }
    return targets;
}


std::vector<ObservedPartyMember> LegendaryCatch::read_party_members(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
){
    ConsoleHandle& console = env.console;
    Logger& logger = console.logger();

    open_party_menu_from_overworld(console, context);
    context.wait_for_all_requests();

    const size_t party_size = static_cast<size_t>(detect_last_occupied_party_slot(console)) + 1;
    logger.log("Detected " + std::to_string(party_size) + " party member(s). Reading summary stats...");

    //  Open Summary on slot 0 (A -> A on Summary).
    SummaryWatcher summary_page1(COLOR_RED);
    int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& ctx){
            pbf_press_button(ctx, BUTTON_A, 150ms, 500ms);
            pbf_press_button(ctx, BUTTON_A, 150ms, 1500ms);
        },
        {summary_page1}
    );
    if (ret != 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Failed to open Pokemon Summary from Party menu.",
            console
        );
    }

    StatsReader stats_reader(COLOR_RED);
    std::vector<ObservedPartyMember> party;
    party.reserve(party_size);

    for (size_t i = 0; i < party_size; i++){
        context.wait_for_all_requests();
        VideoSnapshot snap1 = console.video().snapshot();
        PokemonFRLG_Stats stats;
        stats_reader.read_page1(logger, LANGUAGE, snap1, stats);

        //  Switch to Page 2 for Speed.
        SummaryPage2Watcher summary_page2(COLOR_RED);
        ret = run_until<ProControllerContext>(
            console, context,
            [](ProControllerContext& ctx){
                pbf_press_dpad(ctx, DPAD_RIGHT, 150ms, 600ms);
            },
            {summary_page2}
        );
        if (ret != 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "Failed to switch to Summary Page 2.",
                console
            );
        }
        context.wait_for_all_requests();
        VideoSnapshot snap2 = console.video().snapshot();
        stats_reader.read_page2(logger, LANGUAGE, snap2, stats);

        ObservedPartyMember member;
        member.species = stats.name;
        member.level = stats.level.value_or(0);
        member.speed = stats.speed.value_or(0);
        logger.log(
            "Party slot " + std::to_string(i + 1) + ": " + member.species +
            " Lv." + std::to_string(member.level) +
            " Spe=" + std::to_string(member.speed)
        );
        party.push_back(member);

        if (i + 1 < party_size){
            //  Return to Page 1, then move down to the next party member.
            pbf_press_dpad(context, DPAD_LEFT, 150ms, 400ms);
            pbf_press_dpad(context, DPAD_DOWN, 150ms, 500ms);
            context.wait_for_all_requests();
        }
    }

    //  Back out to the overworld (Summary -> Party -> Start Menu -> Overworld).
    pbf_press_button(context, BUTTON_B, 150ms, 600ms);
    pbf_press_button(context, BUTTON_B, 150ms, 600ms);
    close_start_menu(console, context);
    return party;
}


std::map<std::string, unsigned> LegendaryCatch::read_bag_items(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const LegendaryCatchRequirements& requirements
){
    ConsoleHandle& console = env.console;
    Logger& logger = console.logger();

    std::set<BagPocket> needed_pockets;
    for (const BagItemRequirement& req : requirements.bag_items){
        needed_pockets.insert(req.pocket);
    }

    open_bag_from_overworld(console, context);
    context.wait_for_all_requests();

    std::map<std::string, unsigned> counts;
    //  Bag opens on Items (pocket 0). Pocket order: Items (0), Key Items (1), Poke Balls (2).
    int current_pocket = 0;

    auto scan_current_pocket = [&](BagPocket pocket){
        std::string last_top_name;
        for (int page = 0; page < 15; page++){
            context.wait_for_all_requests();
            VideoSnapshot snap = console.video().snapshot();
            ImageViewRGB32 game_screen = extract_box_reference(snap, GameSettings::instance().GAME_BOX);

            std::string top_name;
            bool saw_cancel = false;
            for (int row = 0; row < BAG_VISIBLE_ROWS; row++){
                std::string slug = read_bag_row_name(logger, LANGUAGE, game_screen, row, pocket);
                if (row == 0){
                    top_name = slug;
                }
                if (slug.empty() || slug == "cancel"){
                    saw_cancel = (slug == "cancel");
                    break;
                }
                unsigned qty = read_bag_row_qty(logger, LANGUAGE, game_screen, row);
                if (qty > 0){
                    counts[slug] = std::max(counts[slug], qty);
                }
            }
            if (saw_cancel || (!last_top_name.empty() && top_name == last_top_name)){
                break;
            }
            last_top_name = top_name;
            for (int s = 0; s < BAG_VISIBLE_ROWS; s++){
                pbf_press_dpad(context, DPAD_DOWN, 100ms, 150ms);
            }
        }
    };

    for (BagPocket pocket : needed_pockets){
        int target_idx = (pocket == BagPocket::Items) ? 0 : (pocket == BagPocket::KeyItems ? 1 : 2);
        while (current_pocket < target_idx){
            pbf_press_dpad(context, DPAD_RIGHT, 150ms, 500ms);
            current_pocket++;
        }
        while (current_pocket > target_idx){
            pbf_press_dpad(context, DPAD_LEFT, 150ms, 500ms);
            current_pocket--;
        }
        scan_current_pocket(pocket);
    }

    //  Close Bag -> Start Menu -> Overworld.
    pbf_press_button(context, BUTTON_B, 150ms, 600ms);
    close_start_menu(console, context);
    return counts;
}


void LegendaryCatch::run_preconditions(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    const LegendaryCatchRequirements& requirements
){
    Logger& logger = env.console.logger();

    home_black_border_check(env.console, context);

    logger.log("Checking party requirements (" + std::to_string(requirements.party_members.size()) + " role(s))...");
    std::vector<ObservedPartyMember> party = read_party_members(env, context);
    std::vector<std::string> party_errors = check_party_requirements(requirements, party);
    if (!party_errors.empty()){
        throw UserSetupError(logger, "Party precondition check failed:\n" + join_errors(party_errors));
    }
    logger.log("All party preconditions satisfied.");

    if (CHECK_BAG_ITEMS){
        logger.log("Checking bag item requirements (" + std::to_string(requirements.bag_items.size()) + " item(s))...");
        std::map<std::string, unsigned> bag_counts = read_bag_items(env, context, requirements);
        std::vector<std::string> bag_errors = check_bag_requirements(requirements, bag_counts);
        if (!bag_errors.empty()){
            throw UserSetupError(logger, "Bag precondition check failed:\n" + join_errors(bag_errors));
        }
        logger.log("All bag preconditions satisfied.");
    }
}


void LegendaryCatch::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    std::vector<LegendaryTarget> targets = selected_targets();
    if (targets.empty()){
        throw UserSetupError(env.console.logger(), "Select at least one legendary to catch.");
    }

    LegendaryCatchRequirements requirements = compute_legendary_catch_requirements(
        targets,
        LUXURY_BALLS_PER_LEGENDARY,
        REQUIRE_TYPE_WALLS
    );

    run_preconditions(env, context, requirements);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}


}
}
}

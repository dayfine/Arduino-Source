/*  PokemonLZA AutoStory - Command File Runner commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <ostream>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileParsing.h"
#include "NintendoSwitch/Programs/CommandFileRunner/NintendoSwitch_CommandFileRunner.h"
#include "VideoGames/PokemonLZA/Inference/Map/PokemonLZA_DirectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_SelectionArrowDetector.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"
#include "PokemonLZA_AutoStoryCommands.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  As long as the story executor gives a MASH step.
constexpr Milliseconds TALK_TIMEOUT = 60s;

//  run_story_move()'s tolerance when it faces a heading.
constexpr double FACE_TOLERANCE = 10.0;


//  Throws ParseException unless `words` has between `min` and `max` arguments.
void check_argument_count(const std::vector<std::string>& words, size_t min, size_t max){
    const size_t count = words.size() - 1;
    if (count < min || count > max){
        throw ParseException(std::format("{}: expected {} to {} arguments, got {}", words[0], min, max, count));
    }
}

//  Argument `index` as a heading in [0, 360).
double parse_heading(const std::vector<std::string>& words, size_t index){
    const double heading = optional_number(words, index, 0);
    if (heading < 0 || heading >= 360){
        throw ParseException(std::format("{}: heading {} is not in [0, 360)", words[0], words[index]));
    }
    return heading;
}

Language parse_language_code(const std::string& code){
    for (int c = (int)Language::English; c < (int)Language::EndOfList; c++){
        if (language_data((Language)c).code == code){
            return (Language)c;
        }
    }
    throw ParseException("lza_fly: unknown language code: " + code);
}


//  A dialogue box of any of the kinds talk_to_facing() waits for.
class AnyDialogDetector : public StaticScreenDetector{
public:
    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_white.make_overlays(items);
        m_blue.make_overlays(items);
        m_teal.make_overlays(items);
    }
    virtual bool detect(const ImageViewRGB32& screen) override{
        return m_white.detect(screen) || m_blue.detect(screen) || m_teal.detect(screen);
    }

private:
    FlatWhiteDialogDetector m_white;
    BlueDialogDetector m_blue;
    TealDialogDetector m_teal;
};

//  The minimap's heading, rounded to a degree; null without the minimap.
JsonValue read_facing(ConsoleHandle&, const ImageViewRGB32& screen){
    DirectionArrowDetector arrow;
    if (!arrow.detect(screen)){
        return JsonValue();
    }
    return (int64_t)std::lround(arrow.detected_angle_deg()) % 360;
}


void run_talk(CommandFileSession& session, const std::vector<std::string>& words){
    mash_to_overworld(session.console, session.context, parse_lza_talk(words), TALK_TIMEOUT);
}
void run_talk_to(CommandFileSession& session, const std::vector<std::string>& words){
    parse_no_arguments(words);
    talk_to_facing(session.console, session.context);
}
void run_pick(CommandFileSession& session, const std::vector<std::string>& words){
    choose_dialog_option(session.console, session.context, parse_lza_pick(words));
}
void run_move(CommandFileSession& session, const std::vector<std::string>& words){
    run_story_move(session.console, session.context, parse_lza_move(words));
}
void run_face(CommandFileSession& session, const std::vector<std::string>& words){
    const FaceArguments face = parse_lza_face(words);
    const double heading = face_heading(session.console, session.context, face.heading, face.tolerance);
    session.out << std::format("facing {:.0f}\n", heading);
}
void run_fly(CommandFileSession& session, const std::vector<std::string>& words){
    const FlyArguments fly = parse_lza_fly(words);
    fly_to(session.console, session.context, fly.language, fly.location);
}
void run_battle(CommandFileSession& session, const std::vector<std::string>& words){
    parse_no_arguments(words);
    BattleAIOption battle_ai;
    fight_battle(session.env, session.console, session.context, battle_ai);
}
void run_heal(CommandFileSession& session, const std::vector<std::string>& words){
    parse_no_arguments(words);
    heal_at_pokemon_center(session.console, session.context);
}
void run_shop(CommandFileSession& session, const std::vector<std::string>& words){
    buy_items(session.console, session.context, parse_lza_shop(words));
}
void run_save(CommandFileSession& session, const std::vector<std::string>& words){
    parse_no_arguments(words);
    save_story_game(session.console, session.context);
}

} // namespace



Button parse_lza_talk(const std::vector<std::string>& words){
    check_argument_count(words, 0, 1);
    if (words.size() == 1 || words[1] == "B"){
        return BUTTON_B;
    }
    if (words[1] == "A"){
        return BUTTON_A;
    }
    throw ParseException(words[0] + ": expected A or B, got " + words[1]);
}

size_t parse_lza_pick(const std::vector<std::string>& words){
    check_argument_count(words, 1, 1);
    const double index = optional_number(words, 1, 0);
    if (index < 0 || index != std::floor(index)){
        throw ParseException(words[0] + ": the choice must be a whole number from 0, got " + words[1]);
    }
    return (size_t)index;
}

StoryMove parse_lza_move(const std::vector<std::string>& words){
    check_argument_count(words, 2, 2);
    StoryMove move;
    if (words[1] != "ahead"){
        move.heading = parse_heading(words, 1);
    }
    move.duration = optional_duration(words, 2, 0ms);
    if (move.duration <= 0ms){
        throw ParseException(words[0] + ": the run time must be more than 0 ms, got " + words[2]);
    }
    return move;
}

FaceArguments parse_lza_face(const std::vector<std::string>& words){
    check_argument_count(words, 1, 2);
    FaceArguments face{.heading = parse_heading(words, 1), .tolerance = optional_number(words, 2, FACE_TOLERANCE)};
    if (face.tolerance <= 0 || face.tolerance >= 180){
        throw ParseException(words[0] + ": the tolerance must be in (0, 180), got " + words[2]);
    }
    return face;
}

FlyArguments parse_lza_fly(const std::vector<std::string>& words){
    check_argument_count(words, 1, 2);
    const std::vector<LocationItem>& places = LOCATION_ENUM_MAPPINGS();
    const auto place = std::ranges::find(places, words[1], &LocationItem::slug);
    if (place == places.end()){
        throw ParseException(words[0] + ": unknown place: " + words[1]);
    }
    return FlyArguments{
        .location = place->location,
        .language = words.size() > 2 ? parse_language_code(words[2]) : Language::English,
    };
}

std::vector<std::string> parse_lza_shop(const std::vector<std::string>& words){
    if (words.size() < 2){
        throw ParseException(words[0] + ": expected at least one item");
    }
    return std::vector<std::string>(words.begin() + 1, words.end());
}

void parse_no_arguments(const std::vector<std::string>& words){
    check_argument_count(words, 0, 0);
}


void register_lza_command_file_commands(){
    using enum CommandFileCommandKind;
    register_command_file_command("lza_talk", "lza_talk [A|B]   (mash until the overworld is back; B if none)", DRIVES_CONSOLE, run_talk);
    register_command_file_command("lza_talk_to", "lza_talk_to   (A at what the player faces, until a dialogue shows)", DRIVES_CONSOLE, run_talk_to);
    register_command_file_command("lza_pick", "lza_pick N   (choice N of a dialogue, 0 is the top)", DRIVES_CONSOLE, run_pick);
    register_command_file_command("lza_move", "lza_move ANGLE|ahead MS   (face ANGLE clockwise from north, then run MS)", DRIVES_CONSOLE, run_move);
    register_command_file_command("lza_face", "lza_face ANGLE [TOL]   (face ANGLE within TOL degrees, 10 if none)", DRIVES_CONSOLE, run_face);
    register_command_file_command("lza_fly", "lza_fly PLACE [LANG]   (fast travel to a location slug; LANG eng if none)", DRIVES_CONSOLE, run_fly);
    register_command_file_command("lza_battle", "lza_battle   (fight the battle that is starting)", DRIVES_CONSOLE, run_battle);
    register_command_file_command("lza_heal", "lza_heal   (not implemented yet)", DRIVES_CONSOLE, run_heal);
    register_command_file_command("lza_shop", "lza_shop ITEM...   (not implemented yet)", DRIVES_CONSOLE, run_shop);
    register_command_file_command("lza_save", "lza_save   (save the game, back to the overworld)", DRIVES_CONSOLE, run_save);

    register_command_file_detector("lza_dialog", [](ConsoleHandle&){
        return std::make_unique<AnyDialogDetector>();
    });
    register_command_file_detector("lza_overworld", [](ConsoleHandle&){
        return std::make_unique<OverworldPartySelectionDetector>();
    });
    register_command_file_detector("lza_arrow", [](ConsoleHandle&){
        return std::make_unique<SelectionArrowDetector>(COLOR_YELLOW, nullptr, SelectionArrowType::RIGHT, dialog_choice_box());
    });
    register_command_file_reading("lza_facing", read_facing);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation

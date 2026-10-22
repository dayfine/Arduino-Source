/*  Terarium Fly Points
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <stdexcept>
#include "PokemonSV_TerariumFlyPoints.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  The moves from Central Plaza are the ones central_to_*() has always used.
//
//  The English words assume the in-game names contain them. The Chinese
//  (Traditional) plaza names follow 中心廣場 and 草原廣場, both read off the
//  lab's map; the other Chinese names aren't known yet, so those labels
//  aren't checked.
const TerariumFlyPointInfo& terarium_fly_point_info(TerariumFlyPoint point){
    static const std::map<TerariumFlyPoint, TerariumFlyPointInfo> TABLE{
        {TerariumFlyPoint::CENTRAL_PLAZA, {
            .name = "Central Plaza",
            .from_central_plaza = {},
            .label_words = {
                {Language::English, {"central", "plaza"}},
                {Language::ChineseTraditional, {"中心廣場"}},
            },
        }},
        {TerariumFlyPoint::COASTAL_PLAZA, {
            .name = "Coastal Plaza",
            .from_central_plaza = {.x = +0.409, .y = +1, .hold = Milliseconds(1680)},
            .label_words = {
                {Language::English, {"coastal", "plaza"}},
                {Language::ChineseTraditional, {"海岸廣場"}},
            },
        }},
        {TerariumFlyPoint::CANYON_PLAZA, {
            .name = "Canyon Plaza",
            .from_central_plaza = {.x = -1, .y = -1, .hold = Milliseconds(1720)},
            .label_words = {
                {Language::English, {"canyon", "plaza"}},
                {Language::ChineseTraditional, {"峽谷廣場"}},
            },
        }},
        {TerariumFlyPoint::SAVANNA_PLAZA, {
            .name = "Savanna Plaza",
            .from_central_plaza = {.x = +0.291, .y = -1, .hold = Milliseconds(1440)},
            .label_words = {
                {Language::English, {"savanna", "plaza"}},
                {Language::ChineseTraditional, {"草原廣場"}},
            },
        }},
        {TerariumFlyPoint::POLAR_PLAZA, {
            .name = "Polar Plaza",
            .from_central_plaza = {.x = -0.844, .y = +0.805, .hold = Milliseconds(1960)},
            .label_words = {
                {Language::English, {"polar", "plaza"}},
                {Language::ChineseTraditional, {"極地廣場"}},
            },
        }},
        {TerariumFlyPoint::POLAR_REST_AREA, {
            .name = "Polar Rest Area",
            .from_central_plaza = {.x = -0.414, .y = +1, .hold = Milliseconds(1840)},
            .label_words = {
                {Language::English, {"polar", "rest"}},
            },
        }},
        {TerariumFlyPoint::POLAR_CLASSROOM_1, {
            .name = "Polar Classroom 1",
            .from_central_plaza = {.x = -1, .y = +0.844, .hold = Milliseconds(1200)},
            .label_words = {
                {Language::English, {"polar", "classroom"}},
            },
        }},
        {TerariumFlyPoint::CANYON_REST_AREA, {
            .name = "Canyon Rest Area",
            .from_central_plaza = {.x = -1, .y = -0.094, .hold = Milliseconds(1280)},
            .label_words = {
                {Language::English, {"canyon", "rest"}},
            },
        }},
        {TerariumFlyPoint::SAVANNA_CLASSROOM, {
            .name = "Savanna Classroom",
            .from_central_plaza = {.x = +1, .y = -0.724, .hold = Milliseconds(1120)},
            .label_words = {
                {Language::English, {"savanna", "classroom"}},
            },
        }},
        {TerariumFlyPoint::CHARGESTONE_CAVERN, {
            .name = "Chargestone Cavern",
            .from_central_plaza = {.x = -1, .y = -0.055, .hold = Milliseconds(1040)},
            .label_words = {
                {Language::English, {"chargestone"}},
            },
        }},
    };
    auto iter = TABLE.find(point);
    if (iter == TABLE.end()){
        throw std::logic_error("terarium_fly_point_info(): Unknown fly point.");
    }
    return iter->second;
}

const std::vector<TerariumFlyPoint>& all_terarium_fly_points(){
    static const std::vector<TerariumFlyPoint> POINTS{
        TerariumFlyPoint::CENTRAL_PLAZA,
        TerariumFlyPoint::COASTAL_PLAZA,
        TerariumFlyPoint::CANYON_PLAZA,
        TerariumFlyPoint::SAVANNA_PLAZA,
        TerariumFlyPoint::POLAR_PLAZA,
        TerariumFlyPoint::POLAR_REST_AREA,
        TerariumFlyPoint::POLAR_CLASSROOM_1,
        TerariumFlyPoint::CANYON_REST_AREA,
        TerariumFlyPoint::SAVANNA_CLASSROOM,
        TerariumFlyPoint::CHARGESTONE_CAVERN,
    };
    return POINTS;
}


std::string normalize_fly_point_label(const std::string& text){
    //  U+3000 IDEOGRAPHIC SPACE in UTF-8.
    const std::string IDEOGRAPHIC_SPACE = "\xE3\x80\x80";

    std::string ret;
    for (size_t c = 0; c < text.size(); c++){
        if (text.compare(c, IDEOGRAPHIC_SPACE.size(), IDEOGRAPHIC_SPACE) == 0){
            c += IDEOGRAPHIC_SPACE.size() - 1;
            continue;
        }
        const unsigned char ch = (unsigned char)text[c];
        if (ch >= 0x80){
            ret += (char)ch;
        }else if ('A' <= ch && ch <= 'Z'){
            ret += (char)(ch - 'A' + 'a');
        }else if (('a' <= ch && ch <= 'z') || ('0' <= ch && ch <= '9')){
            ret += (char)ch;
        }
    }
    return ret;
}

std::string fly_point_label_match_to_string(FlyPointLabelMatch match){
    switch (match){
    case FlyPointLabelMatch::MATCH:         return "match";
    case FlyPointLabelMatch::MISMATCH:      return "mismatch";
    case FlyPointLabelMatch::NO_LABEL:      return "no label";
    case FlyPointLabelMatch::NOT_CHECKED:   return "not checked";
    }
    return "?";
}

FlyPointLabelMatch match_fly_point_label(
    TerariumFlyPoint expected, Language language,
    const std::string& label_text
){
    const TerariumFlyPointInfo& info = terarium_fly_point_info(expected);
    auto iter = info.label_words.find(language);
    if (iter == info.label_words.end()){
        return FlyPointLabelMatch::NOT_CHECKED;
    }
    const std::string normalized = normalize_fly_point_label(label_text);
    if (normalized.empty()){
        return FlyPointLabelMatch::NO_LABEL;
    }
    for (const std::string& word : iter->second){
        if (normalized.find(word) == std::string::npos){
            return FlyPointLabelMatch::MISMATCH;
        }
    }
    return FlyPointLabelMatch::MATCH;
}


bool fly_to_terarium_point(
    Logger& logger, TerariumFlyActions& actions,
    Language language, TerariumFlyPoint destination,
    size_t max_attempts
){
    const TerariumFlyPointInfo& info = terarium_fly_point_info(destination);
    const bool check_label = info.label_words.contains(language);
    if (!check_label){
        logger.log("Fly to " + info.name + ": its name isn't known in this language. Flying without checking it.");
    }

    for (size_t attempt = 1; attempt <= max_attempts; attempt++){
        const std::string prefix = "Fly to " + info.name + ", attempt "
            + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ": ";

        if (!actions.open_map_at_central_plaza(attempt)){
            logger.log(prefix + "Central Plaza not found on the map. Closing the map.");
            actions.close_map();
            continue;
        }
        if (info.from_central_plaza.hold > Milliseconds(0)){
            actions.move_cursor(info.from_central_plaza);
        }

        if (check_label){
            const std::string label = actions.read_fly_point_label();
            const FlyPointLabelMatch match = match_fly_point_label(destination, language, label);
            logger.log(prefix + "map label \"" + label + "\": " + fly_point_label_match_to_string(match) + ".");
            if (match != FlyPointLabelMatch::MATCH){
                actions.close_map();
                continue;
            }
        }

        if (actions.fly_here()){
            logger.log(prefix + "flew.");
            return true;
        }
        logger.log(prefix + "no fly point under the cursor. Closing the map.");
        actions.close_map();
    }

    logger.log("Fly to " + info.name + ": gave up after " + std::to_string(max_attempts) + " attempts.");
    return false;
}



}
}
}

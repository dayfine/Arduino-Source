/*  Terarium Plaza Return
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Strings/Unicode.h"
#include "CommonTools/OCR/OCR_TextMatcher.h"
#include "PokemonSV_TerariumPlazaReturn.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  Both names were seen in the lab: 中心廣場 on the landing banner, and
//  "Central Plaza" is the English name of the same fly point. Other
//  languages aren't checked until their names are confirmed, since a wrong
//  name would fail every return.
std::string central_plaza_banner_to_check(Language language, bool ocr_available){
    if (!ocr_available){
        return "";
    }
    switch (language){
    case Language::English:
        return "Central Plaza";
    case Language::ChineseTraditional:
        return "中心廣場";
    default:
        return "";
    }
}


namespace{

//  Keep letters (lowercased), digits and CJK characters. Drop whitespace and
//  punctuation, including CJK and fullwidth punctuation.
std::u32string normalize_place_name(const std::string& text){
    std::u32string ret;
    for (char32_t ch : utf8_to_utf32(text)){
        if ('A' <= ch && ch <= 'Z'){
            ret += ch - 'A' + 'a';
        }else if (('a' <= ch && ch <= 'z') || ('0' <= ch && ch <= '9')){
            ret += ch;
        }else if (ch < 0x80){
            continue;
        }else if (0x3000 <= ch && ch <= 0x303f){
            continue;
        }else if (0xff00 <= ch && ch <= 0xff0f){
            continue;
        }else{
            ret += ch;
        }
    }
    return ret;
}

}

bool place_name_banner_matches(const std::string& expected, const std::string& ocr_text){
    const std::u32string target = normalize_place_name(expected);
    const std::u32string read = normalize_place_name(ocr_text);
    if (target.empty() || read.empty()){
        return false;
    }
    const size_t max_errors = std::max<size_t>(1, target.size() / 5);
    return OCR::levenshtein_distance_substring(target, read) <= max_errors;
}


bool place_name_banner_readable(const std::string& expected, const std::string& ocr_text){
    const std::u32string target = normalize_place_name(expected);
    const std::u32string read = normalize_place_name(ocr_text);
    //  For a name in another script (Chinese, Japanese, Korean), Latin letters and
    //  digits are noise: R18 read "cnlo" off the Central Plaza banner.
    const bool non_latin = std::any_of(target.begin(), target.end(), [](char32_t ch){ return ch >= 0x80; });
    const size_t count = non_latin
        ? (size_t)std::count_if(read.begin(), read.end(), [](char32_t ch){ return ch >= 0x80; })
        : read.size();
    return 2 * count >= std::max<size_t>(target.size(), 1);
}


bool landing_banner_shows(const std::string& expected, const std::vector<std::string>& reads){
    for (const std::string& text : reads){
        if (place_name_banner_readable(expected, text) && place_name_banner_matches(expected, text)){
            return true;
        }
    }
    return false;
}


std::string plaza_return_result_to_string(PlazaReturnResult result){
    switch (result){
    case PlazaReturnResult::ARRIVED:        return "arrived";
    case PlazaReturnResult::NOT_CHECKED:    return "not checked";
    case PlazaReturnResult::FAILED:         return "failed";
    case PlazaReturnResult::UNCONFIRMED:    return "unconfirmed";
    }
    return "?";
}

PlazaReturnResult return_to_plaza_checked(
    Logger& logger, PlazaReturnActions& actions,
    const std::string& expected_banner,
    size_t max_attempts
){
    bool last_landing_unread = false;
    for (size_t attempt = 1; attempt <= max_attempts; attempt++){
        const std::string prefix = "Return to Central Plaza, attempt "
            + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ": ";

        //  A wrong landing usually means the cursor stopped short of Central
        //  Plaza, so retries overshoot from the start.
        //  After a landing with no banner the player is likely on Central
        //  Plaza, whose icon then can't be found: one search tells (R17
        //  19:34, three searches took a minute).
        if (!actions.fly_to_plaza(attempt > 1, last_landing_unread ? 1 : 3)){
            //  T2 on the routine bench (2026-10-25): the return landed at
            //  Central Plaza with no banner, and every retry found no icon.
            //  The leave-area bench (19:21) read "a" off the same banner.
            if (last_landing_unread){
                logger.log(
                    prefix + "the fly didn't happen after a landing with no banner. "
                        "Central Plaza hides its own icon, so the player is there.",
                    COLOR_ORANGE
                );
                return PlazaReturnResult::UNCONFIRMED;
            }
            logger.log(prefix + "the fly didn't happen.", COLOR_RED);
            continue;
        }

        if (expected_banner.empty()){
            logger.log(prefix + "landed. Not checking where: no place name to check in this language.", COLOR_ORANGE);
            return PlazaReturnResult::NOT_CHECKED;
        }

        auto matches = [&](const std::string& text){
            return place_name_banner_matches(expected_banner, text);
        };
        const std::string banner = actions.read_landing_banner(matches);
        last_landing_unread = !place_name_banner_readable(expected_banner, banner);
        if (matches(banner)){
            logger.log(prefix + "landed at Central Plaza (banner \"" + banner + "\").");
            return PlazaReturnResult::ARRIVED;
        }
        logger.log(
            prefix + "landed, but the banner read \"" + banner + "\", not \"" + expected_banner + "\".",
            COLOR_RED
        );
    }

    logger.log(
        "Return to Central Plaza: no landing showed Central Plaza after "
            + std::to_string(max_attempts) + " attempts.",
        COLOR_RED
    );
    return PlazaReturnResult::FAILED;
}



}
}
}

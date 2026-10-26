/*  Terarium Plaza Return
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Return to Central Plaza and check, from the place name shown on landing,
 *  that the player really is there.
 *
 */

#ifndef PokemonAutomation_PokemonSV_TerariumPlazaReturn_H
#define PokemonAutomation_PokemonSV_TerariumPlazaReturn_H

#include <functional>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  The place name to look for on landing at Central Plaza, or "" when the
//  arrival can't be checked: the name isn't known in this language, or there
//  is no OCR data for it.
std::string central_plaza_banner_to_check(Language language, bool ocr_available);

//  Whether the OCR text of a place-name banner shows `expected`. Case,
//  spacing and punctuation don't count, other text around the name is
//  ignored, and a few OCR errors are allowed (one per 5 characters, at least
//  one), which still tells the plazas apart.
bool place_name_banner_matches(const std::string& expected, const std::string& ocr_text);

//  Whether an OCR read could be a place name at all: at least half as long as
//  `expected`, ignoring what place_name_banner_matches() ignores. A stray
//  letter read off the fading banner ("a") is not, nor Latin letters read for
//  a name in another script ("cnlo" for 中心廣場).
bool place_name_banner_readable(const std::string& expected, const std::string& ocr_text);

//  Whether the banner reads after a landing show `expected`: some read is
//  readable and matches it. No reads, or only noise, is not a match.
bool landing_banner_shows(const std::string& expected, const std::vector<std::string>& reads);


//  The game steps the return-and-check loop drives.
class PlazaReturnActions{
public:
    virtual ~PlazaReturnActions() = default;

    //  Fly toward Central Plaza the way return_to_plaza() always has: map
    //  cursor to the top-left corner, back toward the center, then the
    //  closest Pokecenter. "overshoot" moves the cursor back further, which
    //  helps when the game lags. Tries once: returns false if it didn't fly
    //  (no Pokecenter to fly to, or a battle interrupted it). "searches" caps
    //  the closest-Pokecenter search, ~13 s each.
    virtual bool fly_to_plaza(bool overshoot, size_t searches) = 0;

    //  Watch the place-name banner shown after landing. Returns the first
    //  text read that `matches` accepts, else the last non-empty text read
    //  ("" when nothing was read).
    virtual std::string read_landing_banner(const std::function<bool(const std::string& text)>& matches) = 0;
};

enum class PlazaReturnResult{
    ARRIVED,        //  The banner showed Central Plaza.
    NOT_CHECKED,    //  Landed, but the banner can't be checked in this language.
    FAILED,         //  Every attempt landed somewhere the banner didn't confirm.
    UNCONFIRMED,    //  A landing showed no banner, and the next fly found nothing to fly
                    //  to: the player stands on Central Plaza's icon, which hides it.
};
std::string plaza_return_result_to_string(PlazaReturnResult result);

//  Fly toward Central Plaza and check the landing banner, up to
//  `max_attempts` times. A fly that didn't happen uses up an attempt too, so
//  the return is bounded. A fly that didn't happen right after a landing with
//  no banner means the player is on Central Plaza: it hides its own icon. `expected_banner` comes from
//  central_plaza_banner_to_check(); when it is empty the first landing is
//  accepted unchecked.
PlazaReturnResult return_to_plaza_checked(
    Logger& logger, PlazaReturnActions& actions,
    const std::string& expected_banner,
    size_t max_attempts
);



}
}
}
#endif

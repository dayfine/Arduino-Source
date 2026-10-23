/*  Terarium Fly Points
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Flying to a Terarium fly point from anywhere in the Terarium: where each
 *  fly point is on the map, how to recognize its name label, and the retry
 *  loop. The game steps are injected through TerariumFlyActions, so the loop
 *  can run against a scripted fake in the unit tests.
 *
 */

#ifndef PokemonAutomation_PokemonSV_TerariumFlyPoints_H
#define PokemonAutomation_PokemonSV_TerariumFlyPoints_H

#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class TerariumFlyPoint{
    CENTRAL_PLAZA,
    COASTAL_PLAZA,
    CANYON_PLAZA,
    SAVANNA_PLAZA,
    POLAR_PLAZA,
    POLAR_REST_AREA,
    POLAR_CLASSROOM_1,
    CANYON_REST_AREA,
    SAVANNA_CLASSROOM,
    CHARGESTONE_CAVERN,
};

//  A left joystick push on the map, at the zoom level the map opens with.
struct MapCursorMove{
    double x = 0;
    double y = 0;
    Milliseconds hold = Milliseconds(0);
};

struct TerariumFlyPointInfo{
    //  The name used in logs.
    std::string name;

    //  Where the fly point is relative to the Central Plaza fly point.
    //  A hold of 0 means no move.
    MapCursorMove from_central_plaza;

    //  Words the map's name label must contain, after
    //  normalize_fly_point_label(), for each language. A language that isn't
    //  listed has no known name, and the label isn't checked.
    std::map<Language, std::vector<std::string>> label_words;
};

const TerariumFlyPointInfo& terarium_fly_point_info(TerariumFlyPoint point);
const std::vector<TerariumFlyPoint>& all_terarium_fly_points();


//  Lowercase ASCII letters and drop ASCII spaces and punctuation and
//  ideographic spaces, so OCR spacing doesn't matter. Other characters are
//  kept as they are.
std::string normalize_fly_point_label(const std::string& text);

enum class FlyPointLabelMatch{
    MATCH,
    MISMATCH,
    NO_LABEL,       //  Nothing was read: the cursor isn't on a fly point.
    NOT_CHECKED,    //  The fly point's name isn't known in this language.
};
std::string fly_point_label_match_to_string(FlyPointLabelMatch match);

FlyPointLabelMatch match_fly_point_label(
    TerariumFlyPoint expected, Language language,
    const std::string& label_text
);


//  The game steps fly_to_terarium_point() is made of.
class TerariumFlyActions{
public:
    virtual ~TerariumFlyActions() = default;

    //  Open the map and put the cursor on the Central Plaza fly point,
    //  wherever the player is. "attempt" starts at 1. Return false if the
    //  Central Plaza fly point wasn't found. The map stays open either way.
    virtual bool open_map_at_central_plaza(size_t attempt) = 0;

    //  Move the map cursor, then zoom out, as map_move_cursor_fly() does.
    virtual void move_cursor(const MapCursorMove& move) = 0;

    //  OCR the name label shown next to the cursor. Empty if there is none.
    virtual std::string read_fly_point_label() = 0;

    //  Fly to the fly point under the cursor. Return false if there was no
    //  fly point to fly to.
    virtual bool fly_here() = 0;

    //  Close the map and return to the overworld.
    virtual void close_map() = 0;
};

//  Fly to "destination" from anywhere in the Terarium: open the map, put the
//  cursor on Central Plaza, move it to the destination, check the fly point's
//  name, then fly. A wrong or missing name closes the map and tries again.
//  Return true once flown. Return false, with the map closed, after
//  "max_attempts" failures.
bool fly_to_terarium_point(
    Logger& logger, TerariumFlyActions& actions,
    Language language, TerariumFlyPoint destination,
    size_t max_attempts = 3
);



}
}
}
#endif

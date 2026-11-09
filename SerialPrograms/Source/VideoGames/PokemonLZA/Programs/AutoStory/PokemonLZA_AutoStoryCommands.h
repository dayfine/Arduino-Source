/*  PokemonLZA AutoStory - Command File Runner commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions (PokemonLZA_AutoStoryActions.h) as Command File Runner
 *  commands (NintendoSwitch_CommandFileRunner.h), one action per line, plus
 *  the detectors those actions wait on. Someone playing the story by hand
 *  through the lab channel then records action-level steps rather than
 *  presses, and devtools/lab/cfr_steps_to_story.py turns the recording into
 *  rows of the story table (PokemonLZA_AutoStoryTable.h).
 *
 *      lza_talk [A|B]          mash A or B (default) until the overworld is back
 *      lza_talk_to             A at what the player faces, until a dialogue shows
 *      lza_pick N              choice N of a dialogue (0 is the top)
 *      lza_move ANGLE|ahead MS face ANGLE (clockwise from north), then run MS
 *      lza_face ANGLE [TOL]    face ANGLE within TOL degrees (default 10)
 *      lza_fly PLACE [LANG]    fast travel to a location slug; LANG an OCR code (default eng)
 *      lza_battle              fight the battle that is starting
 *      lza_heal, lza_shop ITEM...   not implemented yet: they fail saying so
 *      lza_save                save the game
 *
 *  Detectors: lza_dialog, lza_overworld, lza_arrow (a dialogue's choice
 *  arrow). Reading: lza_facing, the minimap's heading.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCommands_H
#define PokemonAutomation_PokemonLZA_AutoStoryCommands_H

#include <cstddef>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "PokemonLZA_AutoStoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The arguments of "lza_face ANGLE [TOL]".
struct FaceArguments{
    double heading = 0;     //  Clockwise from north, in [0, 360).
    double tolerance = 0;   //  Degrees either side.
};

//  The arguments of "lza_fly PLACE [LANG]".
struct FlyArguments{
    Location location = Location::CENTRICO_PLAZA;
    Language language = Language::English;
};

//  The parsers of the commands' words (words[0] is the command). Each
//  throws ParseException for a missing or malformed argument.

//  "lza_talk [A|B]": the button to mash, B if none.
Button parse_lza_talk(const std::vector<std::string>& words);

//  "lza_pick N": the choice, 0 for the top one.
size_t parse_lza_pick(const std::vector<std::string>& words);

//  "lza_move ANGLE|ahead MS": the heading ("ahead": none) and run time.
StoryMove parse_lza_move(const std::vector<std::string>& words);

//  "lza_face ANGLE [TOL]".
FaceArguments parse_lza_face(const std::vector<std::string>& words);

//  "lza_fly PLACE [LANG]": PLACE a location slug (e.g. "centrico-plaza"),
//  LANG a language code (e.g. "eng").
FlyArguments parse_lza_fly(const std::vector<std::string>& words);

//  "lza_shop ITEM...": at least one item.
std::vector<std::string> parse_lza_shop(const std::vector<std::string>& words);

//  "lza_talk_to", "lza_battle", "lza_heal", "lza_save": no arguments.
void parse_no_arguments(const std::vector<std::string>& words);


//  Adds the commands, detectors and reading above to the Command File
//  Runner. Called once, from the game's panel list.
void register_lza_command_file_commands();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

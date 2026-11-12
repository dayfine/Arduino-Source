/*  PokemonLZA AutoStory - Command File Runner commands
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions (Actions/) as Command File Runner commands
 *  (NintendoSwitch_CommandFileRunner.h), one action per line, plus the
 *  detectors those actions wait on. Someone playing the story by hand
 *  through the lab channel then records action-level steps rather than
 *  presses, and devtools/lab/cfr_steps_to_story.py turns the recording into
 *  a draft of story steps. The commands are listed at
 *  register_lza_command_file_commands().
 *
 */

// code-health: allow header_symbols -- one parser per command and its arguments, public for the parse tests
#ifndef PokemonAutomation_PokemonLZA_AutoStoryCommands_H
#define PokemonAutomation_PokemonLZA_AutoStoryCommands_H

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryMoves.h"

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

//  The arguments of "lza_battle [now [SECONDS]]".
struct BattleArguments{
    //  True: the battle is already on screen (fight_battle_in_progress()).
    //  False: it is starting (fight_battle()).
    bool in_progress = false;
    std::chrono::seconds timeout = std::chrono::seconds(180);
};

//  The parsers of the commands' words (words[0] is the command). Each
//  throws ParseException for a missing or malformed argument.

//  "lza_talk [A|B]": the button to mash, B if none.
Button parse_lza_talk(const std::vector<std::string>& words);

//  "lza_pick N": the choice, 0 for the top one.
size_t parse_lza_pick(const std::vector<std::string>& words);

//  "lza_move ANGLE|ahead MS": the heading ("ahead": none) and one leg that
//  walks straight on for MS.
StoryMove parse_lza_move(const std::vector<std::string>& words);

//  "lza_face ANGLE [TOL]".
FaceArguments parse_lza_face(const std::vector<std::string>& words);

//  "lza_fly PLACE [LANG]": PLACE a location slug (e.g. "centrico-plaza"),
//  LANG a language code (e.g. "eng").
FlyArguments parse_lza_fly(const std::vector<std::string>& words);

//  "lza_battle [now [SECONDS]]".
BattleArguments parse_lza_battle(const std::vector<std::string>& words);

//  "lza_talk_to", "lza_save": no arguments.
void parse_no_arguments(const std::vector<std::string>& words);


//  Adds these commands, detectors and reading to the Command File Runner.
//  Called once, from the game's panel list.
//
//      lza_talk [A|B]          mash A or B (default) until the overworld is back
//      lza_talk_to             A at what the player faces, until a dialogue shows
//      lza_pick N              choice N of a dialogue (0 is the top)
//      lza_move ANGLE|ahead MS face ANGLE (clockwise from north), then walk MS
//      lza_face ANGLE [TOL]    face ANGLE within TOL degrees (default 10)
//      lza_fly PLACE [LANG]    fast travel to a location slug; LANG an OCR code (default eng)
//      lza_battle [now [S]]    fight the battle that is starting; "now": one
//                              already on screen, for up to S seconds (default 180)
//      lza_save                save the game
//
//  Detectors: lza_dialog, lza_overworld, lza_arrow (a dialogue's choice
//  arrow). Reading: lza_facing, the minimap's heading.
void register_lza_command_file_commands();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  Teach Text Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the messages of the party menu and the TM's scene while a TM is
 *  taught. Implemented in PokemonFRLG_TeachScreenReader.cpp, whose tests
 *  cover it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_TeachTextReader_H
#define PokemonAutomation_PokemonFRLG_TeachTextReader_H

#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The messages of the party menu while a TM is taught.
enum class TeachText{
    UNKNOWN,
    TEACH_WHICH,        //  "Teach which POKEMON?"
    WANTS_TO_LEARN,     //  "<MON> wants to learn the move <MOVE>."
    KNOWS_FOUR,         //  "However, <MON> already knows four moves."
    DELETE_QUESTION,    //  "Should a move be deleted and replaced with <MOVE>?"
    WHICH_MOVE,         //  "Which move should be forgotten?"
    STOP_QUESTION,      //  "Stop trying to teach <MOVE>?"
    DID_NOT_LEARN,      //  "<MON> did not learn the move <MOVE>."
    NOT_COMPATIBLE,     //  "<MON> and <MOVE> are not compatible."
    MACHINE_SET,        //  "Machine set!", in the TM's scene
    LEARNED,            //  "<MON> learned <MOVE>!"
};
//  The human-readable name of a teach dialog message.
std::string_view teach_text_name(TeachText text);

//  The message over the party menu or in the TM's scene, from the prompt's
//  or the message box's first two lines.
TeachText read_teach_text(Logger& logger, Language language, const ImageViewRGB32& screen);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

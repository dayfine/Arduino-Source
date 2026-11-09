/*  BDSP AutoStory Resources
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <iterator>
#include "PokemonBDSP_AutoStory_Resources.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

const std::string_view PATHS[] = {
    //  Actions/PokemonBDSP_AutoStory_Settings.cpp: each row's label and
    //  target value, as SETTINGS there names them.
    "PokemonBDSP/Autostory/Options/textspeed_label.png",
    "PokemonBDSP/Autostory/Options/textspeed_value.png",
    "PokemonBDSP/Autostory/Options/changewindow_label.png",
    "PokemonBDSP/Autostory/Options/changewindow_value.png",
    "PokemonBDSP/Autostory/Options/battleeffects_label.png",
    "PokemonBDSP/Autostory/Options/battleeffects_value.png",
    "PokemonBDSP/Autostory/Options/battlestyle_label.png",
    "PokemonBDSP/Autostory/Options/battlestyle_value.png",
    "PokemonBDSP/Autostory/Options/sendtoboxes_label.png",
    "PokemonBDSP/Autostory/Options/sendtoboxes_value.png",
    "PokemonBDSP/Autostory/Options/givenicknames_label.png",
    "PokemonBDSP/Autostory/Options/givenicknames_value.png",
    "PokemonBDSP/Autostory/Options/gyroscope_label.png",
    "PokemonBDSP/Autostory/Options/gyroscope_value.png",
    "PokemonBDSP/Autostory/Options/autosave_label.png",
    "PokemonBDSP/Autostory/Options/autosave_value.png",
    "PokemonBDSP/Autostory/Options/shownicknames_label.png",
    "PokemonBDSP/Autostory/Options/shownicknames_value.png",
    "PokemonBDSP/Autostory/Options/backgroundmusic_label.png",
    "PokemonBDSP/Autostory/Options/backgroundmusic_value.png",
    "PokemonBDSP/Autostory/Options/soundeffects_label.png",
    "PokemonBDSP/Autostory/Options/soundeffects_value.png",

    //  Actions/PokemonBDSP_AutoStory_Events_Lake.cpp
    "PokemonBDSP/StarterBriefcase.png",
    //  Inference/PokemonBDSP_SelectionArrow.cpp and
    //  PokemonBDSP_NameListDetector.cpp
    "PokemonBDSP/SelectionArrow.png",
};

} // namespace


const std::vector<std::string_view>& autostory_resource_paths(){
    static const std::vector<std::string_view> LIST(std::begin(PATHS), std::end(PATHS));
    return LIST;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

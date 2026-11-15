/*  Lab Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The ids of lab requests: the lab server names each request with a UUID,
 *  which "Lab/state.json" repeats on the start or stop the request made
 *  (ProgramTransition.h). Nothing here touches the app, so all of it is
 *  unit tested.
 *
 */

#ifndef PokemonAutomation_LabTransition_H
#define PokemonAutomation_LabTransition_H

#include <string>
#include <string_view>

namespace PokemonAutomation{


//  A new random (version 4) UUID, lower case: "d8ea6a9d-1eab-4a40-b355-d88072b289a1".
std::string new_lab_uuid();

//  Whether `text` is a lower-case UUID, as the lab server writes them.
bool is_lab_uuid(std::string_view text);



}  // namespace PokemonAutomation
#endif

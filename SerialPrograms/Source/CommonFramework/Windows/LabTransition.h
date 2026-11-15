/*  Lab Transition
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The start or stop of the selected program that "Lab/state.json" names
 *  (ProgramTransition.h), and the ids of lab requests: the lab server names
 *  each request with a UUID, which state.json repeats on the start or stop
 *  the request made. Nothing here touches the app, so all of it is unit
 *  tested.
 *
 */

#ifndef PokemonAutomation_LabTransition_H
#define PokemonAutomation_LabTransition_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ProgramTransition.h"

namespace PokemonAutomation{


//  A new random (version 4) UUID, lower case: "d8ea6a9d-1eab-4a40-b355-d88072b289a1".
std::string new_lab_uuid();

//  Whether `text` is a lower-case UUID, as the lab server writes them.
bool is_lab_uuid(std::string_view text);


//  The selected program's last transition (ProgramSession::last_transition()),
//  unless it is older than the selection at `selected_at`: a start or stop
//  from before the panel was selected again is not news to the lab server.
std::optional<ProgramTransition> lab_transition_since(
    std::optional<ProgramTransition> last, WallClock selected_at
);



}  // namespace PokemonAutomation
#endif

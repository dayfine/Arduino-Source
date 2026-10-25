/*  Terarium
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_Terarium_H
#define PokemonAutomation_PokemonSV_Terarium_H

#include "CommonFramework/Language.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
    struct ProgramInfo;
namespace NintendoSwitch{
namespace PokemonSV{

// Return to Central Plaza from anywhere in the terarium. When the language
// has OCR data and a known Central Plaza name, check the place name shown on
// landing and return again if it is wrong. Throws if no return lands there.
void return_to_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, Language language);

// Leave the player where they are instead of returning to Central Plaza. The next
// central_to_*() flight then goes straight to its destination from here, and
// ensure_at_plaza() makes the return for steps that start at Central Plaza.
void stay_away_from_plaza(Language language);
void ensure_at_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);
void reset_plaza_position();
//  After a flight onto the player's own fly spot: read the place banner and set
//  whether the player is at Central Plaza. Without a readable banner, assume it is.
void note_start_position(VideoStream& stream, ProControllerContext& context, Language language);

// Thrown by map_move_cursor_fly() when it can't fly to its destination.
// BBQ quests catch it to fail the current attempt instead of the program.
class TerariumFlyFailedException : public OperationFailedExceptionWithScreenshot{
public:
    using OperationFailedExceptionWithScreenshot::OperationFailedExceptionWithScreenshot;
    virtual const char* name() const override{ return "TerariumFlyFailedException"; }
};

// Same as return_to_plaza(), but stops at the first wild battle instead of fleeing it.
// Returns true with the battle menu open, or false once at Central Plaza.
bool return_to_plaza_unless_battle(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context, Language language);

// Open map from overworld, move cursor to fly location, and fly to location
void map_move_cursor_fly(
    const ProgramInfo& info,
    VideoStream& stream, ProControllerContext& context,
    double x, double y,
    Milliseconds hold, Milliseconds release,
    std::string location
);

// From central plaza, fly to the polar rest area
void central_to_polar_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to polar outdoor classroom 1
void central_to_polar_class1(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to coastal plaza
void central_to_polar_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to coastal plaza
void central_to_coastal_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to canyon plaza
void central_to_canyon_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to savanna plaza
void central_to_savanna_plaza(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to canyon rest area
void central_to_canyon_rest(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to savanna classroom. Warning: extremely laggy area.
void central_to_savanna_class(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// From central plaza, fly to chargestone cavern
void central_to_chargestone(const ProgramInfo& info, VideoStream& stream, ProControllerContext& context);

// Jump straight up into the air and fly. Fly up for hold_up ticks, fly straight for flight_wait ticks, and wait drop_time ticks to fall to the ground.
void jump_glide_fly(
    VideoStream& stream, ProControllerContext& context,
    bool inverted_flight,
    Milliseconds hold_up,
    Milliseconds flight_wait,
    Milliseconds drop_time
);


}
}
}
#endif

/*  Screen Observation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one look at the game showed, as facts rather than pixels: the
 *  events on screen (a fade, a text box, a battle) and named readings. A
 *  console fills it from a frame with the game's detectors; a simulation
 *  fills it from its model and has no frame. A ScreenState that matches on
 *  an observation (ScreenState::matches_observation) therefore runs in
 *  both.
 *
 */

#ifndef PokemonAutomation_CommonTools_ScreenObservation_H
#define PokemonAutomation_CommonTools_ScreenObservation_H

#include <stdint.h>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"

namespace PokemonAutomation{


//  Something a look at the screen can show. Several can hold at once.
enum class ScreenEvent : uint8_t{
    FADE_BLACK,     //  the screen is (fading to) black: a warp, a battle start
    FADE_WHITE,     //  the screen is (fading to) white
    TEXT_BOX,       //  a dialogue or message box is open
    BATTLE,         //  a battle is showing
    CAN_MOVE,       //  the player can move: no box, menu or scene holds them
    STILL,          //  nothing moved since the previous look
    CHOICE_BOX,     //  a list of answers to pick from is open
    INTERACT_PROMPT,    //  a prompt to interact with what the player faces ("A Talk")
};

//  The set of events one look showed.
class ScreenEvents{
public:
    ScreenEvents() = default;
    explicit ScreenEvents(std::initializer_list<ScreenEvent> events){
        for (ScreenEvent event : events){
            add(event);
        }
    }

    bool has(ScreenEvent event) const{
        return (m_bits & bit(event)) != 0;
    }
    void add(ScreenEvent event){
        m_bits |= bit(event);
    }

    bool operator==(const ScreenEvents& other) const = default;

private:
    static uint32_t bit(ScreenEvent event){
        return (uint32_t)1 << (uint32_t)event;
    }

    uint32_t m_bits = 0;
};

//  One look at the game.
struct ScreenObservation{
    WallClock when = WallClock::min();
    ScreenEvents events = {};
    //  Named numbers a reader found: "bag.RARE_CANDY", "facing",
    //  "heading" (degrees clockwise from north, [0, 360)).
    std::map<std::string, int, std::less<>> readings = {};
    //  The frame looked at; none in a simulation. A state that needs it
    //  can only run on a console.
    std::optional<VideoSnapshot> frame = std::nullopt;
};

//  "fade_black", "fade_white", "text_box", "battle", "can_move", "still",
//  "choice_box", "interact_prompt".
std::string_view screen_event_name(ScreenEvent event);

//  The events in `events` joined by `separator`, or "nothing" when empty.
std::string describe(const ScreenEvents& events, std::string_view separator = ", ");

//  Whether `a` and `b` both have a frame of the same size within the
//  still-screen RMSD threshold (2.0).
bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b);



} // namespace PokemonAutomation
#endif

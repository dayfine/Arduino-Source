/*  BDSP AutoStory - Talks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Talking to people and fighting the trainers among them, and the looks
 *  at the screen the story's events decide by. Measured on the console
 *  (segment 01, 2026-10-29 to 10-31; the Jubilife talks, 2026-10-30).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Talks_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Talks_H

#include <cstdint>
#include <string>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Fails the step with a screenshot of the screen now.
[[noreturn]] void fail_step(VideoStream& stream, const std::string& message);

//  True if a dialogue opens within `timeout`: someone who stopped us as the
//  walk ended reads as BLOCKED, and their box comes up a moment later.
bool dialogue_opens(VideoStream& stream, ProControllerContext& context, Milliseconds timeout);

//  For a dialogue that has opened: B through it. A trainer's ends in a
//  battle, which is fought with the best move (Potions when HP runs low),
//  followed by a Potion if HP is under half. Anyone else's ends back in the
//  overworld. Returns whether there was a battle. Between a trainer's last
//  line and the battle the overworld shows with no dialogue for a moment,
//  so "only talked" needs 4 s of it, and never comes when `battle_expected`.
//  `doubles` for tag and twin battles. A lost battle ends in the last
//  Pokemon Center; see in_pokemon_center().
bool talk_or_fight(
    VideoStream& stream, ProControllerContext& context, const std::string& label,
    bool battle_expected = false, bool doubles = false
);

//  Fights a trainer whose dialogue has opened, with whatever starter we
//  have, then a Potion if HP is under half. Someone who only talks leaves
//  no battle to fight. How the story's planned walks fight (RouteTrainerFight).
void fight_route_trainer(VideoStream& stream, ProControllerContext& context, const std::string& name);

//  A rough check that we stand in a Pokemon Center: its floor fills the lower
//  half of the screen with warm yellow, where the gym is grey and routes are
//  green. After a battle it tells a loss (a whiteout ends at the counter)
//  from a win. `percent`, if given, gets the warm share of the lower half.
bool center_floor_on_screen(const ImageViewRGB32& screen, size_t* percent = nullptr);
bool in_pokemon_center(VideoStream& stream);
//  The Oreburgh Mine's hall and corridors are dim: mean brightness 45 to 64
//  on the console, against 99 to 116 for the city and its yard (2026-10-31).
bool in_dim_cave(VideoStream& stream, uint64_t below = 85);
//  Mean of the three channels over a small copy of the frame, 0 to 255.
uint64_t mean_brightness(const ImageViewRGB32& screen);

//  What a talk showed: whether a dialogue opened, whether it had closed
//  after, and what OCR read in its first box.
struct DialogueRead{
    bool opened = false;
    bool closed = false;
    std::string text;
};
//  Whether a sign's OCR text has `key` (lower case), ignoring case.
bool sign_text_matches(const std::string& text, const std::string& key);
//  Faces `direction` and presses A. If a dialogue opens, reads its first
//  box, then presses `next` once per line, 1.5 s apart, while it shows.
DialogueRead talk_facing(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, Button next, const std::string& label);
//  Presses A the way we face, without the turn talk_facing() taps first: a
//  turn tap the way we already face is a step.
DialogueRead talk_ahead(VideoStream& stream, ProControllerContext& context, Button next, const std::string& label);
//  Walks `direction`, pressing A after each short stretch, for at most
//  `tiles` tiles' time until someone answers; then one A per line, 1.5 s
//  apart, while the box shows.
DialogueRead talk_walking(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction, size_t tiles, const std::string& label);
//  Faces `facing` and presses A: whether a box opened at the bottom of
//  the screen (a sign's may be green, not a dialogue's), what OCR reads
//  in it, and whether B closed it.
DialogueRead read_sign(VideoStream& stream, ProControllerContext& context, Pokemon::Direction facing, const std::string& label);



}
}
}
#endif

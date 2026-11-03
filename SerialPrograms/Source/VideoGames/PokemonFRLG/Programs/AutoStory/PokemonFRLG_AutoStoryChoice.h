/*  AutoStory Choice
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  CHOICE steps: a story pick between two things the player takes one of.
 *  The first is Mt. Moon's fossil (pokefirered data/maps/MtMoon_B2F): after
 *  Super Nerd Miguel is beaten (the battle step's job), the DOME FOSSIL
 *  lies at (13, 7) and the HELIX FOSSIL at (14, 7). A on one: "You want
 *  the DOME/HELIX FOSSIL?" YES/NO. YES: "<player> obtained the ... FOSSIL!",
 *  then Miguel walks to the other and takes it; NO: nothing happens and
 *  both stay.
 *
 *  The table walks the player to face the chosen fossil; the step reads
 *  which fossil the prompt offers before it answers YES, and answers NO
 *  (B) and stops if it offers the other. A prompt it can't read is taken
 *  to offer the fossil the player faces, and answered YES. After the
 *  scene the bag is read for the fossil, where a reader can.
 *
 *  The decisions take their game steps from a ChoiceActions, so they run
 *  in tests against a fake Mt. Moon. On the console the prompt's text has
 *  no reader yet: the step waits for the YES/NO box, answers YES by the
 *  player's place, and clears the scene to the overworld.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryChoice_H
#define PokemonAutomation_PokemonFRLG_AutoStoryChoice_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The `fossil` option: HELIX by default (the world record's).
enum class Fossil{
    DOME,
    HELIX,
};
const char* fossil_name(Fossil fossil);
std::optional<Fossil> fossil_from_name(std::string_view name);

//  Where `fossil` lies on MtMoon_B2F (map.json object_events).
Pokemon::TilePos fossil_tile(Fossil fossil);

//  The prefix of a GAME step's variant that names the fossil CHOICE.
inline constexpr std::string_view FOSSIL_VARIANT_PREFIX = "fossil ";

//  "fossil helix" or "fossil dome".
std::string fossil_variant(Fossil fossil);
std::optional<Fossil> parse_fossil_variant(std::string_view variant);

//  The table's CHOICE(fossil) step, the player facing the fossil.
Pokemon::StoryStep fossil_step(std::string name, std::string why, Fossil fossil);


//  The game steps the fossil CHOICE takes, injected so its decisions run
//  in tests.
class ChoiceActions{
public:
    virtual ~ChoiceActions() = default;

    //  Facing the fossil: A, until its YES/NO shows. False if none did.
    virtual bool talk() = 0;
    //  On the YES/NO: the fossil the prompt offers; nothing if unread.
    virtual std::optional<Fossil> read_offer() = 0;
    //  YES (A) or NO (B) on the YES/NO.
    virtual void answer(bool yes) = 0;
    //  The scene's text to the overworld. False if it didn't come back.
    virtual bool finish() = 0;
    //  From the overworld: the fossil in the bag's Key Items; nothing if
    //  it can't be read.
    virtual std::optional<Fossil> fossil_in_bag() = 0;
};

enum class ChoiceEnd{
    TAKEN,
    //  The prompt offered the other fossil: NO answered.
    WRONG_ONE,
    FAILED,
};
const char* choice_end_name(ChoiceEnd end);

struct ChoiceResult{
    ChoiceEnd end = ChoiceEnd::FAILED;
    //  TAKEN: whether the bag confirmed it.
    bool confirmed = false;
    std::string detail = {};
};

//  Facing the fossil: takes `fossil`.
ChoiceResult run_fossil_choice(Logger& logger, ChoiceActions& actions, Fossil fossil);


//  The console's fossil CHOICE: talks until the YES/NO box shows, has no
//  reader for the prompt's text yet, answers by the box's cursor, and
//  clears the scene to the overworld.
class ConsoleChoiceActions : public ChoiceActions{
public:
    ConsoleChoiceActions(ConsoleHandle& console, ProControllerContext& context);

    virtual bool talk() override;
    virtual std::optional<Fossil> read_offer() override;
    virtual void answer(bool yes) override;
    virtual bool finish() override;
    virtual std::optional<Fossil> fossil_in_bag() override;

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
};


void add_tests_AutoStoryChoice(UnitTestDatabase& database);



}
}
}
#endif

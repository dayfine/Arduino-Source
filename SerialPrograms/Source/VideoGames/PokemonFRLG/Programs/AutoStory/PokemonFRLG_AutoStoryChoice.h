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
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryFossil.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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

//  The outcome of choosing a fossil.
enum class ChoiceEnd{
    TAKEN,
    //  The prompt offered the other fossil: NO answered.
    WRONG_ONE,
    FAILED,
};
//  The human-readable name of a choice outcome.
std::string_view choice_end_name(ChoiceEnd end);

//  The result of a fossil choice attempt.
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


//  Register auto-story fossil choice unit tests.
void add_tests_AutoStoryChoice(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

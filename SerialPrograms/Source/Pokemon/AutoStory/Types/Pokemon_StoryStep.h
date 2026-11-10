/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a tile game's story (FRLG, BDSP) as plain data: its kind
 *  and the fields that kind reads. A game's executor does it
 *  (Pokemon_StoryStepExecutor.h); the shared JSON reads and writes it
 *  (Data/Pokemon_StoryStepJson.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon_StoryArgs.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  A straight leg of a fixed walk.
struct StoryLeg{
    Direction direction = Direction::NORTH;
    int tiles = 0;
};

//  An item and how many to get.
struct StoryItem{
    std::string item;
    size_t quantity = 0;
};

// Specification of an atomic story task with actions and postconditions.
struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name;
    std::string why;

    //  ROUTE: where to go; with several tiles, any of them. READY: the nurse.
    std::optional<Pose> target;
    //  STEP
    Direction direction = Direction::NORTH;
    //  LEGS
    std::vector<StoryLeg> legs;
    //  ROUTE, LEGS: fight wild battles on the way instead of running.
    bool fight_wild = false;
    //  ANSWER
    bool yes = true;
    //  BATTLE: losing goes on (a rival's first battle).
    bool may_lose = false;
    //  TRAIN
    size_t battles = 0;
    //  SHOP
    std::vector<StoryItem> items;
    //  How this game does the kind, when it has more than one way: a
    //  route's plan costs, a door lined up by sight, the Mart lady instead
    //  of the clerk. Empty is the game's usual way. GAME: the step's name
    //  in the game's executor. A plain name; what the action needs beyond
    //  it goes in `args`.
    std::string variant;
    //  The arguments the variant's action reads (a Mart's map and budget),
    //  checked against the ones it declares when the game loads the step.
    StoryArgs args;

    //  Skipped when all of these hold (and there is at least one): names
    //  the game's executor answers (StoryStepExecutor::holds()), run flags
    //  unless the game reads them off the screen.
    std::vector<std::string> skip_if;
    //  Preparation for what follows, done once per save: a TEACH, a check,
    //  a read. Once it has succeeded and changed nothing
    //  (Outcome::changed_nothing), the attempts after a reload of the same
    //  save skip it, so a retry goes from the reload straight to the
    //  battle. One that changed the game (a TM taught) runs again, as the
    //  reload undid it. Only with build_story_checkpoint()'s PreparedSteps.
    bool once_per_save = false;
    //  Where the step leaves the player, when its action does not say: the
    //  end of a scene that moves the player.
    std::optional<Pose> pose_after;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif

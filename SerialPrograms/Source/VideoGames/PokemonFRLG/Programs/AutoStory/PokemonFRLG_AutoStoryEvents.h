/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG story as data for the shared story engine
 *  (Pokemon/AutoStory/Pokemon_StoryEvents.h, Pokemon_StorySegments.h).
 *  A checkpoint is a list of the shared StorySteps
 *  (Pokemon/AutoStory/Pokemon_StoryStepExecutor.h), each one kind of event (a
 *  route, a talk, text read to its end, a YES/NO, a battle, the nurse, the
 *  Mart); StoryExecutor does each kind on the console, and
 *  make_story_segment() turns the table into engine checkpoints. Routes
 *  plan from the pose the run carries and return where they leave the
 *  player; scenes that move the player say where in the table.
 *
 *  The table itself is in PokemonFRLG_AutoStoryTable.h; it is plain data,
 *  so its routes are planned in unit tests without a console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEvents_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  The variant of a TALK to someone who wanders (MOVEMENT_TYPE_WANDER_AROUND):
//  the player faces a tile of their zone, and A is pressed until they step
//  onto it and a message opens.
inline const std::string TALK_WANDERER_VARIANT = "wanderer";

//  What the story's events act on. `supplies` are what the bag holds now,
//  `saved_supplies` what the last save holds.
struct StoryEnv{
    ConsoleHandle& console;
    ProControllerContext& context;
    Supplies supplies;
    Supplies saved_supplies;
    //  Whether the game has a save to reload.
    bool has_save = false;
    Pokemon::StoryCheckLevel checks = Pokemon::StoryCheckLevel::DEFAULT;
    //  The game's language, for reading item names. In one the readers have
    //  no names for, the checks fall back to positions and counts.
    Language language = Language::English;
    //  The name the new game's intro types for the player; empty keeps the
    //  game's default name.
    std::string player_name = {};
    //  The map the checkpoint under way starts on, where its save is: a
    //  reload checks the save card against it.
    std::string start_map = {};
    //  That checkpoint's START menu: a reload reads the bag again with it.
    StartMenu start_menu = StartMenu::BEFORE_POKEMON;
    //  The once_per_save steps done from the last save that changed
    //  nothing: a retry after a reload skips them. Emptied by a save.
    Pokemon::PreparedSteps prepared = {};
    //  The last save's Items pocket was read in full (read_bag_supplies()):
    //  a reload restores `saved_supplies` without reading it again.
    bool bag_read = false;
    //  The frames of where each checkpoint starts, for the start check: a
    //  save at a checkpoint's end keeps one for the next checkpoint.
    StartFrameStore start_frames = {};
    //  The next start check keeps the screen as its checkpoint's start
    //  frame and passes: the user saved there by hand.
    bool keep_start_frame = false;
    //  The player: the one a new game picks, and whose sprites the facing
    //  reader matches. A save must be of this player.
    PlayerGender player_gender = PlayerGender::BOY;
    //  The Items pocket's counts a count_items step kept
    //  (PokemonFRLG_AutoStorySlopeRun.h), for the next to compare.
    std::map<std::string, int> counted_items = {};
    //  The species the party's summaries were read as, for a Fly or a TEACH
    //  that names one, and who uses each field move (`party.users`, set by a
    //  TEACH of one or a Fly's scan). The species are emptied when a save is
    //  loaded; both when a PC deposit or withdrawal moves members. A catch
    //  or a gift adds a member at the end, so the slots stay right.
    PartyCache party = {};
    //  Who uses the field moves as the last save holds it, which a load
    //  restores into `party.users`. The program starts it from its settings.
    FieldMoveUsers saved_field_moves = {};
    //  Called with `saved_field_moves` after each save, to keep them in the
    //  program's settings for the next run; empty: nothing kept.
    std::function<void(const FieldMoveUsers&)> keep_saved_field_moves = {};
};

//  After a save is loaded for a checkpoint that starts on `start_map`: the
//  save card's place, read from the START menu, against it, at every checks
//  level. Empty if the save is there (or the card can't be read), else why
//  not.
std::string check_save_place(StoryEnv& env, const std::string& start_map);

//  After a save is loaded, with START's menu `menu`: the lead's moves as its
//  summary shows them, which the battles then start from in place of the
//  row's (in env's supplies and the ones a reload restores). The row's are
//  kept if no move can be read.
void read_lead_moves(StoryEnv& env, StartMenu menu);

//  After a save is loaded, with START's menu `menu`: the Items pocket's
//  counts as read (set_from_loaded_bag()), in place of the row's, which
//  are kept if nothing can be read. Only with the full menu: the rows
//  before it carry the items they have. Sets `bag_read` when every count
//  was read.
void read_bag_supplies(StoryEnv& env, StartMenu menu);

//  Does FRLG's kinds of story step on the console, each with its row of
//  story_action_rows() (PokemonFRLG_AutoStoryStepTable.h). A console step
//  that fails is a failed Outcome, which the segment answers by reloading.
//  Before each step it clears what came up since the step before
//  (step_interrupts(), PokemonFRLG_AutoStoryStepInterrupts.h).
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);
    //  Its table's actions point at it.
    StoryExecutor(const StoryExecutor&) = delete;
    StoryExecutor& operator=(const StoryExecutor&) = delete;

    StoryEnv& env(){
        return m_env;
    }
    //  The START menu SET_OPTIONS leaves: the checkpoint's being built.
    void set_menu(StartMenu menu){
        m_menu = menu;
    }

    //  The step's action from the table.
    virtual Pokemon::Outcome run(const StoryStep& step, Pokemon::RunState& state) override;

private:
    //  How the console does `id`.
    Pokemon::StoryAction console_action(StoryActionId id);

    StoryEnv& m_env;
    StartMenu m_menu = StartMenu::FULL;
    //  A failed step fails the step; an interrupt between steps is logged.
    InterruptLogGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
    //  What is cleared between steps, looked for on the screen.
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

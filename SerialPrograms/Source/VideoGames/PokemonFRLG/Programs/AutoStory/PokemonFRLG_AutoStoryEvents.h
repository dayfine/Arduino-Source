/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story executor, console env, and #648 StoryEngine checkpoint and
 *  save policy builders (S2 FRLG-owned pose, S15 StoryFailure, E6 Hall of
 *  Fame self-save policy).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEvents_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StorySavePolicy.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryCheckpoints.h"
#include "PokemonFRLG_AutoStoryItemsPocketChange.h"
#include "PokemonFRLG_AutoStoryLineTrainer.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryPartyRoster.h"
#include "PokemonFRLG_AutoStorySaveChecks.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline const std::string TALK_WANDERER_VARIANT = "wanderer";

//  What the story's events act on.
struct StoryEnv{
    ConsoleHandle& console;
    ProControllerContext& context;
    Supplies supplies = {};
    Supplies saved_supplies = {};
    //  Whether the game has a save to reload, and which checkpoint's start it is at.
    bool has_save = false;
    std::string saved_checkpoint_id = {};
    Pokemon::StoryCheckLevel checks = Pokemon::StoryCheckLevel::DEFAULT;
    Language language = Language::English;
    std::string player_name = {};
    std::string start_map = {};
    std::string checkpoint = {};
    StartMenu start_menu = StartMenu::BEFORE_POKEMON;
    //  once_per_save steps completed since the last save.
    PreparedSteps prepared = {};
    bool bag_read = false;
    StartFrameStore start_frames = {};
    bool keep_start_frame = false;
    PlayerGender player_gender = PlayerGender::BOY;
    std::map<std::string, int> counted_items = {};
    PartyCache party = {};
    SavedFieldMoveUsers saved_field_moves = {};
    bool take_master_ball = true;
    uint16_t nugget_farm_count = 10;
    Pokemon::StoryPausePoints pause = {};
    //  FRLG-owned grid pose, gates, and flags (S2).
    FrlgRunState run_state = {};
};

std::string check_save_place(StoryEnv& env, const std::string& start_map);
void read_lead_moves(StoryEnv& env, StartMenu menu);
void read_bag_supplies(StoryEnv& env, StartMenu menu);

//  Executes FRLG story steps on the console against FRLG's FrlgRunState (S2).
class StoryExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);
    StoryExecutor(const StoryExecutor&) = delete;
    StoryExecutor& operator=(const StoryExecutor&) = delete;

    StoryEnv& env(){
        return m_env;
    }
    void set_menu(StartMenu menu){
        m_menu = menu;
    }
    void set_pause_points(Pokemon::StoryPausePoints* pause){
        m_pause = pause;
    }

    //  Executes one step of checkpoint `checkpoint_id` on `state`: pause,
    //  skip_if, once_per_save, check-level gate, between-step interrupts,
    //  step action, pose_after/gates_after/flags_after, and pocket staleness.
    std::optional<Pokemon::StoryFailure> run_step(
        std::string_view checkpoint_id, const StoryStep& step, FrlgRunState& state
    );

    static bool master_ball_skipped(bool take_master_ball, Pokemon::StoryCheckLevel checks);
    static bool nugget_farm_skipped(uint16_t nugget_farm_count);
    static int64_t effective_nugget_farm_count(uint16_t nugget_farm_count, int64_t step_count);
    bool holds(std::string_view condition, const FrlgRunState& state) const;

private:
    FrlgStepAction console_action(StoryActionId id);

    StoryEnv& m_env;
    StartMenu m_menu = StartMenu::FULL;
    Pokemon::StoryPausePoints* m_pause = nullptr;
    InterruptLogGuard m_guard;
    FrlgActionTable m_table;
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};

//  Returns true when `data` ends with the Hall of Fame self-save step (E6).
bool is_hall_of_fame_checkpoint(const StoryCheckpoint& data);

//  Builds a #648 Pokemon::StoryCheckpoint from FRLG's `data` for execution
//  on StoryEngine::run_checkpoints(). Trailing hall_of_fame steps are omitted
//  from the checkpoint's steps and executed by frlg_save_policy() instead (E6).
Pokemon::StoryCheckpoint make_frlg_engine_checkpoint(
    const StoryCheckpoint& data, StoryExecutor& executor, FrlgRunState& run_state
);

//  FRLG's save policy for #648 StoryGame::save (E6): runs menu save_game()
//  with end-map verification & StoryEnv bookkeeping, or runs Hall of Fame
//  self-save when `passed` is a Hall of Fame checkpoint.
Pokemon::StorySavePolicy frlg_save_policy(
    StoryEnv& env,
    const std::vector<StoryCheckpoint>& rows,
    StoryExecutor& executor,
    FrlgRunState& run_state
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

/*  AutoStory Step Interrupts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepInterrupts_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepInterrupts_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeScreenObserver.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventLog.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


struct StepInterruptActions{
    Pokemon::InterruptAction wild = {};
    Pokemon::InterruptAction trainer = {};
    Pokemon::InterruptAction dialog = {};
    Pokemon::InterruptAction warp = {};
};

Pokemon::StoryInterrupts step_interrupts(Pokemon::ScreenLookFunction look, const StepInterruptActions& actions);

StepInterruptActions console_interrupt_actions(
    ConsoleHandle& console, ProControllerContext& context,
    std::function<OverworldInterruptSet()> walker
);

//  Whether a step with S11 action name `action` presses through text showing
//  when it starts.
bool presses_through_text(std::string_view action);


constexpr Milliseconds BETWEEN_STEPS_LOOK = Milliseconds(400);

class ConsoleScreenLook{
public:
    ConsoleScreenLook(ConsoleHandle& console, ProControllerContext& context);

    Pokemon::ScreenLook look(bool fresh);

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    FrlgScreenObserver m_observer;
    SwitchHomeObserver m_eyes;
    ScreenEventWatcher m_watcher;
    WallClock m_start;
};

class InterruptLogGuard{
public:
    explicit InterruptLogGuard(Logger& logger, std::string failures_folder = SCREENSHOTS_PATH() + "AutoStory/failures/")
        : m_logger(logger)
        , m_failures_folder(std::move(failures_folder))
    {}

    void on_failure(const StoryStep& step, const OperationFailedException& error);
    void save_frame(std::string_view name, const OperationFailedException& error);
    void on_interrupt(const StoryStep& step, const Pokemon::InterruptCheck& check);
    void on_screen(const StoryStep& step, const Pokemon::ScreenSettle& screen);
    void on_check_skipped(const StoryStep& step, const std::string& line);

private:
    Logger& m_logger;
    std::string m_failures_folder;
};


void add_tests_AutoStoryStepInterrupts(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

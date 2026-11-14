/*  Story Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "Pokemon_StoryLaunch.h"

namespace PokemonAutomation{
namespace Pokemon{

using namespace NintendoSwitch;


namespace{


//  What a look shows, as far as a launch cares.
enum class LaunchScreen{
    HOME,
    NEW_GAME,
    PROMPT,
    TITLE,
    OVERWORLD,
    OTHER,
};

struct LaunchLook{
    LaunchScreen screen = LaunchScreen::OTHER;
    //  The prompt on screen, for PROMPT.
    const StoryLaunchPrompt* prompt = nullptr;
};

//  `look` as `policy` reads it. HOME first: a game's detectors may read
//  HOME's plain screen as a quiet overworld.
LaunchLook read_launch_look(const StoryLaunchPolicy& policy, const ScreenObservation& look){
    if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
        return {.screen = LaunchScreen::HOME};
    }
    if (policy.new_game && policy.new_game(look)){
        return {.screen = LaunchScreen::NEW_GAME};
    }
    for (const StoryLaunchPrompt& prompt : policy.prompts){
        if (prompt.matches && prompt.matches(look)){
            return {.screen = LaunchScreen::PROMPT, .prompt = &prompt};
        }
    }
    if (policy.title(look)){
        return {.screen = LaunchScreen::TITLE};
    }
    const bool overworld = policy.overworld ? policy.overworld(look) : look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    return {.screen = overworld ? LaunchScreen::OVERWORLD : LaunchScreen::OTHER};
}

Outcome launch_failed(std::string detail){
    return outcome_failed(std::string(STORY_LAUNCH_FAILED), std::move(detail));
}

//  Why `policy` cannot launch `request`, before any press; nothing when it
//  can.
std::optional<Outcome> launch_problem(const StoryLaunchPolicy& policy, StoryLaunchRequest request){
    if (!policy.tile_selected || !policy.title){
        return launch_failed(std::format("{}: the launch policy has no tile or title check", policy.game));
    }
    if (request.save == StoryLaunchRequest::Save::BACKUP && policy.backup_save.empty()){
        return launch_failed(std::format("{}: the game has no backup save", policy.game));
    }
    return std::nullopt;
}


//  One launch: a look, one thing done about it, until the overworld has
//  settled or a screen took too many presses.
template <StoryPose PoseType>
class LaunchDriver{
public:
    LaunchDriver(StoryBackend<PoseType>& backend, const StoryLaunchPolicy& policy, StoryLaunchRequest request)
        : m_backend(backend)
        , m_policy(policy)
        , m_request(request)
    {}

    //  One look and its answer; the launch's end when this look ended it.
    std::optional<Outcome> step(){
        const Observation<PoseType> observation = m_backend.observe();
        const LaunchLook look = read_launch_look(m_policy, observation);
        if (look.screen != LaunchScreen::OVERWORLD){
            m_settled = 0;
        }
        switch (look.screen){
        case LaunchScreen::HOME:
            return on_home(observation);
        case LaunchScreen::NEW_GAME:
            return launch_failed(std::format("{}: the game shows a new game: no save to load, and a launch never starts one", m_policy.game));
        case LaunchScreen::PROMPT:
            return press(look.prompt->name, look.prompt->press, look.prompt->max_presses);
        case LaunchScreen::TITLE:
            return on_title();
        case LaunchScreen::OVERWORLD:
            return on_overworld();
        case LaunchScreen::OTHER:
            break;
        }
        return on_other();
    }

private:
    //  Whether an overworld up now is not yet the save asked for.
    bool needs_load() const{
        return !m_loading && (m_request.running == StoryLaunchRequest::Running::RESTART || m_request.save == StoryLaunchRequest::Save::BACKUP);
    }

    //  The cursor to the game's tile, then the game closed if it must
    //  load again, then A on the tile.
    std::optional<Outcome> on_home(const ScreenObservation& observation){
        if (!m_policy.tile_selected(observation)){
            return move_to_tile();
        }
        if (needs_load()){
            m_loading = true;
            return press("home", {.button = BUTTON_X}, m_policy.presses_per_screen, "close the game");
        }
        m_launched = true;
        return press("home", {.button = BUTTON_A}, m_policy.presses_per_screen, "launch the game");
    }

    std::optional<Outcome> move_to_tile(){
        const size_t left = m_policy.tile_search;
        if (m_tile_moves >= 3 * left){
            return launch_failed(std::format("{}: the game's tile is not on HOME: {} tiles searched", m_policy.game, m_tile_moves));
        }
        const Button button = m_tile_moves < left ? BUTTON_LEFT : BUTTON_RIGHT;
        m_tile_moves++;
        log("home", std::format("not on the game's tile: {}", button_to_string(button)));
        m_backend.press(button, Milliseconds(80), Milliseconds(400));
        return std::nullopt;
    }

    std::optional<Outcome> on_title(){
        m_loading = true;
        m_launched = true;
        if (m_request.save == StoryLaunchRequest::Save::BACKUP && !m_backup_sent){
            m_backup_sent = true;
            log("title", "the backup save");
            for (const StoryLaunchPress& each : m_policy.backup_save){
                m_backend.press(each.button, each.hold, each.release);
            }
        }
        return press("title", m_policy.title_press, m_policy.presses_per_screen);
    }

    std::optional<Outcome> on_overworld(){
        if (needs_load()){
            return press("overworld", {.button = BUTTON_HOME}, m_policy.presses_per_screen, "HOME, to load the save");
        }
        if (++m_settled >= m_policy.settled_looks){
            log("overworld", std::format("settled for {} looks", m_settled));
            return outcome_ok();
        }
        m_backend.wait(m_policy.poll);
        return std::nullopt;
    }

    //  A screen the policy doesn't know: the game's boot or a load once
    //  launched; before that the game was in front already, left with HOME
    //  to load the save, or B toward its overworld.
    std::optional<Outcome> on_other(){
        if (m_launched || m_loading){
            m_backend.wait(m_policy.poll);
            return std::nullopt;
        }
        if (needs_load()){
            return press("unknown screen", {.button = BUTTON_HOME}, m_policy.presses_per_screen, "HOME, to load the save");
        }
        return press("unknown screen", {.button = BUTTON_B}, m_policy.presses_per_screen, "B, toward the overworld");
    }

    //  `what` on `screen`, unless `screen` already took `max` presses.
    std::optional<Outcome> press(std::string_view screen, const StoryLaunchPress& what, size_t max, std::string_view why = {}){
        size_t& count = m_presses[std::string(screen)];
        if (count >= max){
            return launch_failed(std::format("{}: stuck on {}: {} presses changed nothing", m_policy.game, screen, count));
        }
        count++;
        log(screen, why.empty() ? button_to_string(what.button) : std::string(why));
        m_backend.press(what.button, what.hold, what.release);
        return std::nullopt;
    }

    void log(std::string_view screen, std::string_view what){
        m_backend.log(std::format("[Launch] {}: {}: {}", m_policy.game, screen, what));
    }

    StoryBackend<PoseType>& m_backend;
    const StoryLaunchPolicy& m_policy;
    StoryLaunchRequest m_request;
    std::map<std::string, size_t, std::less<>> m_presses;
    size_t m_tile_moves = 0;
    size_t m_settled = 0;
    //  The game was closed from HOME, or showed its title: what comes up
    //  next is a save loading.
    bool m_loading = false;
    //  A on the game's tile, or the title: until the overworld, a screen
    //  the policy doesn't know is the game booting or loading.
    bool m_launched = false;
    bool m_backup_sent = false;
};


} // namespace


template <StoryPose PoseType>
Outcome launch_story_game(StoryBackend<PoseType>& backend, const StoryLaunchPolicy& policy, StoryLaunchRequest request){
    if (std::optional<Outcome> problem = launch_problem(policy, request)){
        backend.log(std::format("[Launch] {}", problem->detail));
        return std::move(*problem);
    }
    LaunchDriver<PoseType> driver(backend, policy, request);
    for (size_t looks = 0; looks < policy.max_looks; looks++){
        backend.scope().throw_if_cancelled();
        if (std::optional<Outcome> end = driver.step()){
            if (!end->succeeded()){
                backend.log(std::format("[Launch] {}", end->detail));
            }
            return std::move(*end);
        }
    }
    const Outcome failed = launch_failed(std::format("{}: no overworld after {} looks", policy.game, policy.max_looks));
    backend.log(std::format("[Launch] {}", failed.detail));
    return failed;
}

template Outcome launch_story_game<GridPose>(StoryBackend<GridPose>&, const StoryLaunchPolicy&, StoryLaunchRequest);
template Outcome launch_story_game<ZonePose>(StoryBackend<ZonePose>&, const StoryLaunchPolicy&, StoryLaunchRequest);



} // namespace Pokemon
} // namespace PokemonAutomation

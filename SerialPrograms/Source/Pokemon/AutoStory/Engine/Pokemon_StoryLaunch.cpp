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
#include <utility>
#include "NintendoSwitch/Inference/NintendoSwitch_HomeTileReader.h"
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

//  `look` as `policy` reads it, `launched` once the launch launched or
//  closed the game. HOME first: a game's detectors may read HOME's plain
//  screen as a quiet overworld.
LaunchLook read_launch_look(const StoryLaunchPolicy& policy, const ScreenObservation& look, bool launched){
    if (look.events.has(ScreenEvent::SWITCH_HOME_MENU)){
        return {.screen = LaunchScreen::HOME};
    }
    if (policy.new_game && policy.new_game(look)){
        return {.screen = LaunchScreen::NEW_GAME};
    }
    for (const StoryLaunchPrompt& prompt : policy.prompts){
        if ((launched || !prompt.after_launch) && prompt.matches && prompt.matches(look)){
            return {.screen = LaunchScreen::PROMPT, .prompt = &prompt};
        }
    }
    if ((launched || !policy.title_after_launch) && policy.title(look)){
        return {.screen = LaunchScreen::TITLE};
    }
    const bool overworld = policy.overworld ? policy.overworld(look) : look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    return {.screen = overworld ? LaunchScreen::OVERWORLD : LaunchScreen::OTHER};
}

StoryFailure launch_failed(std::string detail){
    return StoryFailure{.code = std::string(STORY_LAUNCH_FAILED), .detail = std::move(detail), .repeats = true};
}

//  Why `policy` cannot launch `request`, before any press; nothing when it
//  can.
std::optional<StoryFailure> launch_problem(const StoryLaunchPolicy& policy, StoryLaunchRequest request){
    if (!policy.tile_selected || !policy.title){
        return launch_failed(std::format("{}: the launch policy has no tile or title check", policy.game));
    }
    if (request.save == StoryLaunchRequest::Save::BACKUP && policy.backup_save.empty()){
        return launch_failed(std::format("{}: the game has no backup save", policy.game));
    }
    return std::nullopt;
}


//  How one step of the launch ended: keep going, succeeded, or failed.
struct LaunchStepEnd{
    bool done = false;
    std::optional<StoryFailure> failure = std::nullopt;
};

//  One launch: a look, one thing done about it, until the overworld has
//  settled or a screen took too many presses.
class LaunchDriver{
public:
    LaunchDriver(StoryBackend& backend, const StoryLaunchPolicy& policy, StoryLaunchRequest request)
        : m_backend(backend)
        , m_policy(policy)
        , m_request(request)
    {}

    //  One look and its answer; done when this look ended the launch.
    LaunchStepEnd step(){
        const ScreenObservation observation = m_backend.observe();
        if (m_title_pressed && observation.events.has(ScreenEvent::FADE_BLACK)){
            m_load_seen = true;
        }
        const LaunchLook look = read_launch_look(m_policy, observation, m_launched || m_loading);
        if (look.screen != LaunchScreen::OVERWORLD){
            m_settled = 0;
        }
        switch (look.screen){
        case LaunchScreen::HOME:
            return on_home(observation);
        case LaunchScreen::NEW_GAME:
            return {.done = true, .failure = launch_failed(std::format("{}: the game shows a new game: no save to load, and a launch never starts one", m_policy.game))};
        case LaunchScreen::PROMPT:
            return on_prompt(*look.prompt);
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
    LaunchStepEnd on_home(const ScreenObservation& observation){
        if (m_launch_pressed && m_home_waited < m_policy.launch_wait){
            if (m_home_waited == Milliseconds(0)){
                log("home", "waiting for the game to start");
            }
            m_home_waited += m_policy.poll;
            m_backend.wait(m_policy.poll);
            return {};
        }
        if (!m_policy.tile_selected(observation)){
            return move_to_tile();
        }
        if (needs_load()){
            m_loading = true;
            return press("home", {.button = BUTTON_X}, m_policy.presses_per_screen, "close the game");
        }
        m_launched = true;
        m_launch_pressed = true;
        m_home_waited = Milliseconds(0);
        return press("home", {.button = BUTTON_A}, m_policy.presses_per_screen, "launch the game");
    }

    LaunchStepEnd move_to_tile(){
        const size_t left = m_policy.tile_search;
        if (m_tile_moves >= 3 * left){
            return {.done = true, .failure = launch_failed(std::format("{}: the game's tile is not on HOME: {} tiles searched", m_policy.game, m_tile_moves))};
        }
        const Button button = m_tile_moves < left ? BUTTON_LEFT : BUTTON_RIGHT;
        m_tile_moves++;
        log("home", std::format("not on the game's tile: {}", button_to_string(button)));
        m_backend.press(button, Milliseconds(80), Milliseconds(400));
        return {};
    }

    //  The prompt's lead-in presses, then its press.
    LaunchStepEnd on_prompt(const StoryLaunchPrompt& prompt){
        if (m_presses[prompt.name] < prompt.max_presses){
            for (const StoryLaunchPress& each : prompt.before){
                m_backend.press(each.button, each.hold, each.release);
            }
        }
        return press(prompt.name, prompt.press, prompt.max_presses);
    }

    LaunchStepEnd on_title(){
        m_loading = true;
        m_launched = true;
        if (m_request.save == StoryLaunchRequest::Save::BACKUP && !m_backup_sent){
            m_backup_sent = true;
            log("title", "the backup save");
            for (const StoryLaunchPress& each : m_policy.backup_save){
                m_backend.press(each.button, each.hold, each.release);
            }
        }
        //  A black before this title was not the load.
        m_load_seen = false;
        m_title_pressed = true;
        return press("title", m_policy.title_press, m_policy.presses_per_screen);
    }

    LaunchStepEnd on_overworld(){
        if (needs_load()){
            return press("overworld", {.button = BUTTON_HOME, .release = Milliseconds(1000)}, m_policy.presses_per_screen, "HOME, to load the save");
        }
        if (m_loading && !m_launched){
            //  Closing the game on HOME: a mid-fade frame before HOME returns
            //  is not the title's attract movie yet.
            m_backend.wait(m_policy.poll);
            return {};
        }
        if (m_policy.overworld_after_load && m_loading && !m_load_seen){
            //  Not the load's press: on the movie, it goes back to the
            //  title through a black.
            return press(
                "title", m_policy.title_press, m_policy.presses_per_screen,
                std::format("no load yet, so the title's attract movie: {}", button_to_string(m_policy.title_press.button))
            );
        }
        if (++m_settled >= m_policy.settled_looks){
            log("overworld", std::format("settled for {} looks", m_settled));
            return {.done = true, .failure = std::nullopt};
        }
        m_backend.wait(m_policy.poll);
        return {};
    }

    //  A screen the policy doesn't know: the game's boot or a load once
    //  launched; before that the game was in front already, left with HOME
    //  to load the save, or B toward its overworld.
    LaunchStepEnd on_other(){
        if (m_launched || m_loading){
            m_backend.wait(m_policy.poll);
            return {};
        }
        if (needs_load()){
            return press("unknown screen", {.button = BUTTON_HOME, .release = Milliseconds(1000)}, m_policy.presses_per_screen, "HOME, to load the save");
        }
        return press("unknown screen", {.button = BUTTON_B}, m_policy.presses_per_screen, "B, toward the overworld");
    }

    //  `what` on `screen`, unless `screen` already took `max` presses.
    LaunchStepEnd press(std::string_view screen, const StoryLaunchPress& what, size_t max, std::string_view why = {}){
        size_t& count = m_presses[std::string(screen)];
        if (count >= max){
            return {.done = true, .failure = launch_failed(std::format("{}: stuck on {}: {} presses changed nothing", m_policy.game, screen, count))};
        }
        count++;
        log(screen, why.empty() ? button_to_string(what.button) : std::string(why));
        m_backend.press(what.button, what.hold, what.release);
        return {};
    }

    void log(std::string_view screen, std::string_view what){
        m_backend.log(std::format("[Launch] {}: {}: {}", m_policy.game, screen, what));
    }

    StoryBackend& m_backend;
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
    //  A was pressed on the game's tile, and HOME has shown for
    //  `m_home_waited` since.
    bool m_launch_pressed = false;
    Milliseconds m_home_waited = Milliseconds(0);
    bool m_backup_sent = false;
    //  The title press was sent on a look `policy.title` read as the
    //  title; a press on the attract movie (`overworld_after_load`) is not.
    bool m_title_pressed = false;
    //  A black screen showed after the title press, and the title has not
    //  shown since: the save loading.
    bool m_load_seen = false;
};


} // namespace


std::function<bool(const ScreenObservation& look)> switch_home_tile_named(Language language, std::string name){
    return [language, name = std::move(name)](const ScreenObservation& look){
        return look.frame && look.frame->frame && selected_home_tile_is(language, *look.frame->frame, name);
    };
}


std::optional<StoryFailure> launch_story_game(StoryBackend& backend, const StoryLaunchPolicy& policy, StoryLaunchRequest request){
    if (std::optional<StoryFailure> problem = launch_problem(policy, request)){
        backend.log(std::format("[Launch] {}", problem->detail));
        return problem;
    }
    LaunchDriver driver(backend, policy, request);
    for (size_t looks = 0; looks < policy.max_looks; looks++){
        backend.scope().throw_if_cancelled();
        LaunchStepEnd end = driver.step();
        if (end.done){
            if (end.failure){
                backend.log(std::format("[Launch] {}", end.failure->detail));
            }
            return std::move(end.failure);
        }
    }
    StoryFailure failed = launch_failed(std::format("{}: no overworld after {} looks", policy.game, policy.max_looks));
    backend.log(std::format("[Launch] {}", failed.detail));
    return failed;
}



} // namespace Pokemon
} // namespace PokemonAutomation

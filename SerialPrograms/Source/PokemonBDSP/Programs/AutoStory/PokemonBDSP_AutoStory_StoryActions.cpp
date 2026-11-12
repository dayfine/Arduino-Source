/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <map>
#include <optional>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryActionRegistry.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryGame.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryStepRunner.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"
#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::GridPose;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;

using GameAction = Pokemon::StoryAction<GridPose>;


namespace{

//  `act` as the engine's handler: it runs the step the engine runs, with
//  the step's own args.
Pokemon::StoryActionHandler<GridPose> handler_of(StoryBoundAction act){
    return [act = std::move(act)](Pokemon::StoryBackend<GridPose>&, const Pokemon::StoryActionCall& call, RunState& state){
        return act(call.step, state);
    };
}

//  What a step of a kind BDSP has no action for does.
Outcome unsupported(Pokemon::StoryBackend<GridPose>&, const Pokemon::StoryActionCall& call, RunState&){
    return Pokemon::unsupported_step(call.step);
}

//  The variants a kind's action reads that have no spec of their own: they
//  take the kind's default's args.
const std::map<StoryStepKind, std::vector<std::string>>& variants_without_spec(){
    static const std::map<StoryStepKind, std::vector<std::string>> variants{
        {StoryStepKind::LEGS, {"measured"}},
        {StoryStepKind::STEP, {"tap"}},
    };
    return variants;
}

//  Adds `act` for `kind`: first as the kind's default, `name`, which is the
//  built-in action's name where there is one, so it takes the built-in's
//  place (the engine's default is a kind's first action), with the args of
//  the kind's first spec in bdsp_story_actions(); then for each of the
//  kind's other specs and variants. A name that is also a story event stays
//  the event's.
void add_kind(std::vector<GameAction>& actions, StoryStepKind kind, std::string name, const StoryBoundAction& act){
    const std::span<const Pokemon::StoryActionSpec> specs = bdsp_story_actions();
    Pokemon::StoryActionSpec first{.kind = kind, .name = std::move(name), .args = {}};
    if (const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(specs, kind, "")){
        first.args = spec->args;
    }
    actions.push_back({.spec = first, .handler = handler_of(act)});
    for (const Pokemon::StoryActionSpec& spec : specs){
        if (spec.kind != kind || spec.name.empty() || spec.name == first.name || story_events().contains(spec.name)){
            continue;
        }
        actions.push_back({.spec = spec, .handler = handler_of(act)});
    }
    const auto variants = variants_without_spec().find(kind);
    if (variants == variants_without_spec().end()){
        return;
    }
    for (const std::string& variant : variants->second){
        actions.push_back({.spec = {.kind = kind, .name = variant, .args = first.args}, .handler = handler_of(act)});
    }
}

//  Each story event, for a step of any kind, with its spec's args if it has any.
void add_events(std::vector<GameAction>& actions, const StoryBoundActions& bound){
    for (const auto& item : story_events()){
        std::vector<Pokemon::StoryArgSpec> args;
        for (const Pokemon::StoryActionSpec& spec : bdsp_story_actions()){
            if (spec.name == item.first && !spec.args.empty()){
                args = spec.args;
                break;
            }
        }
        const Pokemon::StoryActionHandler<GridPose> event = handler_of(bound.event(item.first));
        for (int kind = 0; kind <= (int)StoryStepKind::GAME; kind++){
            actions.push_back({.spec = {.kind = (StoryStepKind)kind, .name = item.first, .args = args}, .handler = event});
        }
    }
}

//  BDSP's story actions as the engine's game: the walking steps on
//  `walks`, the rest `bound`'s.
Pokemon::StoryGame<GridPose> story_game(StoryWalkSteps& walks, const StoryBoundActions& bound){
    Pokemon::StoryGame<GridPose> game;
    game.id = "bdsp";
    std::vector<GameAction>& actions = game.actions;
    //  A GAME step names its event: an unnamed one finds this.
    actions.push_back({.spec = {.kind = StoryStepKind::GAME, .name = "", .args = {}}, .handler = &unsupported});

    using WalkMethod = Outcome (StoryWalkSteps::*)(RunState&, const StoryStep&);
    const auto walk = [&](StoryStepKind kind, std::string name, WalkMethod method){
        add_kind(actions, kind, std::move(name), [&walks, method](const StoryStep& step, RunState& state){
            return (walks.*method)(state, step);
        });
    };
    walk(StoryStepKind::ROUTE, "route", &StoryWalkSteps::route);
    walk(StoryStepKind::LEGS, "legs", &StoryWalkSteps::legs);
    walk(StoryStepKind::STEP, "face", &StoryWalkSteps::step);
    walk(StoryStepKind::TALK, "talk", &StoryWalkSteps::talk);
    walk(StoryStepKind::MASH, "mash", &StoryWalkSteps::mash);

    add_kind(actions, StoryStepKind::NEW_GAME, "new_game", bound.new_game);
    add_kind(actions, StoryStepKind::OPTIONS, "options", bound.options);
    add_kind(actions, StoryStepKind::ANSWER, "answer", bound.answer);
    add_kind(actions, StoryStepKind::BATTLE, "fight", bound.battle);
    add_kind(actions, StoryStepKind::TRAIN, "train", bound.train);
    add_kind(actions, StoryStepKind::HEAL, "heal", bound.heal);
    add_kind(actions, StoryStepKind::SAVE, "save", bound.save);
    //  A row's start save names the map its card must show (make_story_segment()).
    actions.back().spec.args.push_back(Pokemon::StoryArgSpec{
        .key = std::string(SAVES_IN_ARG), .type = Pokemon::StoryArgType::WORD, .required = false,
        .fallback = std::nullopt, .why = "the map the save card must name; none saves anywhere",
    });

    //  No SHOP or READY of BDSP's own: the built-in ones would need the game's.
    for (const Pokemon::StoryActionSpec& spec : Pokemon::builtin_story_actions()){
        if (spec.kind == StoryStepKind::SHOP || spec.kind == StoryStepKind::READY){
            actions.push_back({.spec = spec, .handler = &unsupported});
        }
    }
    add_events(actions, bound);
    return game;
}

//  Why the backend's inputs throw.
inline constexpr std::string_view NO_BACKEND_INPUT = "BDSP's story actions play through StoryStepIO, not the backend's inputs.";

} // namespace


StoryLogBackend::StoryLogBackend(Pokemon::StoryBackendKind kind, std::function<void(std::string_view)> log, CancellableScope& scope)
    : m_kind(kind)
    , m_log(std::move(log))
    , m_scope(scope)
{}

void StoryLogBackend::press(Button, Milliseconds, Milliseconds){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void StoryLogBackend::tap(Pokemon::Direction){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void StoryLogBackend::hold(Pokemon::Direction, Milliseconds, Pokemon::StoryPace){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void StoryLogBackend::stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}
void StoryLogBackend::wait(Milliseconds){
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_BACKEND_INPUT));
}

Pokemon::Observation<GridPose> StoryLogBackend::observe(){
    return {};
}

Outcome StoryLogBackend::battle(const Pokemon::StoryActionArgs&){
    return Pokemon::outcome_failed("unsupported_step", "BDSP fights in its BATTLE action");
}

void StoryLogBackend::log(std::string_view line){
    if (m_log){
        m_log(line);
    }
}


//  The game first: the registry and the runner keep references to it.
struct StoryEngineSteps::Engine{
    Engine(StoryWalkSteps& walks, const StoryBoundActions& bound, Pokemon::StoryBackend<GridPose>& backend)
        : game(story_game(walks, bound))
        , registry(game)
        , runner(registry, backend)
    {}

    Pokemon::StoryGame<GridPose> game;
    Pokemon::StoryActionRegistry<GridPose> registry;
    Pokemon::StoryStepRunner<GridPose> runner;
};

StoryEngineSteps::StoryEngineSteps(StoryWalkSteps& walks, const StoryBoundActions& bound, Pokemon::StoryBackend<GridPose>& backend)
    : m_engine(std::make_unique<Engine>(walks, bound, backend))
{}

StoryEngineSteps::~StoryEngineSteps() = default;

Outcome StoryEngineSteps::act(const StoryStep& step, RunState& state){
    return m_engine->runner.act(step, state);
}

std::vector<std::string> StoryEngineSteps::check(const StoryStep& step) const{
    return m_engine->registry.check(step);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation

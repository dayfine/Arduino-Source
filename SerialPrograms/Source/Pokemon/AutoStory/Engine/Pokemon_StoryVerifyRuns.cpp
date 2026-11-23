/*  Story Verify Runs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include <utility>
#include "Pokemon_StoryVerifyRuns.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string step_name_at(const StoryCheckpoint& cp, const std::optional<size_t>& index){
    if (index && *index < cp.steps.size()){
        return cp.steps[*index].name;
    }
    return {};
}

//  Reloads `game` for checkpoint `id` into `state`. Returns nullopt on success
//  (landed at `id`), else the reload failure.
std::optional<StoryFailure> reload_for_verify(
    StoryBackend& backend, std::string_view id, StoryGame& game, RunState& state
){
    state = RunState{};
    if (!game.reload){
        return StoryFailure{.code = "reload_failed", .detail = "no reload hook", .repeats = true};
    }
    StoryCheckpoint marker;
    marker.id = std::string(id);
    const std::optional<std::string> landed = game.reload(backend, marker);
    if (!landed || *landed != id){
        return StoryFailure{
            .code = "reload_failed",
            .detail = std::format("reloaded to {} instead of {}", landed ? *landed : "none", id),
            .repeats = true,
        };
    }
    return std::nullopt;
}

//  Runs one try of `checkpoint` through StoryEngine::run_checkpoints() using
//  a no-save, no-retry copy of `game`.
std::optional<CheckpointFailure> run_single_try(
    StoryBackend& backend, StoryCheckpoint& checkpoint, RunState& state
){
    StoryGame no_save_game;
    no_save_game.attempts_per_checkpoint = 1;
    std::span<StoryCheckpoint> one(&checkpoint, 1);
    return run_checkpoints(backend, one, no_save_game, state);
}

bool is_fatal_verify_failure(const StoryFailure& failure){
    return failure.repeats
        || failure.code == GAME_NOT_RUNNING
        || failure.code == "precondition_failed"
        || failure.code == "start_failed"
        || failure.code == VERIFY_UNREAD;
}

} // namespace


VerifyModeReport verify_checkpoint(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    const VerifyModePolicy& policy,
    StoryCheckpointFactory make_checkpoint,
    StoryGame& game,
    RunState& state,
    VerifyPassPredicate verify_pass
){
    VerifyModeReport report;
    report.checkpoint = std::string(checkpoint_id);
    if (policy.runs == 0 || policy.passes_needed > policy.runs){
        report.failure = StoryFailure{
            .code = "bad_verify_policy",
            .detail = std::format("{} of {} runs", policy.passes_needed, policy.runs),
            .repeats = true,
        };
        backend.log(std::format("[Verify] {}: {}", checkpoint_id, *report.failure));
        return report;
    }
    backend.log(std::format(
        "[Verify] {}: start, {} runs from one save, {} must pass",
        checkpoint_id, policy.runs, policy.passes_needed
    ));

    for (size_t run = 1; run <= policy.runs; run++){
        if (run > 1){
            if (std::optional<StoryFailure> reload_error = reload_for_verify(backend, checkpoint_id, game, state)){
                backend.log(std::format("[Verify] {}: no reload before run {}: {}", checkpoint_id, run, *reload_error));
                report.failure = std::move(reload_error);
                break;
            }
        }
        report.runs++;
        StoryCheckpoint checkpoint = make_checkpoint();
        std::optional<CheckpointFailure> failed = run_single_try(backend, checkpoint, state);
        if (!failed && verify_pass){
            if (std::optional<StoryFailure> check_fail = verify_pass(backend, checkpoint, state)){
                failed = CheckpointFailure{
                    .checkpoint_id = std::string(checkpoint_id),
                    .failure = std::move(*check_fail),
                };
            }
        }
        const bool save_at_end = policy.save_mode == StorySaveMode::AFTER_VERIFICATION
            && run == policy.runs
            && report.passes + 1 == run;
        if (!failed && save_at_end && game.save){
            if (std::optional<StoryFailure> save_fail = game.save(backend, checkpoint)){
                failed = CheckpointFailure{
                    .checkpoint_id = std::string(checkpoint_id),
                    .failure = std::move(*save_fail),
                };
            }
        }
        if (!failed){
            report.passes++;
            report.saved = save_at_end;
            backend.log(std::format("[Verify] {}: run {}/{} passed", checkpoint_id, run, policy.runs));
            continue;
        }
        const std::string step = step_name_at(checkpoint, failed->event_index);
        backend.log(std::format(
            "[Verify] {}: run {}/{} failed{}: {}",
            checkpoint_id, run, policy.runs,
            step.empty() ? std::string() : std::format(" at {}", step),
            failed->failure
        ));
        if (is_fatal_verify_failure(failed->failure)){
            report.failure = std::move(failed->failure);
            break;
        }
    }

    if (policy.save_mode == StorySaveMode::AFTER_VERIFICATION){
        backend.log(std::format(
            "[Verify] {}: {}", checkpoint_id,
            report.saved ? "every run passed: saved at the end state" : "not every run passed: not saving"
        ));
    }
    report.verified = !report.failure && report.passes >= policy.passes_needed;
    backend.log(std::format(
        "[Verify] {}: {}/{} passed, {} needed: {}",
        checkpoint_id, report.passes, policy.runs, policy.passes_needed,
        report.verified ? "verified" : "not verified"
    ));
    return report;
}

namespace{

class VerifyRowRunner{
public:
    VerifyRowRunner(
        StoryBackend& backend,
        std::string_view checkpoint_id,
        const StoryVerify& verify,
        StoryCheckpointFactory make_checkpoint,
        StoryGame& game,
        RunState& state,
        VerifyPassPredicate verify_pass
    )
        : m_backend(backend)
        , m_checkpoint_id(checkpoint_id)
        , m_verify(verify)
        , m_make_checkpoint(std::move(make_checkpoint))
        , m_game(game)
        , m_state(state)
        , m_verify_pass(std::move(verify_pass))
    {
        m_report.checkpoint = std::string(checkpoint_id);
    }

    VerifyRowReport run(){
        log_start();
        if (m_verify.tries > 0 && !run_phase1()){
            return std::move(m_report);
        }
        return run_phase2();
    }

private:
    void log_start(){
        if (m_verify.tries > 0){
            m_backend.log(std::format(
                "[Verify] {}: phase 1, {} tries from one save, {} must pass; phase 2, up to {} tries, the first pass saved",
                m_checkpoint_id, m_verify.tries, m_verify.min_pass, m_verify.reset_budget
            ));
        }else{
            m_backend.log(std::format(
                "[Verify] {}: up to {} tries, the first pass saved",
                m_checkpoint_id, m_verify.reset_budget
            ));
        }
    }

    void stop(std::string code, std::string detail){
        m_backend.log(std::format("[Verify] {}: {}; stopping", m_checkpoint_id, detail));
        m_report.failure = CheckpointFailure{
            .checkpoint_id = std::string(m_checkpoint_id),
            .failure = StoryFailure{.code = std::move(code), .detail = std::move(detail), .repeats = true},
        };
    }

    //  Runs m_verify_pass on a passed checkpoint. Sets m_report.failure and
    //  returns false if VERIFY_UNREAD forces an immediate stop.
    bool evaluate_pass_check(
        const StoryCheckpoint& checkpoint,
        std::optional<CheckpointFailure>& failed
    ){
        if (failed || !m_verify_pass){
            return true;
        }
        std::optional<StoryFailure> check_fail = m_verify_pass(m_backend, checkpoint, m_state);
        if (!check_fail){
            return true;
        }
        if (check_fail->code == VERIFY_UNREAD){
            m_backend.log(std::format(
                "[Verify] {}: the pass check couldn't read the game; no save, stopping", m_checkpoint_id
            ));
            check_fail->repeats = true;
            m_report.failure = CheckpointFailure{
                .checkpoint_id = std::string(m_checkpoint_id),
                .failure = std::move(*check_fail),
            };
            return false;
        }
        failed = CheckpointFailure{
            .checkpoint_id = std::string(m_checkpoint_id),
            .failure = StoryFailure{
                .code = "verify_no_pass",
                .detail = std::format("{}", *check_fail),
            },
        };
        return true;
    }

    //  Returns true if try passed, false if try failed (continue loop), or
    //  nullopt if the loop must stop immediately (m_report.failure is set).
    std::optional<bool> try_once(size_t phase, size_t index, size_t of){
        if (m_report.attempts > 0){
            m_report.reloads++;
            if (std::optional<StoryFailure> reload_error = reload_for_verify(m_backend, m_checkpoint_id, m_game, m_state)){
                m_report.failure = CheckpointFailure{
                    .checkpoint_id = std::string(m_checkpoint_id),
                    .failure = std::move(*reload_error),
                };
                return std::nullopt;
            }
        }
        m_report.attempts++;
        StoryCheckpoint checkpoint = m_make_checkpoint();
        std::optional<CheckpointFailure> failed = run_single_try(m_backend, checkpoint, m_state);
        if (!evaluate_pass_check(checkpoint, failed)){
            return std::nullopt;
        }
        if (!failed && phase == 2 && m_game.save){
            if (std::optional<StoryFailure> save_fail = m_game.save(m_backend, checkpoint)){
                failed = CheckpointFailure{
                    .checkpoint_id = std::string(m_checkpoint_id),
                    .failure = std::move(*save_fail),
                };
            }else{
                m_report.saved = true;
            }
        }
        if (!failed){
            m_backend.log(std::format("[Verify] {}: phase {} try {}/{} passed", m_checkpoint_id, phase, index, of));
            return true;
        }
        const std::string step = step_name_at(checkpoint, failed->event_index);
        m_backend.log(std::format(
            "[Verify] {}: phase {} try {}/{} failed{}: {}",
            m_checkpoint_id, phase, index, of,
            step.empty() ? std::string() : std::format(" at {}", step),
            failed->failure
        ));
        if (is_fatal_verify_failure(failed->failure)){
            m_backend.log(std::format("[Verify] {}: stopping", m_checkpoint_id));
            m_report.failure = std::move(failed);
            return std::nullopt;
        }
        return false;
    }

    bool run_phase1(){
        for (size_t c = 1; c <= m_verify.tries && m_report.phase1_passes < m_verify.min_pass; c++){
            const std::optional<bool> ok = try_once(/*phase=*/1, c, m_verify.tries);
            if (!ok){
                return false;
            }
            m_report.phase1_passes += *ok ? 1 : 0;
            if (m_report.phase1_passes + (m_verify.tries - c) < m_verify.min_pass){
                stop("verify_below_min_pass", std::format(
                    "phase 1 passed {} of {}, {} left: under {}",
                    m_report.phase1_passes, c, m_verify.tries - c, m_verify.min_pass
                ));
                return false;
            }
        }
        if (m_report.phase1_passes < m_verify.min_pass){
            stop("verify_below_min_pass", std::format(
                "phase 1 passed {} of {}, under {}", m_report.phase1_passes, m_verify.tries, m_verify.min_pass
            ));
            return false;
        }
        m_backend.log(std::format("[Verify] {}: phase 1 reached {} passes", m_checkpoint_id, m_report.phase1_passes));
        return true;
    }

    VerifyRowReport run_phase2(){
        for (size_t c = 1; c <= m_verify.reset_budget; c++){
            const std::optional<bool> ok = try_once(/*phase=*/2, c, m_verify.reset_budget);
            if (!ok){
                return std::move(m_report);
            }
            if (*ok){
                m_report.passed = true;
                m_backend.log(std::format("[Verify] {}: phase 2 try {} passed and saved", m_checkpoint_id, c));
                return std::move(m_report);
            }
        }
        stop("verify_out_of_budget", std::format("no pass in {} phase 2 tries", m_verify.reset_budget));
        return std::move(m_report);
    }

    StoryBackend& m_backend;
    std::string_view m_checkpoint_id;
    const StoryVerify& m_verify;
    StoryCheckpointFactory m_make_checkpoint;
    StoryGame& m_game;
    RunState& m_state;
    VerifyPassPredicate m_verify_pass;
    VerifyRowReport m_report;
};

} // namespace


VerifyRowReport run_verify_row(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    const StoryVerify& verify,
    StoryCheckpointFactory make_checkpoint,
    StoryGame& game,
    RunState& state,
    VerifyPassPredicate verify_pass
){
    return VerifyRowRunner(
        backend, checkpoint_id, verify, std::move(make_checkpoint), game, state, std::move(verify_pass)
    ).run();
}



} // namespace Pokemon
} // namespace PokemonAutomation

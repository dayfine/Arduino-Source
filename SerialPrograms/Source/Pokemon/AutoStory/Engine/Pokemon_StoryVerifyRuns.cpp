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

struct VerifyModeContext{
    StoryBackend& backend;
    std::string_view checkpoint_id;
    const VerifyModePolicy& policy;
    StoryCheckpointFactory& make_checkpoint;
    StoryGame& game;
    RunState& state;
    VerifyPassPredicate& verify_pass;
    VerifyModeReport& report;
};

std::optional<CheckpointFailure> run_verify_mode_pass(
    VerifyModeContext& ctx,
    StoryCheckpoint& checkpoint,
    bool save_at_end
){
    std::optional<CheckpointFailure> failed = run_single_try(ctx.backend, checkpoint, ctx.state);
    if (!failed && ctx.verify_pass){
        if (std::optional<StoryFailure> check_fail = ctx.verify_pass(ctx.backend, checkpoint, ctx.state)){
            failed = CheckpointFailure{
                .checkpoint_id = std::string(ctx.checkpoint_id),
                .failure = std::move(*check_fail),
            };
        }
    }
    if (!failed && save_at_end && ctx.game.save){
        if (std::optional<StoryFailure> save_fail = ctx.game.save(ctx.backend, checkpoint)){
            failed = CheckpointFailure{
                .checkpoint_id = std::string(ctx.checkpoint_id),
                .failure = std::move(*save_fail),
            };
        }
    }
    return failed;
}

void log_verify_failure(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    std::string_view prefix,
    const StoryCheckpoint& checkpoint,
    const CheckpointFailure& failed
){
    const std::string step = step_name_at(checkpoint, failed.event_index);
    backend.log(std::format(
        "[Verify] {}: {} failed{}: {}",
        checkpoint_id, prefix,
        step.empty() ? std::string() : std::format(" at {}", step),
        failed.failure
    ));
}

struct VerifyRowContext{
    StoryBackend& backend;
    std::string_view checkpoint_id;
    StoryCheckpointFactory& make_checkpoint;
    StoryGame& game;
    RunState& state;
    VerifyPassPredicate& verify_pass;
    VerifyRowReport& report;
};

void stop_verify_row(
    VerifyRowContext& ctx, std::string code, std::string detail
){
    ctx.backend.log(std::format("[Verify] {}: {}; stopping", ctx.checkpoint_id, detail));
    ctx.report.failure = CheckpointFailure{
        .checkpoint_id = std::string(ctx.checkpoint_id),
        .failure = StoryFailure{.code = std::move(code), .detail = std::move(detail), .repeats = true},
    };
}

std::optional<CheckpointFailure> check_verify_row_pass(
    VerifyRowContext& ctx,
    const StoryCheckpoint& checkpoint,
    bool& unread_stop
){
    if (!ctx.verify_pass){
        return std::nullopt;
    }
    std::optional<StoryFailure> check_fail = ctx.verify_pass(ctx.backend, checkpoint, ctx.state);
    if (!check_fail){
        return std::nullopt;
    }
    if (check_fail->code == VERIFY_UNREAD){
        ctx.backend.log(std::format(
            "[Verify] {}: the pass check couldn't read the game; no save, stopping", ctx.checkpoint_id
        ));
        check_fail->repeats = true;
        unread_stop = true;
        return CheckpointFailure{
            .checkpoint_id = std::string(ctx.checkpoint_id),
            .failure = std::move(*check_fail),
        };
    }
    return CheckpointFailure{
        .checkpoint_id = std::string(ctx.checkpoint_id),
        .failure = StoryFailure{
            .code = "verify_no_pass",
            .detail = std::format("{}", *check_fail),
        },
    };
}

//  Returns true if try passed, false if try failed (continue loop), or
//  nullopt if the loop must stop immediately (ctx.report.failure is set).
std::optional<bool> try_verify_row_once(
    VerifyRowContext& ctx, size_t phase, size_t index, size_t of
){
    if (ctx.report.attempts > 0){
        ctx.report.reloads++;
        if (std::optional<StoryFailure> reload_error = reload_for_verify(ctx.backend, ctx.checkpoint_id, ctx.game, ctx.state)){
            ctx.report.failure = CheckpointFailure{
                .checkpoint_id = std::string(ctx.checkpoint_id),
                .failure = std::move(*reload_error),
            };
            return std::nullopt;
        }
    }
    ctx.report.attempts++;
    StoryCheckpoint checkpoint = ctx.make_checkpoint();
    std::optional<CheckpointFailure> failed = run_single_try(ctx.backend, checkpoint, ctx.state);
    if (!failed){
        bool unread_stop = false;
        failed = check_verify_row_pass(ctx, checkpoint, unread_stop);
        if (unread_stop){
            ctx.report.failure = std::move(failed);
            return std::nullopt;
        }
    }
    if (!failed && phase == 2 && ctx.game.save){
        if (std::optional<StoryFailure> save_fail = ctx.game.save(ctx.backend, checkpoint)){
            failed = CheckpointFailure{
                .checkpoint_id = std::string(ctx.checkpoint_id),
                .failure = std::move(*save_fail),
            };
        }else{
            ctx.report.saved = true;
        }
    }
    if (!failed){
        ctx.backend.log(std::format("[Verify] {}: phase {} try {}/{} passed", ctx.checkpoint_id, phase, index, of));
        return true;
    }
    log_verify_failure(ctx.backend, ctx.checkpoint_id, std::format("phase {} try {}/{}", phase, index, of), checkpoint, *failed);
    if (is_fatal_verify_failure(failed->failure)){
        ctx.backend.log(std::format("[Verify] {}: stopping", ctx.checkpoint_id));
        ctx.report.failure = std::move(failed);
        return std::nullopt;
    }
    return false;
}

bool run_verify_row_phase1(VerifyRowContext& ctx, const StoryVerify& verify){
    for (size_t c = 1; c <= verify.tries && ctx.report.phase1_passes < verify.min_pass; c++){
        const std::optional<bool> ok = try_verify_row_once(ctx, /*phase=*/1, c, verify.tries);
        if (!ok){
            return false;
        }
        ctx.report.phase1_passes += *ok ? 1 : 0;
        if (ctx.report.phase1_passes + (verify.tries - c) < verify.min_pass){
            stop_verify_row(ctx, "verify_below_min_pass", std::format(
                "phase 1 passed {} of {}, {} left: under {}",
                ctx.report.phase1_passes, c, verify.tries - c, verify.min_pass
            ));
            return false;
        }
    }
    if (ctx.report.phase1_passes < verify.min_pass){
        stop_verify_row(ctx, "verify_below_min_pass", std::format(
            "phase 1 passed {} of {}, under {}", ctx.report.phase1_passes, verify.tries, verify.min_pass
        ));
        return false;
    }
    ctx.backend.log(std::format("[Verify] {}: phase 1 reached {} passes", ctx.checkpoint_id, ctx.report.phase1_passes));
    return true;
}

bool run_verify_mode_iteration(VerifyModeContext& ctx, size_t run){
    if (run > 1){
        if (std::optional<StoryFailure> reload_error = reload_for_verify(ctx.backend, ctx.checkpoint_id, ctx.game, ctx.state)){
            ctx.backend.log(std::format("[Verify] {}: no reload before run {}: {}", ctx.checkpoint_id, run, *reload_error));
            ctx.report.failure = std::move(reload_error);
            return false;
        }
    }
    ctx.report.runs++;
    StoryCheckpoint checkpoint = ctx.make_checkpoint();
    const bool save_at_end = ctx.policy.save_mode == StorySaveMode::AFTER_VERIFICATION
        && run == ctx.policy.runs
        && ctx.report.passes + 1 == run;
    std::optional<CheckpointFailure> failed = run_verify_mode_pass(ctx, checkpoint, save_at_end);
    if (!failed){
        ctx.report.passes++;
        ctx.report.saved = save_at_end;
        ctx.backend.log(std::format("[Verify] {}: run {}/{} passed", ctx.checkpoint_id, run, ctx.policy.runs));
        return true;
    }
    log_verify_failure(ctx.backend, ctx.checkpoint_id, std::format("run {}/{}", run, ctx.policy.runs), checkpoint, *failed);
    if (is_fatal_verify_failure(failed->failure)){
        ctx.report.failure = std::move(failed->failure);
        return false;
    }
    return true;
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

    VerifyModeContext ctx{backend, checkpoint_id, policy, make_checkpoint, game, state, verify_pass, report};
    for (size_t run = 1; run <= policy.runs; run++){
        if (!run_verify_mode_iteration(ctx, run)){
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

VerifyRowReport run_verify_row(
    StoryBackend& backend,
    std::string_view checkpoint_id,
    const StoryVerify& verify,
    StoryCheckpointFactory make_checkpoint,
    StoryGame& game,
    RunState& state,
    VerifyPassPredicate verify_pass
){
    VerifyRowReport report;
    report.checkpoint = std::string(checkpoint_id);
    if (verify.tries == 0){
        backend.log(std::format(
            "[Verify] {}: phase 1 skipped; phase 2, up to {} tries, the first pass saved",
            checkpoint_id, verify.reset_budget
        ));
    }else{
        backend.log(std::format(
            "[Verify] {}: phase 1, {} tries from one save, {} must pass; phase 2, up to {} tries, the first pass saved",
            checkpoint_id, verify.tries, verify.min_pass, verify.reset_budget
        ));
    }
    VerifyRowContext ctx{backend, checkpoint_id, make_checkpoint, game, state, verify_pass, report};
    if (verify.tries > 0 && !run_verify_row_phase1(ctx, verify)){
        return report;
    }
    for (size_t c = 1; c <= verify.reset_budget; c++){
        const std::optional<bool> ok = try_verify_row_once(ctx, /*phase=*/2, c, verify.reset_budget);
        if (!ok){
            return report;
        }
        if (*ok){
            report.passed = true;
            backend.log(std::format("[Verify] {}: phase 2 try {} passed and saved", checkpoint_id, c));
            return report;
        }
    }
    stop_verify_row(ctx, "verify_out_of_budget", std::format("no pass in {} phase 2 tries", verify.reset_budget));
    return report;
}



} // namespace Pokemon
} // namespace PokemonAutomation

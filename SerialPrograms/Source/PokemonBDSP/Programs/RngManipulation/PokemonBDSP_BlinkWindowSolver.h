/*  BDSP Blink Window Solver
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_BlinkWindowSolver_H
#define PokemonAutomation_PokemonBDSP_BlinkWindowSolver_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/Pokemon_Xorshift128.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_BlinkExtraction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


struct BlinkWindowSolverConfig{
    size_t min_rolls_to_try = 44;
    size_t confirmation_events = 6;
    size_t window_events = 52;
    size_t max_failed_windows = 3;
    size_t max_confirmation_failures = 3; // 0 disables dropping stuck provisional candidates
};

enum class BlinkWindowStatus{
    Waiting,
    SolveFailed,
    ProvisionalFound,
    ProvisionalChanged,
    ProvisionalDropped,
    Confirmed,
    GaveUp,
};

struct BlinkWindowStep{
    BlinkWindowStatus status = BlinkWindowStatus::Waiting;
    size_t events = 0;
    Pokemon::Xorshift128State state;
    std::vector<uint8_t> slots;
    double tick_seconds = 0;
};


//  Pure sliding-window solve and provisional-state confirmation state machine.
class BlinkWindowSolver{
public:
    explicit BlinkWindowSolver(const BlinkWindowSolverConfig& config);

    //  False while confirming a provisional candidate so its earliest blink stays pinned.
    bool should_trim_window() const;

    BlinkWindowStep step(
        const std::vector<std::vector<BlinkEvent>>& streams,
        WallClock origin
    );

private:
    bool has_fresh_blink(
        const std::vector<std::vector<BlinkEvent>>& streams,
        WallClock origin
    );
    BlinkWindowStep handle_solve_failure(size_t events);
    BlinkWindowStep handle_solve_success(
        const Pokemon::Xorshift128State& state,
        std::vector<uint8_t> winning_slots,
        double tick_seconds,
        size_t events
    );

private:
    BlinkWindowSolverConfig m_config;
    size_t m_give_up_after = 0;
    size_t m_failed_solves = 0;
    size_t m_confirmation_failures = 0;
    bool m_has_candidate = false;
    Pokemon::Xorshift128State m_candidate;
    size_t m_candidate_events = 0;
    std::vector<WallClock> m_newest_blink;
};


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif

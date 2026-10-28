/*  Max Lair Path Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Decides whether the host should reset to keep its current path.
 *
 *  The default rule keeps the path while its observed win ratio is at least
 *  the target. This throws away good paths on a short losing streak: a path
 *  at 14/19 (0.737) is far better than a new path that wins 5 of 12 first
 *  runs, but it is below 0.75.
 *
 *  The "likely" rule models the path's win rate with a Beta posterior and
 *  keeps the path while either:
 *    - its win rate could still plausibly reach the target, or
 *    - it is almost certainly better than a new path.
 *
 *  The first condition still drops a new path after an early loss, which is
 *  what makes searching for a good path fast. The second keeps a path that
 *  is good but below the target when good paths are rare.
 *
 */

#ifndef PokemonAutomation_PokemonSwSh_MaxLair_PathPolicy_H
#define PokemonAutomation_PokemonSwSh_MaxLair_PathPolicy_H

#include <stdint.h>
#include <string>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{


struct WinRecord{
    uint64_t wins = 0;
    uint64_t runs = 0;

    void add_run(bool win){
        runs++;
        if (win){
            wins++;
        }
    }
};


enum class PathKeepingRule{
    //  Keep while wins / runs >= target.
    OBSERVED_WIN_RATE,
    //  Keep while the win rate is likely enough to reach the target, or
    //  almost certainly beats a new path.
    LIKELY_WIN_RATE,
};


struct PathPolicySettings{
    PathKeepingRule rule = PathKeepingRule::OBSERVED_WIN_RATE;

    //  The win rate we want a path to have.
    double target_win_rate = 0.75;

    //  The settings below are for LIKELY_WIN_RATE.

    //  Keep the path while P(path rate >= target) is at least this.
    double min_target_probability = 0.2;

    //  Also keep the path while P(path rate > new-path rate) is at least this.
    double min_better_than_new_probability = 0.95;

    //  Prior for the path's win rate, as pseudo-wins and pseudo-losses.
    //  (1, 1) is uniform.
    double path_prior_wins = 1;
    double path_prior_losses = 1;

    //  Prior for the win rate of a new path. Keeps the baseline sane before
    //  any new path has been played.
    double new_path_prior_wins = 2;
    double new_path_prior_losses = 2;
};


struct PathPolicyDecision{
    bool keep;

    //  P(path rate >= target)
    double target_probability;

    //  P(path rate > new-path rate)
    double better_than_new_probability;

    //  One line for the log.
    std::string reason;
};


//  P(X >= x) for X ~ Beta(a, b).
double probability_beta_at_least(double a, double b, double x);

//  P(X > Y) for independent X ~ Beta(a1, b1) and Y ~ Beta(a2, b2).
double probability_beta_greater(double a1, double b1, double a2, double b2);


//  "path" is the current path's record. "new_paths" counts the first run on
//  each new path this session. Count only first runs: later runs happen only
//  on paths that were kept, which would bias the baseline up.
PathPolicyDecision decide_keep_path(
    const PathPolicySettings& settings,
    const WinRecord& path,
    const WinRecord& new_paths
);



}
}
}
}
#endif

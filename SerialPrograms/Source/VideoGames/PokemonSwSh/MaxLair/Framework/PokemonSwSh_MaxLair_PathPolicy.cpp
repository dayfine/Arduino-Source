/*  Max Lair Path Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "PokemonSwSh_MaxLair_PathPolicy.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{



namespace{

//  Integrate on a grid instead of with the incomplete beta function. With 512
//  cells the error is about 1e-4 for parameters >= 1, which is far finer than
//  the thresholds the policy uses.
const size_t BETA_CELLS = 512;

//  Beta density at the midpoint of each cell of [0, 1], normalized to sum to 1.
//  Normalizing numerically avoids lgamma() and keeps the result a proper
//  distribution near the edges.
std::vector<double> beta_cell_masses(double a, double b){
    std::vector<double> ret(BETA_CELLS);
    double total = 0;
    for (size_t c = 0; c < BETA_CELLS; c++){
        double x = (c + 0.5) / BETA_CELLS;
        double mass = std::exp((a - 1) * std::log(x) + (b - 1) * std::log1p(-x));
        ret[c] = mass;
        total += mass;
    }
    for (double& mass : ret){
        mass /= total;
    }
    return ret;
}

}


double probability_beta_at_least(double a, double b, double x){
    std::vector<double> masses = beta_cell_masses(a, b);
    double ret = 0;
    for (size_t c = 0; c < BETA_CELLS; c++){
        double lo = (double)c / BETA_CELLS;
        double hi = (double)(c + 1) / BETA_CELLS;
        if (lo >= x){
            ret += masses[c];
        }else if (hi > x){
            //  Split the cell that contains x.
            ret += masses[c] * (hi - x) * BETA_CELLS;
        }
    }
    return ret;
}

double probability_beta_greater(double a1, double b1, double a2, double b2){
    std::vector<double> x = beta_cell_masses(a1, b1);
    std::vector<double> y = beta_cell_masses(a2, b2);

    //  Sum over cells: P(X in cell) * P(Y < X), counting a tie in the same
    //  cell as half.
    double ret = 0;
    double y_below = 0;
    for (size_t c = 0; c < BETA_CELLS; c++){
        ret += x[c] * (y_below + 0.5 * y[c]);
        y_below += y[c];
    }
    return ret;
}


namespace{

std::string compare_str(double value, double threshold){
    return tostr_fixed(value, 3) + (value >= threshold ? " >= " : " < ") + tostr_fixed(threshold, 3);
}

}


PathPolicyDecision decide_keep_path(
    const PathPolicySettings& settings,
    const WinRecord& path,
    const WinRecord& new_paths
){
    double path_a = settings.path_prior_wins + (double)path.wins;
    double path_b = settings.path_prior_losses + (double)(path.runs - path.wins);
    double new_a = settings.new_path_prior_wins + (double)new_paths.wins;
    double new_b = settings.new_path_prior_losses + (double)(new_paths.runs - new_paths.wins);
    double win_ratio = path.runs == 0 ? 0 : (double)path.wins / (double)path.runs;

    //  Both probabilities are logged under either rule, so a user can see
    //  what the other rule would have done.
    PathPolicyDecision ret;
    ret.target_probability = probability_beta_at_least(path_a, path_b, settings.target_win_rate);
    ret.better_than_new_probability = probability_beta_greater(path_a, path_b, new_a, new_b);

    switch (settings.rule){
    case PathKeepingRule::OBSERVED_WIN_RATE:
        ret.keep = win_ratio >= settings.target_win_rate;
        break;
    case PathKeepingRule::LIKELY_WIN_RATE:
        ret.keep = ret.target_probability >= settings.min_target_probability
            || ret.better_than_new_probability >= settings.min_better_than_new_probability;
        break;
    }

    ret.reason =
        "Path: " + std::to_string(path.wins) + "/" + std::to_string(path.runs) +
        ", Win Ratio = " + compare_str(win_ratio, settings.target_win_rate) +
        ", New Paths: " + std::to_string(new_paths.wins) + "/" + std::to_string(new_paths.runs) +
        ", P(rate >= " + tostr_fixed(settings.target_win_rate, 2) + ") = " +
        compare_str(ret.target_probability, settings.min_target_probability) +
        ", P(better than new) = " +
        compare_str(ret.better_than_new_probability, settings.min_better_than_new_probability);
    return ret;
}



}
}
}
}

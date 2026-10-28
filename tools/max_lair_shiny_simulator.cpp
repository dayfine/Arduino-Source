// Offline sensitivity analysis of time to the first shiny; no app or Switch access.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include "max_lair_simulation_random.h"

struct Population{
    double ordinary_min;
    double ordinary_max;
    double excellent_rate;
    double excellent_probability;
};

struct Start{
    std::string name;
    double true_rate;
    uint64_t wins;
    uint64_t attempts;
};

struct Costs{
    std::string name;
    double attempt_seconds = 720;
    double reset_seconds = 7.7440405;
    int ore = -1;
};

struct Hunt{
    double hours = 0;
    uint64_t attempts = 0;
    uint64_t wins = 0;
    uint64_t resets = 0;
    uint64_t routes = 0;
    uint64_t excellent_discarded = 0;
    bool ever_excellent = false;
};

struct State{
    uint64_t route = 0;
    uint64_t wins = 0;
    uint64_t attempts = 0;
    double rate = -1;
    double seconds = 0;
    int ore = -1;
};

double draw_route(const Population& population, uint64_t seed, uint64_t route){
    if (uniform(seed, route, 0, 1) < population.excellent_probability){
        return population.excellent_rate;
    }
    return population.ordinary_min + uniform(seed, route, 0, 8)
        * (population.ordinary_max - population.ordinary_min);
}

void next_route(State& state){
    state.route++;
    state.wins = 0;
    state.attempts = 0;
    state.rate = -1;
}

void decide(State& state, Hunt& result, const Costs& costs, double target){
    bool keep = state.wins >= target * state.attempts;
    if (keep && state.ore >= 0 && state.ore < 10) keep = false;
    if (keep){
        result.resets++;
        state.seconds += costs.reset_seconds;
        if (state.ore >= 0) state.ore -= 10;
    }else{
        result.excellent_discarded += state.rate >= .85;
        next_route(state);
    }
}

Hunt simulate(const Population& population, const Start& start, const Costs& costs,
    double target, uint64_t wanted_wins, uint64_t seed)
{
    Hunt result;
    State state{.wins = start.wins, .attempts = start.attempts,
        .rate = start.true_rate, .ore = costs.ore};
    if (state.attempts){
        decide(state, result, costs, target);
    }
    while (result.wins < wanted_wins){
        if (result.attempts >= 1000000) throw std::runtime_error("Hunt guard reached; no result was censored.");
        if (state.rate < 0){
            state.rate = draw_route(population, seed, state.route);
            result.routes++;
            result.ever_excellent = result.ever_excellent || state.rate >= .85;
        }
        bool win = uniform(seed, state.route, state.attempts, 2) < state.rate;
        state.seconds += costs.attempt_seconds;
        result.attempts++;
        result.wins += win;
        state.attempts++;
        state.wins += win;
        if (result.wins == wanted_wins) break;
        decide(state, result, costs, target);
    }
    result.hours = state.seconds / 3600;
    return result;
}

uint64_t shiny_target(uint64_t seed, double shiny_probability){
    double draw = uniform(seed, 0, 0, 4);
    return (uint64_t)(std::log1p(-draw) / std::log1p(-shiny_probability)) + 1;
}

struct Batch{
    std::vector<double> hours;
    double mean_hours = 0;
    double mean_attempts = 0;
    double mean_wins = 0;
    double mean_resets = 0;
    double mean_routes = 0;
    double probability_excellent = 0;
    double excellent_discarded = 0;
    double within8 = 0;
    double within24 = 0;
    double within48 = 0;
    double standard_error = 0;
};

Batch evaluate(const Population& population, const Start& start, const Costs& costs,
    double target, const std::vector<uint64_t>& wanted)
{
    Batch result;
    double squared = 0;
    for (size_t c = 0; c < wanted.size(); c++){
        Hunt hunt = simulate(population, start, costs, target, wanted[c], 23000000 + c);
        result.hours.push_back(hunt.hours);
        result.mean_hours += hunt.hours;
        result.mean_attempts += hunt.attempts;
        result.mean_wins += hunt.wins;
        result.mean_resets += hunt.resets;
        result.mean_routes += hunt.routes;
        result.probability_excellent += hunt.ever_excellent;
        result.excellent_discarded += hunt.excellent_discarded;
        result.within8 += hunt.hours <= 8;
        result.within24 += hunt.hours <= 24;
        result.within48 += hunt.hours <= 48;
        squared += hunt.hours * hunt.hours;
    }
    double n = (double)wanted.size();
    result.mean_hours /= n;
    result.mean_attempts /= n;
    result.mean_wins /= n;
    result.mean_resets /= n;
    result.mean_routes /= n;
    result.probability_excellent /= n;
    result.excellent_discarded /= n;
    result.within8 /= n;
    result.within24 /= n;
    result.within48 /= n;
    result.standard_error = std::sqrt(std::max(0.0, (squared - n * result.mean_hours * result.mean_hours) / (n - 1)) / n);
    return result;
}

std::pair<double, double> paired_difference(const Batch& a, const Batch& b){
    double total = 0, squared = 0;
    for (size_t c = 0; c < a.hours.size(); c++){
        double delta = a.hours[c] - b.hours[c];
        total += delta;
        squared += delta * delta;
    }
    double n = (double)a.hours.size();
    double mean = total / n;
    return {mean, std::sqrt(std::max(0.0, (squared - n * mean * mean) / (n - 1)) / n)};
}

std::vector<double> sorted_hours(const Batch& batch){
    auto result = batch.hours;
    std::sort(result.begin(), result.end());
    return result;
}

void write_batch(std::ostream& output, const Population& population, const Start& start,
    const Costs& costs, double target, const Batch& batch)
{
    auto sorted = sorted_hours(batch);
    output << population.ordinary_min << ',' << population.ordinary_max << ',' << population.excellent_rate << ','
        << population.excellent_probability << ',' << start.name << ',' << costs.name << ',' << target << ','
        << batch.mean_hours << ',' << sorted[sorted.size() / 2] << ',' << sorted[(size_t)(.9 * sorted.size())] << ','
        << batch.standard_error << ',' << batch.mean_attempts << ',' << batch.mean_wins << ',' << batch.mean_resets << ','
        << batch.mean_routes << ',' << batch.probability_excellent << ',' << batch.excellent_discarded << ','
        << batch.within8 << ',' << batch.within24 << ',' << batch.within48 << '\n';
}

void study_case(const Population& population, const Start& start, const Costs& costs,
    const std::vector<uint64_t>& wanted, std::ostream& output, std::ostream& comparisons)
{
    std::vector<Batch> results;
    const std::array<double, 5> targets{0, .70, .75, .80, .85};
    for (double target : targets){
        results.push_back(evaluate(population, start, costs, target, wanted));
        write_batch(output, population, start, costs, target, results.back());
    }
    auto [delta, se] = paired_difference(results[4], results[1]);
    comparisons << population.ordinary_min << ',' << population.ordinary_max << ',' << population.excellent_rate << ','
        << population.excellent_probability << ',' << start.name << ',' << costs.name << ','
        << results[1].mean_hours << ',' << results[4].mean_hours << ',' << delta << ',' << se << '\n';
}

void run_checks(){
    const Population certain{.ordinary_min = 1, .ordinary_max = 1,
        .excellent_rate = 1, .excellent_probability = 1};
    const Start fresh{.name = "fresh", .true_rate = -1, .wins = 0, .attempts = 0};
    const Costs costs{.name = "test", .attempt_seconds = 600, .reset_seconds = 60};
    Hunt first = simulate(certain, fresh, costs, .70, 1, 1);
    if (first.wins != 1 || first.attempts != 1 || first.resets != 0 || std::abs(first.hours - 1.0 / 6) > 1e-12){
        throw std::runtime_error("First shiny must stop before resetting.");
    }
    Hunt third = simulate(certain, fresh, costs, .85, 3, 1);
    if (third.attempts != 3 || third.resets != 2 || std::abs(third.hours - 1920.0 / 3600) > 1e-12){
        throw std::runtime_error("Three certain wins must take three attempts and two resets.");
    }
    const Costs no_ore{.name = "no-ore", .attempt_seconds = 600, .reset_seconds = 60, .ore = 0};
    Hunt forced = simulate(certain, fresh, no_ore, .70, 3, 1);
    if (forced.routes != 3 || forced.resets != 0 || forced.attempts != 3) throw std::runtime_error("Ore exhaustion must force new routes.");
    const Start previous{.name = "previous", .true_rate = 1, .wins = 10, .attempts = 14};
    Hunt retained = simulate(certain, previous, costs, .70, 1, 1);
    Hunt abandoned = simulate(certain, previous, costs, .85, 1, 1);
    if (retained.routes != 0 || retained.resets != 1 || abandoned.routes != 1 || abandoned.resets != 0){
        throw std::runtime_error("Initial 10/14 decision must keep at 70% and abandon at 85%.");
    }
    std::vector<double> wins;
    for (uint64_t seed = 0; seed < 100000; seed++) wins.push_back((double)shiny_target(seed, .01));
    double mean = std::accumulate(wins.begin(), wins.end(), 0.0) / wins.size();
    if (std::abs(mean - 100) > 1.5) throw std::runtime_error("Geometric mean differs from 100 by more than sampling tolerance.");
    std::cout << "PASS shiny stop; reset timing; ore exhaustion; existing-route decision; geometric sampling (mean " << mean << ")\n";
}

int main(int argc, char** argv){
    try{
        if (argc == 2 && std::string(argv[1]) == "--selftest"){
            run_checks();
            return 0;
        }
        if (argc != 4 && argc != 5) throw std::runtime_error("Usage: simulator results.csv comparisons.csv samples [refine]");
        std::vector<uint64_t> wanted;
        for (size_t c = 0; c < std::stoul(argv[3]); c++) wanted.push_back(shiny_target(23000000 + c, .01));
        std::ofstream output(argv[1]), comparisons(argv[2]);
        output << std::setprecision(10);
        comparisons << std::setprecision(10);
        output << "ordinary_min,ordinary_max,excellent_rate,excellent_probability,start,costs,target,mean_hours,median_hours,p90_hours,se_hours,mean_attempts,mean_wins,mean_resets,mean_routes,probability_ever_drew_excellent,mean_excellent_discarded,shiny_within8,shiny_within24,shiny_within48\n";
        comparisons << "ordinary_min,ordinary_max,excellent_rate,excellent_probability,start,costs,hours70,hours85,delta85minus70_hours,paired_se_hours\n";
        const std::array<Start, 4> starts{{
            {.name = "fresh", .true_rate = -1, .wins = 0, .attempts = 0},
            {.name = "previous10of14-true60", .true_rate = .60, .wins = 10, .attempts = 14},
            {.name = "previous10of14-true70", .true_rate = .70, .wins = 10, .attempts = 14},
            {.name = "previous10of14-true80", .true_rate = .80, .wins = 10, .attempts = 14},
        }};
        const Costs base{.name = "12min-plus-measured-reset"};
        if (argc == 5){
            if (std::string(argv[4]) != "refine") throw std::runtime_error("Unknown study mode.");
            for (double excellent : {.85, .95}){
                for (double q : {.0125, .015, .0175, .25, .30, .35, .40}){
                    const Population population{.ordinary_min = .4, .ordinary_max = .8,
                        .excellent_rate = excellent, .excellent_probability = q};
                    study_case(population, starts[2], base, wanted, output, comparisons);
                }
            }
            return 0;
        }
        for (double low : {.2, .4, .6}){
            for (double excellent : {.85, .95}){
                for (double q : {0.0, .001, .005, .01, .02, .05, .10, .20, .50, 1.0}){
                    Population population{.ordinary_min = low, .ordinary_max = .8,
                        .excellent_rate = excellent, .excellent_probability = q};
                    for (const auto& start : starts) study_case(population, start, base, wanted, output, comparisons);
                    if (low == .4 && excellent == .85){
                        for (const Costs& costs : std::array<Costs, 2>{{
                            {.name = "60s-extra-reset", .reset_seconds = 60},
                            {.name = "999-ore-no-replenishment", .ore = 999},
                        }}){
                            study_case(population, starts[0], costs, wanted, output, comparisons);
                            study_case(population, starts[2], costs, wanted, output, comparisons);
                        }
                    }
                    std::cerr << "ordinary=" << low << "..0.8 excellent=" << excellent << " q=" << q << '\n';
                }
            }
        }
    }catch (const std::exception& error){
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

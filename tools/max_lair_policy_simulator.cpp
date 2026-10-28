// Offline Monte Carlo analysis. Links the real PR #55 policy; no Switch inputs.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "VideoGames/PokemonSwSh/MaxLair/Framework/PokemonSwSh_MaxLair_PathPolicy.h"
#include "max_lair_simulation_random.h"

using namespace PokemonAutomation::NintendoSwitch::PokemonSwSh::MaxLairInternal;

struct Model{
    std::string name;
    double mean;
    double low;
    double high;
    double high_probability;
    double reset_seconds;
    std::vector<double> wins;
    std::vector<double> losses;
};

struct Policy{
    std::string name;
    PathKeepingRule rule;
    double target;
    bool standard = false;
};

struct Variant{
    std::string name;
    double reset_seconds = -1;
    bool slower_good_routes = false;
    int ore = -1;
};

struct Outcome{
    double wins = 0;
    double runs = 0;
    double resets = 0;
    double new_routes = 0;
    double forced_continues = 0;
};

struct Evaluation{
    std::vector<double> wins;
    Outcome mean;
    double standard_error = 0;
};

std::vector<std::string> split(const std::string& line, char delimiter){
    std::vector<std::string> result;
    std::istringstream input(line);
    std::string part;
    while (std::getline(input, part, delimiter)) result.push_back(part);
    return result;
}

std::vector<double> doubles(const std::string& values){
    std::vector<double> result;
    for (const auto& value : split(values, ',')) result.push_back(std::stod(value));
    return result;
}

std::vector<Model> read_models(const std::string& filename){
    std::ifstream input(filename);
    if (!input) throw std::runtime_error("Cannot open models: " + filename);
    std::string line;
    std::getline(input, line);
    std::vector<Model> models;
    while (std::getline(input, line)){
        auto fields = split(line, '\t');
        if (fields.size() != 9) throw std::runtime_error("Expected nine model fields.");
        models.push_back(Model{
            .name = fields[0], .mean = std::stod(fields[1]), .low = std::stod(fields[2]),
            .high = std::stod(fields[3]), .high_probability = std::stod(fields[4]),
            .reset_seconds = std::stod(fields[5]), .wins = doubles(fields[6]), .losses = doubles(fields[7]),
        });
    }
    return models;
}

class Decisions{
public:
    bool keep(size_t index, const Policy& policy, const WinRecord& path, const WinRecord& fresh){
        if (policy.standard) return false;
        if (policy.rule == PathKeepingRule::OBSERVED_WIN_RATE){
            return (double)path.wins / path.runs >= policy.target;
        }
        if (std::max(path.runs, fresh.runs) >= 1024){
            throw std::runtime_error("Decision-cache packing limit exceeded.");
        }
        uint64_t key = path.wins | (path.runs << 10) | (fresh.wins << 20)
            | (fresh.runs << 30) | ((uint64_t)index << 40);
        auto found = m_cache.find(key);
        if (found != m_cache.end()) return found->second;
        PathPolicySettings settings;
        settings.rule = policy.rule;
        settings.target_win_rate = policy.target;
        bool result = decide_keep_path(settings, path, fresh).keep;
        m_cache.emplace(key, result);
        return result;
    }
    size_t cache_size() const{ return m_cache.size(); }
private:
    std::unordered_map<uint64_t, bool> m_cache;
};

Outcome simulate(const Model& model, size_t index, const Policy& policy,
    const Variant& variant, double hours, uint64_t seed, Decisions& decisions)
{
    WinRecord path, fresh;
    Outcome result;
    uint64_t route = 0;
    double elapsed = 0;
    double rate = model.low;
    int ore = variant.ore;
    while (elapsed < hours * 3600){
        if (path.runs == 0){
            rate = uniform(seed, route, 0, 1) < model.high_probability ? model.high : model.low;
            result.new_routes++;
        }
        bool win = uniform(seed, route, path.runs, 2) < rate;
        const auto& pool = win ? model.wins : model.losses;
        double seconds = pool[(size_t)(uniform(seed, route, path.runs, 3) * pool.size())];
        if (variant.slower_good_routes && rate > model.mean) seconds *= 1.2;
        elapsed += seconds;
        if (elapsed > hours * 3600) break;
        result.wins += win;
        result.runs++;
        path.add_run(win);
        if (path.runs == 1) fresh.add_run(win);
        bool keep = decisions.keep(index, policy, path, fresh);
        if (keep && ore >= 0 && ore < 10){
            result.forced_continues++;
            keep = false;
        }
        if (keep){
            result.resets++;
            if (ore >= 0) ore -= 10;
            elapsed += variant.reset_seconds < 0 ? model.reset_seconds : variant.reset_seconds;
        }else{
            path = WinRecord();
            route++;
        }
    }
    return result;
}

Evaluation evaluate(const Model& model, size_t index, const Policy& policy,
    const Variant& variant, double hours, size_t count, uint64_t seed_base, Decisions& decisions)
{
    Evaluation result;
    double sum_squared = 0;
    for (size_t session = 0; session < count; session++){
        Outcome one = simulate(model, index, policy, variant, hours, seed_base + session, decisions);
        result.wins.push_back(one.wins);
        result.mean.wins += one.wins;
        result.mean.runs += one.runs;
        result.mean.resets += one.resets;
        result.mean.new_routes += one.new_routes;
        result.mean.forced_continues += one.forced_continues;
        sum_squared += one.wins * one.wins;
    }
    result.mean.wins /= count;
    result.mean.runs /= count;
    result.mean.resets /= count;
    result.mean.new_routes /= count;
    result.mean.forced_continues /= count;
    double variance = std::max(0.0, (sum_squared - count * result.mean.wins * result.mean.wins) / (count - 1));
    result.standard_error = std::sqrt(variance / count);
    return result;
}

std::pair<double, double> paired_difference(const Evaluation& a, const Evaluation& b){
    double total = 0, squared = 0;
    for (size_t c = 0; c < a.wins.size(); c++){
        double delta = a.wins[c] - b.wins[c];
        total += delta;
        squared += delta * delta;
    }
    double n = (double)a.wins.size();
    double mean = total / n;
    return {mean, std::sqrt(std::max(0.0, (squared - n * mean * mean) / (n - 1)) / n)};
}

std::vector<Policy> policies(){
    std::vector<Policy> result{
        {.name = "standard", .rule = PathKeepingRule::OBSERVED_WIN_RATE, .target = 1, .standard = true},
    };
    for (double target : {0.0, .60, .70, .75, .80, .85, .90, .95}){
        result.push_back(Policy{.name = "observed-" + std::to_string((int)std::round(target * 100)),
            .rule = PathKeepingRule::OBSERVED_WIN_RATE, .target = target});
    }
    for (double target : {.60, .70, .75, .80, .85, .90, .95}){
        result.push_back(Policy{.name = "likely-" + std::to_string((int)std::round(target * 100)),
            .rule = PathKeepingRule::LIKELY_WIN_RATE, .target = target});
    }
    return result;
}

void write_evaluation(std::ostream& output, const Model& model, double hours,
    const std::string& split_name, const Variant& variant, const Policy& policy, const Evaluation& result)
{
    output << model.name << ',' << hours << ',' << split_name << ',' << variant.name << ','
        << policy.name << ',' << result.mean.wins << ',' << result.mean.wins / hours << ','
        << result.mean.runs << ',' << result.mean.resets << ',' << result.mean.new_routes << ','
        << result.mean.forced_continues << ',' << result.standard_error << '\n';
}

size_t best_policy(const std::vector<Evaluation>& results, size_t begin, size_t end){
    size_t best = begin;
    for (size_t c = begin + 1; c < end; c++){
        if (results[c].mean.wins > results[best].mean.wins) best = c;
    }
    return best;
}

void study_model(const Model& model, double hours, size_t train_count, size_t validation_count,
    Decisions& decisions, std::ostream& raw, std::ostream& comparisons)
{
    const auto choices = policies();
    const Variant empirical{.name = "empirical"};
    std::vector<Evaluation> training, validation;
    for (size_t c = 0; c < choices.size(); c++){
        training.push_back(evaluate(model, c, choices[c], empirical, hours, train_count, 1000000, decisions));
        write_evaluation(raw, model, hours, "train", empirical, choices[c], training.back());
    }
    size_t observed = best_policy(training, 1, 9);
    size_t likely = best_policy(training, 9, choices.size());
    for (size_t c = 0; c < choices.size(); c++){
        validation.push_back(evaluate(model, c, choices[c], empirical, hours, validation_count, 9000000, decisions));
        write_evaluation(raw, model, hours, "heldout", empirical, choices[c], validation.back());
    }
    auto comparison = [&](const Variant& variant, size_t a, size_t b, const Evaluation& x, const Evaluation& y){
        auto [delta, se] = paired_difference(x, y);
        comparisons << model.name << ',' << hours << ',' << variant.name << ',' << choices[a].name << ','
            << choices[b].name << ',' << x.mean.wins << ',' << y.mean.wins << ',' << delta << ',' << se << '\n';
    };
    comparison(empirical, likely, observed, validation[likely], validation[observed]);
    comparison(empirical, observed, 4, validation[observed], validation[4]);
    comparison(empirical, 11, observed, validation[11], validation[observed]);
    const std::array<Variant, 4> sensitivities{{
        {.name = "no-reset-overhead", .reset_seconds = 0},
        {.name = "reset-extra-60s", .reset_seconds = 60},
        {.name = "good-routes-20pct-slower", .slower_good_routes = true},
        {.name = "999-ore-no-replenishment", .ore = 999},
    }};
    std::vector<size_t> selected{0, 4, 11, observed, likely};
    std::sort(selected.begin(), selected.end());
    selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
    for (const auto& variant : sensitivities){
        std::map<size_t, Evaluation> result;
        for (size_t c : selected){
            result.emplace(c, evaluate(model, c, choices[c], variant, hours, validation_count, 9000000, decisions));
            write_evaluation(raw, model, hours, "heldout", variant, choices[c], result.at(c));
        }
        comparison(variant, likely, observed, result.at(likely), result.at(observed));
        comparison(variant, observed, 4, result.at(observed), result.at(4));
        comparison(variant, 11, observed, result.at(11), result.at(observed));
    }
    std::cerr << model.name << " " << hours << "h: selected " << choices[observed].name << " / "
        << choices[likely].name << "; decision cache " << decisions.cache_size() << '\n';
}

void run_checks(){
    const Model certain_win{.name = "certain-win", .mean = 1, .low = 1, .high = 1,
        .high_probability = 1, .reset_seconds = 0, .wins = {600}, .losses = {600}};
    const Model certain_loss{.name = "certain-loss", .mean = 0, .low = 0, .high = 0,
        .high_probability = 1, .reset_seconds = 0, .wins = {600}, .losses = {600}};
    struct Case{
        std::string name;
        Model model;
        size_t policy;
        Variant variant;
        double expected_wins;
        double expected_runs;
        double expected_resets;
        double expected_routes;
    };
    const std::vector<Case> cases{
        {.name = "Standard six independent wins", .model = certain_win, .policy = 0,
            .variant = {.name = "base"}, .expected_wins = 6, .expected_runs = 6, .expected_resets = 0, .expected_routes = 6},
        {.name = "Observed retains one winning route", .model = certain_win, .policy = 4,
            .variant = {.name = "base"}, .expected_wins = 6, .expected_runs = 6, .expected_resets = 6, .expected_routes = 1},
        {.name = "PR55 retains one winning route", .model = certain_win, .policy = 11,
            .variant = {.name = "base"}, .expected_wins = 6, .expected_runs = 6, .expected_resets = 6, .expected_routes = 1},
        {.name = "Six losses are six fresh routes", .model = certain_loss, .policy = 4,
            .variant = {.name = "base"}, .expected_wins = 0, .expected_runs = 6, .expected_resets = 0, .expected_routes = 6},
        {.name = "Reset overhead costs one attempt", .model = certain_win, .policy = 4,
            .variant = {.name = "slow-reset", .reset_seconds = 60}, .expected_wins = 5, .expected_runs = 5, .expected_resets = 5, .expected_routes = 1},
        {.name = "No ore forces a new route", .model = certain_win, .policy = 4,
            .variant = {.name = "no-ore", .ore = 0}, .expected_wins = 6, .expected_runs = 6, .expected_resets = 0, .expected_routes = 6},
    };
    Decisions decisions;
    const auto choices = policies();
    for (const auto& test : cases){
        Outcome actual = simulate(test.model, test.policy, choices[test.policy], test.variant, 1, 1, decisions);
        if (actual.wins != test.expected_wins || actual.runs != test.expected_runs
            || actual.resets != test.expected_resets || actual.new_routes != test.expected_routes){
            throw std::runtime_error(test.name + ": unexpected win, attempt, reset or route count.");
        }
        std::cout << "PASS " << test.name << '\n';
    }
}

int main(int argc, char** argv){
    if (argc == 2 && std::string(argv[1]) == "--selftest"){
        run_checks();
        return 0;
    }
    if (argc != 6){
        std::cerr << "Usage: simulator models.tsv results.csv comparisons.csv training_sessions heldout_sessions\n";
        return 2;
    }
    try{
        auto models = read_models(argv[1]);
        std::ofstream raw(argv[2]), comparisons(argv[3]);
        raw << std::setprecision(10);
        comparisons << std::setprecision(10);
        raw << "model,hours,split,variant,policy,mean_wins,wins_per_hour,mean_attempts,mean_resets,mean_routes,mean_forced_continues,se_wins\n";
        comparisons << "model,hours,variant,policy_a,policy_b,mean_wins_a,mean_wins_b,delta_wins,paired_se_wins\n";
        Decisions decisions;
        for (const auto& model : models){
            for (double hours : {8.0, 24.0, 72.0}){
                study_model(model, hours, std::stoul(argv[4]), std::stoul(argv[5]), decisions, raw, comparisons);
            }
        }
    }catch (const std::exception& error){
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

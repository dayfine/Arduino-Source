/*  Story Monte Carlo
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <cstdlib>
#include <format>
#include "Pokemon_StoryMonteCarlo.h"

namespace PokemonAutomation{
namespace Pokemon{


uint64_t SimRandom::next(){
    m_state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = m_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
double SimRandom::uniform(){
    return (double)(next() >> 11) / (double)(1ULL << 53);
}
int64_t SimRandom::between(int64_t lo, int64_t hi){
    if (hi <= lo){
        return lo;
    }
    return lo + (int64_t)(next() % (uint64_t)(hi - lo + 1));
}
bool SimRandom::chance(double p){
    return uniform() < p;
}


DisturbancePlan make_disturbance_plan(uint64_t seed, const DisturbanceRates& rates, size_t fades, size_t banners){
    SimRandom root(seed);
    DisturbancePlan plan{
        .seed = seed,
        .rates = rates,
        .battles = SimRandom(root.next()),
        .drift = SimRandom(root.next()),
        .wanderers = SimRandom(root.next()),
        .missed_fade = std::nullopt,
        .missed_banner = std::nullopt,
    };
    SimRandom misses(root.next());
    if (fades + banners > 0 && misses.chance(rates.missed_detection)){
        const size_t which = (size_t)misses.between(0, (int64_t)(fades + banners) - 1);
        if (which < fades){
            plan.missed_fade = which;
        }else{
            plan.missed_banner = which - fades;
        }
    }
    return plan;
}

std::string describe(const DisturbancePlan& plan){
    std::string text = std::format("seed {}", plan.seed);
    if (plan.missed_fade){
        text += std::format(": miss fade {}", *plan.missed_fade);
    }
    if (plan.missed_banner){
        text += std::format(": miss banner {}", *plan.missed_banner);
    }
    return text;
}


std::vector<uint64_t> story_seeds(size_t count){
    std::vector<uint64_t> seeds;
    SimRandom pinned(20261108);
    for (size_t i = 0; i < count; i++){
        //  Short enough to type into a reproduction.
        seeds.push_back(pinned.next() % 1000000);
    }
    return seeds;
}

size_t seed_count(const char* name, size_t fallback){
    const char* value = std::getenv(name);
    if (value == nullptr){
        return fallback;
    }
    const long long count = std::atoll(value);
    return count > 0 ? (size_t)count : fallback;
}

std::vector<std::string> selected_rows(const char* name, const std::vector<std::string>& rows){
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0'){
        return rows;
    }
    const std::string wanted = std::string(",") + value + ",";
    std::vector<std::string> out;
    for (const std::string& row : rows){
        if (wanted.find("," + row + ",") != std::string::npos){
            out.push_back(row);
        }
    }
    return out;
}


std::string failure_kind(std::string_view failure, std::string_view message, size_t limit){
    std::string text(failure);
    if (!message.empty()){
        text += ": ";
        text += message;
    }
    std::string kind;
    for (char c : text){
        const bool digit = std::isdigit((unsigned char)c) != 0;
        if (!digit){
            kind += c;
        }else if (kind.empty() || kind.back() != '#'){
            kind += '#';
        }
    }
    return kind.size() > limit ? kind.substr(0, limit) + "..." : kind;
}


RowReport run_row_seeds(const std::string& row, const std::vector<uint64_t>& seeds, const SeedRun& run){
    RowReport report{.row = row};
    for (uint64_t seed : seeds){
        const SeedOutcome outcome = run(row, seed);
        report.runs++;
        if (outcome.passed){
            report.passes++;
            continue;
        }
        KindCount& count = report.failures[outcome.failed_step][outcome.kind];
        if (count.runs++ == 0){
            count.seed = seed;
            count.detail = outcome.detail;
        }
    }
    return report;
}

std::vector<RowReport> run_seeds(const std::vector<std::string>& rows, const std::vector<uint64_t>& seeds, const SeedRun& run){
    std::vector<RowReport> reports;
    for (const std::string& row : rows){
        reports.push_back(run_row_seeds(row, seeds, run));
    }
    return reports;
}

std::string format_report(const RowReport& report){
    const double rate = report.runs == 0 ? 0.0 : 100.0 * (double)report.passes / (double)report.runs;
    std::string text = std::format("{}: {}/{} passed ({:.0f}%)", report.row, report.passes, report.runs, rate);
    for (const auto& [step, kinds] : report.failures){
        for (const auto& [kind, count] : kinds){
            text += std::format("\n    {} x{}: {} (seed {}; {})", step, count.runs, kind, count.seed, count.detail);
        }
    }
    return text;
}



} // namespace Pokemon
} // namespace PokemonAutomation

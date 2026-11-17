/*  AutoStory League Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include "PokemonFRLG_AutoStoryLeagueLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

std::string_view end_name(LeagueBattleEnd end){
    switch (end){
    case LeagueBattleEnd::WON:
        return "won";
    case LeagueBattleEnd::LOST:
        return "lost";
    case LeagueBattleEnd::ROARED:
        return "reset on a Roar or Whirlwind";
    case LeagueBattleEnd::EARLY_RESET:
        return "early reset";
    }
    return "";
}

std::string counts_text(const SlugCounts& counts){
    std::string text;
    for (const auto& [name, count] : counts){
        if (count != 0){
            text += std::format("{}{} {}", text.empty() ? "" : ", ", name, count);
        }
    }
    return text.empty() ? std::string("nothing") : text;
}

//  The first of `floors` that `counts` is below, as a line; none if none.
std::optional<std::string> below_floor(std::string_view what, const SlugCounts& counts, const SlugCounts& floors){
    for (const auto& [name, floor] : floors){
        const int have = count_of(counts, name);
        if (have < floor){
            return std::format("{} {}: {}, at least {}", what, name, have, floor);
        }
    }
    return std::nullopt;
}

//  What `before` has more of than `after`.
SlugCounts spent(const SlugCounts& before, const SlugCounts& after){
    SlugCounts used;
    for (const auto& [item, count] : before){
        const int less = count - count_of(after, item);
        if (less > 0){
            used[item] = less;
        }
    }
    return used;
}

class FightLoop{
public:
    FightLoop(const LeagueFightData& fight, const LeagueFightRunner& runner)
        : m_fight(fight)
        , m_runner(runner)
    {}

    LeagueFightReport run(){
        if (!m_fight.pass){
            m_runner.log(std::format("League {}: the plan has no pass figures; stopping.", m_fight.name));
            return std::move(m_report);
        }
        const LeaguePassFigures& figures = *m_fight.pass;
        for (int c = 0; c < figures.min_pass_of; c++){
            if (!try_once(1, c, figures.min_pass_of)){
                return std::move(m_report);
            }
            m_report.phase1_passes += m_report.tries.back().passes ? 1 : 0;
        }
        if (m_report.phase1_passes < figures.min_pass){
            return stop(LeagueFightVerdict::BELOW_MIN_PASS, std::format(
                "phase 1 passed {} of {}, under {}", m_report.phase1_passes, figures.min_pass_of, figures.min_pass
            ));
        }
        for (int c = 0; c < figures.reset_budget; c++){
            if (!try_once(2, c, figures.reset_budget)){
                return std::move(m_report);
            }
            if (m_report.tries.back().passes){
                m_runner.save();
                m_runner.log(std::format("League {}: saved after phase 2 try {}.", m_fight.name, c + 1));
                m_report.verdict = LeagueFightVerdict::SAVED;
                return std::move(m_report);
            }
        }
        return stop(LeagueFightVerdict::OUT_OF_BUDGET, std::format("no pass in {} phase 2 tries", figures.reset_budget));
    }

private:
    LeagueFightReport stop(LeagueFightVerdict verdict, std::string detail){
        m_runner.log(std::format("League {}: {}; stopping.", m_fight.name, detail));
        m_report.verdict = verdict;
        m_report.detail = std::move(detail);
        return std::move(m_report);
    }

    //  One try from the start save; false if the entry contract broke.
    bool try_once(int phase, int index, int of){
        m_runner.load_start();
        const LeagueSide start = m_runner.read_side();
        LeagueTryRecord record{.phase = phase, .top_up = m_runner.top_up()};
        const LeagueSide entry = m_runner.read_side();
        if (std::optional<std::string> broken = entry_broken(entry)){
            stop(LeagueFightVerdict::PRECONDITION_FAILED, std::format("entry contract: {}", *broken));
            return false;
        }
        record.end = m_runner.fight();
        std::optional<std::string> after;
        if (record.end == LeagueBattleEnd::WON){
            const LeagueSide end = m_runner.read_side();
            record.used = spent(start.bag, end.bag);
            after = below_floor("item", end.bag, m_fight.reserve_items);
            after = after ? after : below_floor("PP", end.pp, m_fight.reserve_pp);
        }
        record.passes = !after && league_try_passes(m_fight, LeagueTry{
            .won = record.end == LeagueBattleEnd::WON,
            .roared = record.end == LeagueBattleEnd::ROARED,
            .early_reset = record.end == LeagueBattleEnd::EARLY_RESET,
            .used = record.used,
        });
        m_runner.log(std::format(
            "League {}: phase {} try {}/{}: HP {}/{}, top-up [{}], {}, used {}{}: {}.",
            m_fight.name, phase, index + 1, of, entry.hp, entry.max_hp, counts_text(counted(record.top_up)),
            end_name(record.end), counts_text(record.used), after ? std::format(", {}", *after) : "",
            record.passes ? "pass" : "no pass"
        ));
        if (after){
            m_report.detail = *after;
        }
        m_report.tries.push_back(std::move(record));
        return true;
    }

    //  The entry contract: every item at least its count (hard); a move's
    //  PP under its count only logged.
    std::optional<std::string> entry_broken(const LeagueSide& entry) const{
        if (std::optional<std::string> pp = below_floor("PP", entry.pp, m_fight.entry_pp)){
            m_runner.log(std::format("League {}: entry {}; fighting on.", m_fight.name, *pp));
        }
        return below_floor("item", entry.bag, m_fight.entry_items);
    }

    static SlugCounts counted(const std::vector<std::string>& items){
        SlugCounts counts;
        for (const std::string& item : items){
            counts[item]++;
        }
        return counts;
    }

    const LeagueFightData& m_fight;
    const LeagueFightRunner& m_runner;
    LeagueFightReport m_report;
};

} // namespace


LeagueFightReport run_league_fight(const LeagueFightData& fight, const LeagueFightRunner& runner){
    return FightLoop(fight, runner).run();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

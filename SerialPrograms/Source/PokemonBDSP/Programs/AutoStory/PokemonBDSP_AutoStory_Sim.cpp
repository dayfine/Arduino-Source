/*  BDSP AutoStory - Movement Simulator
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <tuple>
#include "PokemonBDSP_AutoStory_Sim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Checkpoint;
using Pokemon::Direction;
using Pokemon::Event;
using Pokemon::Outcome;
using Pokemon::RunState;
using Pokemon::TileGrid;
using Pokemon::TilePos;
using Pokemon::TileType;
using Pokemon::outcome_failed;
using Pokemon::outcome_ok;


namespace{

const char* const SIGN_TEXT = "The Pok\xc3\xa9tch Company / Pok\xc3\xa9mon Watches for the World!";
const char* const CLOWN_TEXT = "Hi! I'm a Pok\xc3\xa9tch campaign clown!";

}


const TileGrid& sim_poketch_company_grid(){
    //     0         1         2
    //     01234567890123456789012345
    static const TileGrid grid({
        R"(##########################)",  //   0
        R"(##########################)",  //   1  the Company's front wall
        R"(###............###########)",  //   2  the plaza
        R"(###............###########)",  //   3
        R"(###............###########)",  //   4
        R"(########S.################)",  //   5  hedges, the sign, the gap
        R"(##.......................#)",  //   6  L = (2,6)
        R"(##.......................#)",  //   7  the long road; the lamp (1,7)
        R"(########.#################)",  //   8  two buildings, a passage
        R"(########.#################)",  //   9
        R"(########.#################)",  //  10
        R"(##########################)",  //  11
    });
    return grid;
}
TilePos sim_long_road_east_end(){ return {24, 7}; }
TilePos sim_poketch_company_sign(){ return {8, 5}; }
TilePos sim_poketch_company_gap(){ return {9, 5}; }


SimJubilife::SimJubilife(const SimModel& model, uint64_t seed)
    : m_model(model)
    , m_rng(seed)
    , m_grid(sim_poketch_company_grid())
    , m_pos(sim_long_road_east_end())
{
    std::vector<TilePos> patch;
    for (int y = m_model.clown_patch_min.y; y <= m_model.clown_patch_max.y; y++){
        for (int x = m_model.clown_patch_min.x; x <= m_model.clown_patch_max.x; x++){
            if (m_grid.standable({x, y})){
                patch.push_back({x, y});
            }
        }
    }
    m_clown = patch[std::uniform_int_distribution<size_t>(0, patch.size() - 1)(m_rng)];
    m_clown_next_move = std::uniform_int_distribution<int64_t>(0, m_model.clown_move_max_ms)(m_rng);
}

bool SimJubilife::chance(double p){
    return std::uniform_real_distribution<double>(0, 1)(m_rng) < p;
}
void SimJubilife::advance(int64_t ms){
    m_clock += ms;
    while (m_clown_next_move <= m_clock){
        const Direction direction = Pokemon::ALL_DIRECTIONS[std::uniform_int_distribution<int>(0, 3)(m_rng)];
        const TilePos next = m_clown.next(direction);
        if (in_clown_patch(next) && m_grid.standable(next) && !(next == m_pos)){
            m_clown = next;
        }
        m_clown_next_move += std::uniform_int_distribution<int64_t>(m_model.clown_move_min_ms, m_model.clown_move_max_ms)(m_rng);
    }
}
bool SimJubilife::in_clown_patch(const TilePos& pos) const{
    return pos.x >= m_model.clown_patch_min.x && pos.x <= m_model.clown_patch_max.x &&
        pos.y >= m_model.clown_patch_min.y && pos.y <= m_model.clown_patch_max.y;
}
bool SimJubilife::open(const TilePos& pos) const{
    return m_grid.standable(pos) && !(pos == m_clown);
}
bool SimJubilife::move(Direction direction){
    m_facing = direction;
    const TilePos next = m_pos.next(direction);
    if (!open(next)){
        return false;
    }
    m_pos = next;
    return true;
}
void SimJubilife::turn_tap(Direction direction){
    advance(m_model.turn_ms);
    if (m_facing != direction){
        m_facing = direction;
        return;
    }
    if (move(direction)){
        m_turn_steps++;
        log(std::string("turn ") + Pokemon::direction_name(direction) + " was a step");
    }
}
int64_t SimJubilife::ms_per_tile(Direction direction) const{
    return direction == Direction::EAST || direction == Direction::WEST
        ? m_model.ms_per_tile_across
        : m_model.ms_per_tile_up_down;
}
void SimJubilife::log(const std::string& line){
    m_trace.push_back(
        std::to_string(m_clock) + " ms " + Pokemon::to_string(m_pos) + " " +
        Pokemon::direction_name(m_facing) + ": " + line
    );
}


Outcome SimJubilife::walk_until_stopped(Direction direction, size_t tiles, const std::string& label){
    //  Before the grid: the scripted legs from the TV station.
    if (m_approach_done < m_model.approach.size()){
        const SimScriptedLeg& leg = m_model.approach[m_approach_done++];
        if (leg.direction != direction){
            return outcome_failed("off_route", label + ": the sim's approach expected another direction");
        }
        double lag = 0;
        if (leg.still_lag_mean_tiles > 0){
            lag = std::exponential_distribution<double>(1.0 / leg.still_lag_mean_tiles)(m_rng);
        }
        const double seen = std::min<double>((double)leg.tiles + lag, (double)tiles);
        advance(m_model.walk_overhead_ms + (int64_t)(seen * ms_per_tile(direction)));
        m_facing = direction;
        if ((double)leg.tiles + lag > (double)tiles){
            log(label + ": stopped, but not seen still in time");
            return outcome_failed("off_route", label + ": nothing stopped us within " +
                std::to_string(tiles) + " tiles' time; not on the measured route");
        }
        log(label + ": stopped (scripted)");
        return outcome_ok();
    }
    turn_tap(direction);
    advance(m_model.walk_overhead_ms);
    size_t walked = 0;
    while (walked < tiles){
        advance(ms_per_tile(direction));
        if (!move(direction)){
            log(label + ": stopped after " + std::to_string(walked));
            return outcome_ok();
        }
        walked++;
    }
    //  Still walking at the limit: the stop is beyond it.
    if (!open(m_pos.next(direction))){
        log(label + ": stopped at the limit");
        return outcome_ok();
    }
    log(label + ": limit");
    return outcome_failed("off_route", label + ": nothing stopped us within " +
        std::to_string(tiles) + " tiles' time; not on the measured route");
}

DialogueRead SimJubilife::talk(Direction direction, Button next, const std::string& label){
    turn_tap(direction);
    DialogueRead read;
    const TilePos ahead = m_pos.next(m_facing);
    if (ahead == m_clown){
        read.opened = true;
        read.closed = true;
        read.text = CLOWN_TEXT;
        //  His question's default answer is right: A takes the coupon.
        if (next == BUTTON_A){
            m_coupon = true;
        }
        advance(m_model.talk_answer_ms);
        log(label + ": the clown talked");
        return read;
    }
    if (m_grid.at(ahead) == TileType::SIGN && chance(m_model.talk_opens_sign_chance)){
        read.opened = true;
        read.closed = true;
        read.text = chance(m_model.sign_ocr_empty_chance) ? "" : SIGN_TEXT;
        advance(m_model.read_sign_open_ms);
        log(label + ": the sign's box");
        return read;
    }
    advance(m_model.talk_no_answer_ms);
    return read;
}

DialogueRead SimJubilife::read_sign(Direction direction, const std::string& label){
    turn_tap(direction);
    DialogueRead read;
    const TilePos ahead = m_pos.next(m_facing);
    const bool sign = m_grid.at(ahead) == TileType::SIGN;
    if (!sign && !(ahead == m_clown)){
        advance(m_model.read_sign_ms);
        log(label + ": no box");
        return read;
    }
    //  The clown answers A too, and B turns his question down: no coupon.
    read.opened = true;
    read.closed = true;
    read.text = chance(m_model.sign_ocr_empty_chance) ? "" : sign ? SIGN_TEXT : CLOWN_TEXT;
    advance(m_model.read_sign_open_ms);
    log(label + (sign ? ": the sign" : ": the clown, closed with B") + (read.text.empty() ? " (no text)" : ""));
    return read;
}

Outcome SimJubilife::step(Direction direction, const std::string& label){
    //  200 ms: one tile, turning included.
    advance(m_model.step_ms);
    const bool moved = move(direction);
    log(label + (moved ? ": stepped" : ": blocked"));
    return outcome_ok();
}

Outcome SimJubilife::tile(Direction direction, const std::string& label){
    advance(m_model.tile_ms);
    const bool moved = move(direction);
    if (moved && chance(m_model.tile_extra_chance)){
        move(direction);
    }
    log(label + ": tile");
    return outcome_ok();
}

Outcome SimJubilife::taps(Direction direction, size_t tiles, const std::string& label){
    for (size_t i = 0; i < tiles; i++){
        advance(m_model.tap_ms);
        if (move(direction) && chance(m_model.taps_extra_chance)){
            move(direction);
        }
    }
    advance(m_model.taps_settle_ms);
    log(label + ": " + std::to_string(tiles) + " taps");
    return outcome_ok();
}

void SimJubilife::turn(Direction direction){
    turn_tap(direction);
}

Outcome SimJubilife::hold(Direction direction, int64_t ms, const std::string& label){
    const int64_t tiles = ms / ms_per_tile(direction);
    advance(ms + m_model.turn_ms);
    for (int64_t i = 0; i < tiles; i++){
        move(direction);
    }
    log(label + ": hold");
    return outcome_ok();
}


//  Not on 00.05f's route.
Outcome SimJubilife::walk(std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label){
    return outcome_failed("not_simulated", label);
}
Outcome SimJubilife::walk_in_town(std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label){
    return outcome_failed("not_simulated", label);
}
Outcome SimJubilife::line_up_with_door(){ return outcome_failed("not_simulated"); }
Outcome SimJubilife::east_then_north_in_sandgem(){ return outcome_failed("not_simulated"); }
DialogueRead SimJubilife::talk_walking(Direction, size_t, const std::string&){ return {}; }
Outcome SimJubilife::walk_to_route_202_save_spot(){ return outcome_failed("not_simulated"); }
Outcome SimJubilife::save_on_route_202(){ return outcome_failed("not_simulated"); }
Outcome SimJubilife::climb_the_steps(){ return outcome_failed("not_simulated"); }
Outcome SimJubilife::heal_at_center(){ return outcome_failed("not_simulated"); }
void SimJubilife::nudge(Direction, const std::string&){}
void SimJubilife::step_across(double, const std::string&){}
DialogueRead SimJubilife::walk_until_talked_to(size_t, const std::string&){ return {}; }
Outcome SimJubilife::leave_south(const std::string& label){ return outcome_failed("not_simulated", label); }
bool SimJubilife::through_door_north(const std::string&){ return false; }



std::string SimStats::to_string() const{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << successes << " / " << runs << " succeeded (" << 100 * success_rate() << "%)";
    if (false_successes > 0){
        out << ", " << false_successes << " completed without his talk";
    }
    out << "; time mean " << mean_ms / 1000 << " s, max " << (double)max_ms / 1000 <<
        " s, mean when it succeeded " << mean_success_ms / 1000 << " s";
    if (runs_with_turn_steps > 0){
        out << "; " << runs_with_turn_steps << " runs had a turn that stepped";
    }
    std::vector<std::pair<std::string, size_t>> sorted(failures.begin(), failures.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b){ return a.second > b.second; });
    for (const auto& [reason, count] : sorted){
        out << "\n    " << count << " x " << reason << "  (e.g. " << failure_example.at(reason) << ")";
    }
    return out.str();
}

SimStats run_monte_carlo(
    const std::function<Checkpoint(JubilifeActions&)>& make_checkpoint,
    const SimModel& model, size_t runs, uint64_t seed
){
    SimStats stats;
    double total_ms = 0;
    double success_ms = 0;
    for (size_t i = 0; i < runs; i++){
        SimJubilife game(model, seed + i);
        Checkpoint checkpoint = make_checkpoint(game);
        RunState state;
        Pokemon::EventLogger log = [](const std::string&){};
        const Pokemon::CheckpointReport report = Pokemon::run_checkpoint(state, checkpoint, log);

        stats.runs++;
        total_ms += (double)game.clock_ms();
        stats.max_ms = std::max(stats.max_ms, game.clock_ms());
        if (game.turn_steps() > 0 && stats.runs_with_turn_steps++ == 0){
            stats.turn_step_trace = game.trace();
        }
        std::string reason;
        if (report.completed && game.got_coupon()){
            stats.successes++;
            success_ms += (double)game.clock_ms();
            continue;
        }
        if (report.completed){
            stats.false_successes++;
            reason = "completed without his talk";
        }else{
            reason = "event " + std::to_string(report.failed_event) + " " + report.failed_name + ": " +
                report.failure.failure + (report.failure.detail.empty() ? "" : ": " + report.failure.detail);
        }
        reason += " @ " + Pokemon::to_string(game.position());
        if (stats.failures[reason]++ == 0){
            stats.failure_example[reason] = "seed " + std::to_string(seed + i) + ", facing " +
                Pokemon::direction_name(game.facing()) + ", clown at " + Pokemon::to_string(game.clown());
        }
    }
    stats.mean_ms = runs == 0 ? 0 : total_ms / runs;
    stats.mean_success_ms = stats.successes == 0 ? 0 : success_ms / stats.successes;
    return stats;
}



Checkpoint sim_clown_company_by_passage_checkpoint(JubilifeActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "sim_clown_company_by_passage";
    const auto flagged = [](std::string name, std::string flag, Pokemon::Act act){
        Event event;
        event.name = name;
        event.why = name;
        event.skip_if.push_back(Pokemon::condition(flag, [flag](RunState& state){ return state.flags.count(flag) > 0; }));
        event.act = [flag, act = std::move(act)](RunState& state) mutable{
            const Outcome done = act(state);
            if (done.succeeded()){
                state.flags.insert(flag);
            }
            return done;
        };
        event.expect.push_back(Pokemon::condition(flag, [flag](RunState& state){ return state.flags.count(flag) > 0; }));
        return event;
    };
    checkpoint.events.push_back(flagged("to_the_lamp", "at_long_road_lamp", [&game](RunState&){
        for (const auto& [direction, tiles, label] : {
            std::tuple<Direction, size_t, const char*>{Direction::WEST, 20, "West to the hedges"},
            {Direction::SOUTH, 10, "South to the long road"},
            {Direction::WEST, 26, "West to the lamp"},
        }){
            const Outcome walked = game.walk_until_stopped(direction, tiles, label);
            if (!walked.succeeded()){
                return walked;
            }
        }
        return outcome_ok();
    }));
    //  The one open leg: six taps east, then the passage's walls hold the
    //  column; north out of it the sign stops us, and one read says so.
    checkpoint.events.push_back(flagged("to_the_sign", "below_sign", [&game](RunState&){
        game.turn(Direction::EAST);
        game.taps(Direction::EAST, 6, "East to the passage");
        const Outcome down = game.walk_until_stopped(Direction::SOUTH, 6, "South down the passage");
        if (!down.succeeded()){
            return down;
        }
        const Outcome up = game.walk_until_stopped(Direction::NORTH, 8, "North to the sign");
        if (!up.succeeded()){
            return up;
        }
        const DialogueRead sign = game.read_sign(Direction::NORTH, "the Poketch Company's sign");
        if (!sign.opened || (!sign.text.empty() && sign.text.find("Company") == std::string::npos)){
            return outcome_failed("not_at_sign", "no sign north of us");
        }
        game.turn(Direction::EAST);
        game.taps(Direction::EAST, 1, "East of the sign");
        return game.step(Direction::NORTH, "Into the gap beside the sign");
    }));
    {
        std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
        Event talk;
        talk.name = "talk_to_clown";
        talk.why = "the clown asks a question (Yes is right) and gives Coupon 3";
        talk.act = [&game, read](RunState&){
            for (size_t round = 0; round < 18 && !read->opened; round++){
                for (Direction direction : {Direction::EAST, Direction::NORTH, Direction::WEST}){
                    *read = game.talk(direction, BUTTON_A, "Clown by the Poketch Company");
                    if (read->opened && read->text.find("Company") != std::string::npos){
                        *read = DialogueRead();
                    }
                    if (read->opened){
                        break;
                    }
                }
            }
            return outcome_ok();
        };
        talk.expect.push_back(Pokemon::condition("his talk opened and was closed", [read](RunState&){
            return read->opened && read->closed;
        }));
        checkpoint.events.push_back(std::move(talk));
    }
    return checkpoint;
}



}
}
}

/*  BDSP AutoStory - Movement Simulator
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Plays JubilifeActions on a tile grid with a stochastic movement model,
 *  so a checkpoint's own code runs unchanged against it, and runs a
 *  checkpoint many times (Monte Carlo) to score it in seconds instead of
 *  three minutes a console run.
 *
 *  The model's numbers were measured on the console (2026-10-30) where the
 *  comments say so; the others are guesses, named as such.
 *
 *  The grid is a hand-made local grid of the Poketch Company's corner of
 *  Jubilife (JubilifeCity::grid() is DP-derived and rough there), built from
 *  console facts relative to the lamp at the long road's west end. The walk
 *  there from the TV station clown is played as scripted legs.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Sim_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Sim_H

#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "StorySegments/PokemonBDSP_AutoStory_SandgemCenterEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A leg of a walk until stopped that the sim does not lay out on its grid:
//  it stops after `tiles` tiles. `still_lag_mean_tiles` models the screen
//  still detector being late (an animated screen nearby keeps the picture
//  moving): the stop is seen that many tiles' time late on average
//  (exponential), and a leg whose `tiles` plus lag exceeds its time limit
//  reads as LIMIT.
struct SimScriptedLeg{
    Pokemon::Direction direction;
    size_t tiles = 0;
    double still_lag_mean_tiles = 0;
};

struct SimModel{
    //  tile(): one 160 ms hold moves one tile, sometimes two (six tile()
    //  calls went seven once). Chance per hold: a guess from that.
    double tile_extra_chance = 0.08;
    //  taps(n): n tiles, sometimes n + 1 (six taps went six by hand, seven in
    //  r160a). Chance per tap, so per six taps about 40%: a guess from that.
    double taps_extra_chance = 0.08;
    //  read_sign(): the box opens but OCR reads no text. A guess.
    double sign_ocr_empty_chance = 0.15;
    //  talk() facing a sign: whether a sign's box reads as a dialogue. The
    //  facts say a talk opens only on an NPC.
    double talk_opens_sign_chance = 0;

    //  The Poketch Company clown tries a random step every few seconds
    //  (uniform in this range, a guess), within his patch; he blocks us.
    //  The patch: the plaza behind the hedges east of the sign, the sign's
    //  row (only the gap is open there) and three rows north. How far east
    //  it reaches is a guess; it sets how often he comes by the gap.
    Pokemon::TilePos clown_patch_min{9, 2};
    Pokemon::TilePos clown_patch_max{11, 5};
    int64_t clown_move_min_ms = 2000;
    int64_t clown_move_max_ms = 5000;

    //  Clock, per action.
    int64_t tap_ms = 560;               //  taps(): a 160 ms hold, 400 ms apart
    int64_t taps_settle_ms = 800;       //  taps(): the wait after the last
    int64_t tile_ms = 1000;             //  tile(): hold and settle (a guess)
    int64_t step_ms = 1200;             //  step()
    int64_t turn_ms = 400;              //  route_turn(): 100 ms tap, 300 ms wait
    int64_t talk_no_answer_ms = 2500;
    int64_t talk_answer_ms = 9000;      //  his talk and question (a guess)
    int64_t read_sign_ms = 2800;        //  no box
    int64_t read_sign_open_ms = 4500;   //  a box: OCR and B (a guess)
    int64_t walk_overhead_ms = 1050;    //  start delay and the wait to read still
    int64_t ms_per_tile_across = 160;
    int64_t ms_per_tile_up_down = 240;

    //  The walk from beside the TV station clown to the long road, before
    //  the grid: 00.05f's first two legs. Tile counts and the lag are
    //  guesses; r161 saw the first leg's stop only at 14 tiles' time, r162a
    //  not within it.
    std::vector<SimScriptedLeg> approach = {
        {Pokemon::Direction::WEST, 8, 3.0},
        {Pokemon::Direction::SOUTH, 5, 0},
    };
};


//  The local grid (x east, y south), hand-made from console facts
//  (2026-10-30) around the lamp L at the long road's west end:
//    - "North by the lamp" stops at L (2,6), a row south of the hedge row 5.
//    - The Poketch Company's sign is six tiles east of L, in the hedge row
//      (8,5); it reads facing north from (8,6). Hedges run west of it.
//    - The tile east of the sign (9,5) is a gap into the plaza.
//    - The Company's front wall is about four rows north of the hedges (1);
//      the plaza between (rows 2-4) is open.
//  Guessed: hedges east of the gap, the plaza's width, the long road (row 7)
//  and its east end, and the two buildings south of it with a one-tile
//  passage in the sign's column (for the alternative route).
const Pokemon::TileGrid& sim_poketch_company_grid();
//  Where the scripted approach puts us: the long road's east end.
Pokemon::TilePos sim_long_road_east_end();
Pokemon::TilePos sim_poketch_company_sign();
Pokemon::TilePos sim_poketch_company_gap();


//  JubilifeActions played on sim_poketch_company_grid(). Actions 00.05f does
//  not use fail with "not_simulated".
class SimJubilife : public JubilifeActions{
public:
    SimJubilife(const SimModel& model, uint64_t seed);

    //  What happened, for scoring.
    bool got_coupon() const{ return m_coupon; }
    int64_t clock_ms() const{ return m_clock; }
    Pokemon::TilePos position() const{ return m_pos; }
    Pokemon::Direction facing() const{ return m_facing; }
    Pokemon::TilePos clown() const{ return m_clown; }
    //  Turns the way we already faced that moved us a tile (r161c).
    size_t turn_steps() const{ return m_turn_steps; }
    const std::vector<std::string>& trace() const{ return m_trace; }

    virtual std::string save_screen_location() override{ return "Jubilife City"; }
    virtual Pokemon::Outcome walk(
        std::vector<Pokemon::TilePos>& at,
        const std::function<bool(const Pokemon::TilePos&)>& goal,
        const std::string& label
    ) override;
    virtual bool east_then_south_into_sandgem() override{ return false; }
    virtual Pokemon::Outcome walk_in_town(
        std::vector<Pokemon::TilePos>& at,
        const std::function<bool(const Pokemon::TilePos&)>& goal,
        const std::string& label
    ) override;
    virtual DialogueRead read_sign(Pokemon::Direction facing, const std::string& label) override;
    virtual std::optional<LitDoorway> lit_doorway() override{ return std::nullopt; }
    virtual Pokemon::Outcome line_up_with_door() override;
    virtual bool inside_pokemon_center() override{ return false; }
    virtual Pokemon::Outcome east_then_north_in_sandgem() override;
    virtual bool dialogue_showing() override{ return false; }
    virtual Pokemon::Outcome finish_dialogue() override{ return Pokemon::outcome_ok(); }

    virtual Pokemon::Outcome walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual DialogueRead talk(Pokemon::Direction direction, Button next, const std::string& label) override;
    virtual Pokemon::Outcome step(Pokemon::Direction direction, const std::string& label) override;
    virtual DialogueRead talk_walking(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual Pokemon::Outcome walk_to_route_202_save_spot() override;
    virtual Pokemon::Outcome save_on_route_202() override;

    virtual Pokemon::Outcome climb_the_steps() override;
    virtual Pokemon::Outcome heal_at_center() override;
    virtual void turn(Pokemon::Direction direction) override;
    virtual void nudge(Pokemon::Direction direction, const std::string& label) override;
    virtual std::optional<double> school_door_across() override{ return std::nullopt; }
    virtual void step_across(double tiles, const std::string& label) override;
    virtual bool indoors() override{ return false; }
    virtual Pokemon::Outcome hold(Pokemon::Direction direction, int64_t ms, const std::string& label) override;
    virtual DialogueRead walk_until_talked_to(size_t tiles, const std::string& label) override;
    virtual Pokemon::Outcome leave_south(const std::string& label) override;
    virtual Pokemon::Outcome tile(Pokemon::Direction direction, const std::string& label) override;
    virtual Pokemon::Outcome taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool through_door_north(const std::string& label) override;

private:
    bool chance(double p);
    //  Moves the clock on, and the clown with it.
    void advance(int64_t ms);
    bool in_clown_patch(const Pokemon::TilePos& pos) const;
    bool open(const Pokemon::TilePos& pos) const;
    //  One tile `direction` if open; faces it either way.
    bool move(Pokemon::Direction direction);
    //  route_turn(): a tap only turns us when we face another way; facing it
    //  already, it is a step if the tile ahead is open (r161c).
    void turn_tap(Pokemon::Direction direction);
    int64_t ms_per_tile(Pokemon::Direction direction) const;
    void log(const std::string& line);

private:
    SimModel m_model;
    std::mt19937_64 m_rng;
    const Pokemon::TileGrid& m_grid;
    size_t m_approach_done = 0;
    Pokemon::TilePos m_pos;
    Pokemon::Direction m_facing = Pokemon::Direction::SOUTH;
    Pokemon::TilePos m_clown;
    int64_t m_clock = 0;
    int64_t m_clown_next_move = 0;
    bool m_coupon = false;
    size_t m_turn_steps = 0;
    std::vector<std::string> m_trace;
};


//  A checkpoint run `runs` times against SimJubilife, seeds seed, seed + 1, ...
struct SimStats{
    size_t runs = 0;
    size_t successes = 0;
    //  Completed, but the clown never talked (a box taken for his).
    size_t false_successes = 0;
    double mean_ms = 0;
    int64_t max_ms = 0;
    double mean_success_ms = 0;
    size_t runs_with_turn_steps = 0;
    //  The trace of the first run whose turn stepped.
    std::vector<std::string> turn_step_trace;
    //  "event N name: failure: detail @ the tile it ended on" -> runs; and
    //  the first such run's seed and where the clown was.
    std::map<std::string, size_t> failures;
    std::map<std::string, std::string> failure_example;

    double success_rate() const{ return runs == 0 ? 0 : (double)successes / runs; }
    std::string to_string() const;
};
SimStats run_monte_carlo(
    const std::function<Pokemon::Checkpoint(JubilifeActions&)>& make_checkpoint,
    const SimModel& model, size_t runs, uint64_t seed
);


//  For comparison only, not run on the console: the alternative way to the
//  sign (the user's idea), as far as the guessed layout goes. Along the long
//  road to the lamp; six taps east to the passage between the two buildings
//  south of the road, south down it, then north until the sign stops us; one
//  read of the sign, then the same wait in the gap as 00.05f.
Pokemon::Checkpoint sim_clown_company_by_passage_checkpoint(JubilifeActions& game);



}
}
}
#endif

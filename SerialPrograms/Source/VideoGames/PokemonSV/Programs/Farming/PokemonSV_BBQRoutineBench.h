/*  BBQ Routine Bench
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lab only. Runs one BBQ quest routine N times and judges each run, so two builds
 *  can be compared routine by routine instead of by whole farming runs.
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQRoutineBench_H
#define PokemonAutomation_PokemonSV_BBQRoutineBench_H

#include <string>
#include <vector>
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class BBQBenchRoutine{
    SneakUp,
    TeraSelf,
    CoastalGlide,   //  Coastal Plaza -> grassland hill, look around (catch-flying spot).
};

// Where each rep starts from.
enum class BBQBenchStart{
    BackToBack,     //  Wherever the last rep left the player, as the farmer does.
    Central,        //  Central Plaza.
    PolarRest,      //  Polar Rest Area, so the route flies in from another region.
};

// Two variants compared rep by rep: odd reps run A, even reps run B, so both see the
// same spawns and time of day.
enum class BBQBenchVariant{
    None,               //  Every rep runs the code as built.
    TeraSelfSpot,       //  A: Pride Rock. B: the Chargestone court (R11/R12).
    DirectLeaveArea,    //  A: return to Central. B: fly directly, leaving the spot before the day skip.
};

// One judged run of a routine.
struct BBQBenchRep{
    bool pass;
    double seconds;
    char arm = '-';
};

// "passes/reps, median N s" over the reps so far. Median of all reps, passed or not.
std::string bbq_bench_summary(const std::vector<BBQBenchRep>& reps);

// bbq_bench_summary() of the reps of one arm ('A' or 'B').
std::string bbq_bench_arm_summary(const std::vector<BBQBenchRep>& reps, char arm);

// What an arm runs, for the log and the overlay: "A: Pride Rock". "-" without a variant.
std::string bbq_bench_arm_label(BBQBenchVariant variant, char arm);


class BBQRoutineBench_Descriptor : public SingleSwitchProgramDescriptor{
public:
    BBQRoutineBench_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class BBQRoutineBench : public SingleSwitchProgramInstance{
public:
    using Descriptor = BBQRoutineBench_Descriptor;
    BBQRoutineBench();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    EnumDropdownOption<BBQBenchRoutine> ROUTINE;
    SimpleIntegerOption<uint16_t> REPS;
    EnumDropdownOption<BBQBenchStart> START_FROM;
    EnumDropdownOption<BBQBenchVariant> VARIANT;
    BBQOption BBQ_OPTIONS;
};

}
}
}
#endif

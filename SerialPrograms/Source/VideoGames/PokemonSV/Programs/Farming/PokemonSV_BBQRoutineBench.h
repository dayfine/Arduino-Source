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
};

// Where each rep starts from.
enum class BBQBenchStart{
    BackToBack,     //  Wherever the last rep left the player, as the farmer does.
    Central,        //  Central Plaza.
    PolarRest,      //  Polar Rest Area, so the route flies in from another region.
};

// One judged run of a routine.
struct BBQBenchRep{
    bool pass;
    double seconds;
};

// "passes/reps, median N s" over the reps so far. Median of all reps, passed or not.
std::string bbq_bench_summary(const std::vector<BBQBenchRep>& reps);


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
    BBQOption BBQ_OPTIONS;
};

}
}
}
#endif

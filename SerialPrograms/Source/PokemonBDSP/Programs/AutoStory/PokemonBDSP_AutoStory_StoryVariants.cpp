/*  BDSP AutoStory - Story step variants
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <array>
#include <utility>
#include "PokemonBDSP_AutoStory_StoryVariants.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::StoryStepKind;


namespace{

//  Each way's name; the first is "", the kind's usual way.
template <typename Way>
struct WayNames;
template <>
struct WayNames<RouteCosts>{
    static constexpr std::array<std::pair<RouteCosts, const char*>, 3> names{{
        {RouteCosts::ROUTE, ""},
        {RouteCosts::TOWN, "town"},
        {RouteCosts::ROBUST, "robust"},
    }};
};
template <>
struct WayNames<LegsWay>{
    static constexpr std::array<std::pair<LegsWay, const char*>, 5> names{{
        {LegsWay::DPAD, ""},
        {LegsWay::DPAD_ONE_SHORT_OK, "one_short_ok"},
        {LegsWay::TO_WALL, "to_wall"},
        {LegsWay::HOLD, "hold"},
        {LegsWay::HOLD_TAPS, "hold_taps"},
    }};
};
template <>
struct WayNames<MashWay>{
    static constexpr std::array<std::pair<MashWay, const char*>, 3> names{{
        {MashWay::B, ""},
        {MashWay::A, "a"},
        {MashWay::WAIT_OUT, "wait_out"},
    }};
};
template <>
struct WayNames<StepWay>{
    static constexpr std::array<std::pair<StepWay, const char*>, 2> names{{
        {StepWay::DPAD, ""},
        {StepWay::OUT_SOUTH, "out_south"},
    }};
};
template <>
struct WayNames<TalkWay>{
    static constexpr std::array<std::pair<TalkWay, const char*>, 2> names{{
        {TalkWay::AHEAD, ""},
        {TalkWay::WALKING, "walking"},
    }};
};
template <>
struct WayNames<BattleFormat>{
    static constexpr std::array<std::pair<BattleFormat, const char*>, 2> names{{
        {BattleFormat::SINGLES, ""},
        {BattleFormat::DOUBLES, "doubles"},
    }};
};

template <typename Way>
bool names_a_way(const std::string& variant){
    return parse_variant<Way>(variant).has_value();
}

}


template <typename Way>
const char* variant_name(Way way){
    for (const auto& [value, name] : WayNames<Way>::names){
        if (value == way){
            return name;
        }
    }
    return "";
}
template <typename Way>
std::optional<Way> parse_variant(const std::string& name){
    for (const auto& [value, way_name] : WayNames<Way>::names){
        if (name == way_name){
            return value;
        }
    }
    return std::nullopt;
}
template <typename Way>
std::vector<Way> all_variants(){
    std::vector<Way> ways;
    for (const auto& entry : WayNames<Way>::names){
        ways.push_back(entry.first);
    }
    return ways;
}

#define PA_BDSP_STORY_WAY(Way)  \
    template const char* variant_name<Way>(Way);    \
    template std::optional<Way> parse_variant<Way>(const std::string&); \
    template std::vector<Way> all_variants<Way>();
PA_BDSP_STORY_WAY(RouteCosts)
PA_BDSP_STORY_WAY(LegsWay)
PA_BDSP_STORY_WAY(MashWay)
PA_BDSP_STORY_WAY(StepWay)
PA_BDSP_STORY_WAY(TalkWay)
PA_BDSP_STORY_WAY(BattleFormat)
#undef PA_BDSP_STORY_WAY

bool built_in_variant(StoryStepKind kind, const std::string& variant){
    switch (kind){
    case StoryStepKind::ROUTE:
        return names_a_way<RouteCosts>(variant);
    case StoryStepKind::LEGS:
        return names_a_way<LegsWay>(variant);
    case StoryStepKind::MASH:
        return names_a_way<MashWay>(variant);
    case StoryStepKind::STEP:
        return names_a_way<StepWay>(variant);
    case StoryStepKind::TALK:
        return names_a_way<TalkWay>(variant);
    case StoryStepKind::BATTLE:
        return names_a_way<BattleFormat>(variant);
    case StoryStepKind::GAME:
        return false;
    default:
        return variant.empty();
    }
}


}
}
}

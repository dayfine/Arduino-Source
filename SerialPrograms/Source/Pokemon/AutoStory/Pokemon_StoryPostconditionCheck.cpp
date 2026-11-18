/*  Story Postcondition Check
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <utility>
#include <variant>
#include "Pokemon_StoryPostconditionCheck.h"
#include "Pokemon_StoryStepExecutor.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string verdict_name(PostconditionVerdict verdict){
    switch (verdict){
    case PostconditionVerdict::PASSED:
        return "passed";
    case PostconditionVerdict::FAILED:
        return "failed";
    case PostconditionVerdict::NOT_CHECKED:
        return "not checked";
    }
    return "?";
}

PostconditionResult result(std::string what, PostconditionVerdict verdict, std::string detail){
    return PostconditionResult{.what = std::move(what), .verdict = verdict, .detail = std::move(detail)};
}

PostconditionResult not_read(std::string what){
    return result(std::move(what), PostconditionVerdict::NOT_CHECKED, "the game can't read it");
}

//  The first pose among a step's args (a route's target).
std::optional<GridPose> pose_arg(const StoryStep& step){
    for (const auto& [unused, value] : step.args){
        if (const GridPose* pose = std::get_if<GridPose>(&value)){
            return *pose;
        }
    }
    return std::nullopt;
}

std::string tiles_text(const std::vector<TilePos>& tiles){
    std::string text;
    for (const TilePos& tile : tiles){
        text += (text.empty() ? "" : " or ") + to_string(tile);
    }
    return text;
}

void check_map(const GridPose& end, const ObservedState& observed, std::vector<PostconditionResult>& out){
    if (!observed.map){
        out.push_back(not_read("map"));
        return;
    }
    const bool same = *observed.map == end.map;
    out.push_back(result(
        "map", same ? PostconditionVerdict::PASSED : PostconditionVerdict::FAILED,
        same ? end.map : std::format("expected {}, read {}", end.map, *observed.map)
    ));
}

//  The tile and facing mean nothing on another map: not checked then.
void check_tile(const GridPose& end, const ObservedState& observed, bool map_failed, std::vector<PostconditionResult>& out){
    if (end.tiles.empty()){
        return;
    }
    if (map_failed){
        out.push_back(result("tile", PostconditionVerdict::NOT_CHECKED, "on another map"));
        return;
    }
    if (!observed.tile){
        out.push_back(not_read("tile"));
        return;
    }
    const bool on = std::find(end.tiles.begin(), end.tiles.end(), *observed.tile) != end.tiles.end();
    out.push_back(result(
        "tile", on ? PostconditionVerdict::PASSED : PostconditionVerdict::FAILED,
        on ? to_string(*observed.tile) : std::format("expected {}, read {}", tiles_text(end.tiles), to_string(*observed.tile))
    ));
}

void check_facing(const GridPose& end, const ObservedState& observed, bool map_failed, std::vector<PostconditionResult>& out){
    if (!end.facing){
        return;
    }
    if (map_failed){
        out.push_back(result("facing", PostconditionVerdict::NOT_CHECKED, "on another map"));
        return;
    }
    if (!observed.facing){
        out.push_back(not_read("facing"));
        return;
    }
    const bool same = *observed.facing == *end.facing;
    out.push_back(result(
        "facing", same ? PostconditionVerdict::PASSED : PostconditionVerdict::FAILED,
        same ? direction_name(*end.facing)
             : std::format("expected {}, read {}", direction_name(*end.facing), direction_name(*observed.facing))
    ));
}

void check_position(const StoryCheckpoint& checkpoint, const ObservedState& observed, std::vector<PostconditionResult>& out){
    const std::optional<GridPose> end = checkpoint_end_pose(checkpoint);
    if (!end){
        return;
    }
    check_map(*end, observed, out);
    const bool map_failed = out.back().verdict == PostconditionVerdict::FAILED;
    check_tile(*end, observed, map_failed, out);
    check_facing(*end, observed, map_failed, out);
}

void check_items(const StoryPostconditions& expected, const ObservedState& observed, std::vector<PostconditionResult>& out){
    for (const StoryItem& item : expected.items){
        const std::string what = std::format("item {}", item.item);
        const auto read = observed.items.find(item.item);
        if (read == observed.items.end()){
            out.push_back(not_read(what));
            continue;
        }
        const bool same = read->second == item.quantity;
        out.push_back(result(
            what, same ? PostconditionVerdict::PASSED : PostconditionVerdict::FAILED,
            same ? std::format("{}", item.quantity) : std::format("expected {}, read {}", item.quantity, read->second)
        ));
    }
}

void check_flags(const StoryPostconditions& expected, const ObservedState& observed, std::vector<PostconditionResult>& out){
    for (const std::string& flag : expected.flags){
        const std::string what = std::format("flag {}", flag);
        const auto read = observed.flags.find(flag);
        if (read == observed.flags.end()){
            out.push_back(not_read(what));
            continue;
        }
        out.push_back(result(
            what, read->second ? PostconditionVerdict::PASSED : PostconditionVerdict::FAILED,
            read->second ? "set" : "expected set, read clear"
        ));
    }
}

std::string summary_verdict(const PostconditionReport& report){
    if (report.results.empty()){
        return "nothing to check";
    }
    if (report.count(PostconditionVerdict::FAILED) > 0){
        return "failed";
    }
    return report.all_passed() ? "passed" : "incomplete";
}

} // namespace


size_t PostconditionReport::count(PostconditionVerdict verdict) const{
    size_t total = 0;
    for (const PostconditionResult& item : results){
        total += item.verdict == verdict ? 1 : 0;
    }
    return total;
}

bool PostconditionReport::all_passed() const{
    return !results.empty() && count(PostconditionVerdict::PASSED) == results.size();
}


std::optional<GridPose> checkpoint_end_pose(const StoryCheckpoint& checkpoint){
    if (checkpoint.postconditions.position){
        return checkpoint.postconditions.position;
    }
    for (auto step = checkpoint.steps.rbegin(); step != checkpoint.steps.rend(); ++step){
        if (step->pose_after){
            return grid_pose_of(*step->pose_after);
        }
        if (std::optional<GridPose> target = pose_arg(*step)){
            return target;
        }
        if (step->kind == StoryStepKind::LEGS){
            return std::nullopt;
        }
    }
    return std::nullopt;
}

PostconditionReport check_postconditions(const StoryCheckpoint& checkpoint, const ObservedState& observed){
    PostconditionReport report;
    report.checkpoint = checkpoint.id;
    check_position(checkpoint, observed, report.results);
    check_items(checkpoint.postconditions, observed, report.results);
    check_flags(checkpoint.postconditions, observed, report.results);
    return report;
}

std::vector<std::string> postcondition_log_lines(const PostconditionReport& report){
    std::vector<std::string> lines;
    for (const PostconditionResult& item : report.results){
        lines.push_back(std::format(
            "[Postcondition] {} {}: {}: {}", report.checkpoint, item.what, verdict_name(item.verdict), item.detail
        ));
    }
    lines.push_back(std::format(
        "[Postcondition] {}: {} passed, {} failed, {} not checked: {}",
        report.checkpoint,
        report.count(PostconditionVerdict::PASSED),
        report.count(PostconditionVerdict::FAILED),
        report.count(PostconditionVerdict::NOT_CHECKED),
        summary_verdict(report)
    ));
    return lines;
}



} // namespace Pokemon
} // namespace PokemonAutomation

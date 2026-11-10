/*  AutoStory Table Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace StoryTable{


Pokemon::Pose at(const char* map, int x, int y, std::optional<Direction> facing){
    return to_pose(StoryPose{map, {x, y}, facing});
}

StoryCheckpoint row(
    std::string id, std::string title, std::optional<Pokemon::Pose> start,
    Supplies supplies, StartMenu menu, std::vector<StoryStep> steps
){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = std::move(title);
    checkpoint.start = std::move(start);
    checkpoint.steps = std::move(steps);
    checkpoint.supplies = supplies;
    checkpoint.menu = menu;
    return checkpoint;
}

Supplies after_brock(){
    Supplies supplies;
    supplies.lead = story_lead_after_brock();
    return supplies;
}

StoryStep make(StoryStepKind kind, std::string name, std::string why){
    StoryStep step;
    step.kind = kind;
    step.name = std::move(name);
    step.why = std::move(why);
    return step;
}

StoryStep route(std::string name, Pokemon::Pose target, std::string why, WildBattles wild){
    StoryStep step = make(StoryStepKind::ROUTE, std::move(name), std::move(why));
    step.target = std::move(target);
    step.fight_wild = wild == WildBattles::FIGHT;
    return step;
}

StoryStep step_into(std::string name, Direction direction, std::string why){
    StoryStep step = make(StoryStepKind::STEP, std::move(name), std::move(why));
    step.direction = direction;
    return step;
}

StoryStep into_line(std::string name, Direction direction, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.variant = TRAINER_LINE_VARIANT;
    return step;
}

StoryStep hop(std::string name, Direction direction, Pokemon::Pose landing, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.pose_after = std::move(landing);
    return step;
}

StoryStep step_onto(std::string name, Direction direction, Pokemon::Pose tile, std::string why){
    return hop(std::move(name), direction, std::move(tile), std::move(why));
}

StoryStep answer(std::string name, bool yes, std::string why){
    StoryStep step = make(StoryStepKind::ANSWER, std::move(name), std::move(why));
    step.yes = yes;
    return step;
}

StoryStep scene(std::string name, std::optional<Pokemon::Pose> pose_after, std::string why){
    StoryStep step = make(StoryStepKind::MASH, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep battle(std::string name, Pokemon::Pose pose_after, bool may_lose, std::string why){
    StoryStep step = make(StoryStepKind::BATTLE, std::move(name), std::move(why));
    step.pose_after = std::move(pose_after);
    step.may_lose = may_lose;
    return step;
}

StoryStep legs(std::string name, std::vector<StoryLeg> legs, std::string why, WildBattles wild){
    StoryStep step = make(StoryStepKind::LEGS, std::move(name), std::move(why));
    step.legs = std::move(legs);
    step.fight_wild = wild == WildBattles::FIGHT;
    return step;
}

StoryStep running(StoryStep step){
    step.variant = RUN_VARIANT;
    return step;
}

StoryStep new_game_intro(Pokemon::Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::NEW_GAME, "intro", std::move(why));
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep talk(std::string name, std::string why){
    return make(StoryStepKind::TALK, std::move(name), std::move(why));
}

StoryStep ready(std::string name, Pokemon::Pose nurse, std::string why){
    StoryStep step = make(StoryStepKind::READY, std::move(name), std::move(why));
    step.target = std::move(nurse);
    return step;
}

StoryStep heal(std::string name){
    return make(StoryStepKind::HEAL, std::move(name), "full HP");
}

StoryStep shop(std::string name, std::vector<Pokemon::StoryItem> items, std::string why){
    StoryStep step = make(StoryStepKind::SHOP, std::move(name), std::move(why));
    step.items = std::move(items);
    return step;
}

StoryStep game_step(std::string variant, Pokemon::Pose pose_after, std::string why){
    StoryStep step = make(StoryStepKind::GAME, variant, std::move(why));
    step.variant = std::move(variant);
    step.pose_after = std::move(pose_after);
    return step;
}

StoryStep options_step(){
    return make(StoryStepKind::OPTIONS, "options", "fast text; no switch prompt between a trainer's Pokemon");
}
StoryStep into_sight(std::string name, Direction direction, std::string why){
    StoryStep step = step_into(std::move(name), direction, std::move(why));
    step.variant = TRAINER_WAIT_VARIANT;
    return step;
}

StoryStep cut_tree(std::string name, Pokemon::Pose tree, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why));
    step.variant = CUT_TREE_VARIANT;
    step.direction = facing_of(tree).value_or(Direction::NORTH);
    step.pose_after = std::move(tree);
    return step;
}

StoryStep poster_switch(std::string name, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why));
    step.variant = POSTER_SWITCH_VARIANT;
    return step;
}

StoryStep lift_floor(std::string name, std::string_view from, std::string_view to, std::string why){
    StoryStep step = make(StoryStepKind::GAME, std::move(name), std::move(why));
    step.variant = lift_floor_variant(from, to);
    return step;
}



} // namespace StoryTable
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

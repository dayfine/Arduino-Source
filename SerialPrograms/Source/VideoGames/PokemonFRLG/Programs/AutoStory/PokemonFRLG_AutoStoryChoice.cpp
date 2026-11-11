/*  AutoStory Choice
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryChoice.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


std::string_view fossil_name(Fossil fossil){
    switch (fossil){
    case Fossil::DOME:  return "dome";
    case Fossil::HELIX: return "helix";
    }
    return "?";
}

std::optional<Fossil> fossil_from_name(std::string_view name){
    for (Fossil fossil : {Fossil::DOME, Fossil::HELIX}){
        if (name == fossil_name(fossil)){
            return fossil;
        }
    }
    return std::nullopt;
}

Pokemon::TilePos fossil_tile(Fossil fossil){
    return fossil == Fossil::DOME ? Pokemon::TilePos{13, 7} : Pokemon::TilePos{14, 7};
}

std::string_view choice_end_name(ChoiceEnd end){
    switch (end){
    case ChoiceEnd::TAKEN:      return "taken";
    case ChoiceEnd::WRONG_ONE:  return "wrong_one";
    case ChoiceEnd::FAILED:     return "failed";
    }
    return "?";
}


std::optional<Fossil> fossil_of(const Pokemon::StoryActionArgs& args){
    const std::string* name = game_arg<std::string>(args, "fossil");
    if (name == nullptr){
        return std::nullopt;
    }
    return fossil_from_name(*name);
}

Pokemon::StoryStep fossil_step(std::string name, std::string why, Fossil fossil){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = FOSSIL_ACTION;
    step.args = {{"fossil", std::string(fossil_name(fossil))}};
    return step;
}


namespace{

ChoiceResult choice_end(ChoiceEnd end, std::string detail){
    return {.end = end, .confirmed = false, .detail = std::move(detail)};
}

//  NO on the prompt, then the overworld; ends as `end`.
ChoiceResult decline(ChoiceActions& actions, ChoiceEnd end, std::string detail){
    actions.answer(false);
    if (!actions.finish()){
        return choice_end(ChoiceEnd::FAILED, "the overworld didn't come back after NO");
    }
    return choice_end(end, std::move(detail));
}

} // namespace

ChoiceResult run_fossil_choice(Logger& logger, ChoiceActions& actions, Fossil fossil){
    if (!actions.talk()){
        return choice_end(ChoiceEnd::FAILED, "no YES/NO after talking to the fossil");
    }
    //  Unread, the offer is taken to be the fossil the table faced the
    //  player to.
    const std::optional<Fossil> offer = actions.read_offer();
    if (offer && *offer != fossil){
        return decline(actions, ChoiceEnd::WRONG_ONE, std::format("facing the {} fossil, not the {}", fossil_name(*offer), fossil_name(fossil)));
    }
    actions.answer(true);
    if (!actions.finish()){
        return choice_end(ChoiceEnd::FAILED, "the overworld didn't come back after the fossil");
    }
    const std::optional<Fossil> held = actions.fossil_in_bag();
    if (held && *held != fossil){
        return choice_end(ChoiceEnd::FAILED, std::format("the bag holds the {} fossil", fossil_name(*held)));
    }
    logger.log(std::format(
        "Choice: took the {} fossil{}{}.", fossil_name(fossil), offer ? "" : ", by where the player faces (the prompt isn't read)",
        held ? ", seen in the bag" : " (the bag isn't read)"
    ));
    return {.end = ChoiceEnd::TAKEN, .confirmed = held.has_value(), .detail = ""};
}


ConsoleChoiceActions::ConsoleChoiceActions(ConsoleHandle& console, ProControllerContext& context)
    : m_console(console)
    , m_context(context)
{}

bool ConsoleChoiceActions::talk(){
    pbf_press_button(m_context, BUTTON_A, 160ms, 500ms);
    const WallClock deadline = current_time() + 5000ms;
    while (current_time() < deadline){
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_console.video().snapshot();
        if (screen && yes_no_cursor(screen)){
            return true;
        }
        pbf_wait(m_context, 250ms);
    }
    return false;
}

std::optional<Fossil> ConsoleChoiceActions::read_offer(){
    m_console.log("Choice: no reader for the fossil prompt's text yet.", COLOR_RED);
    return std::nullopt;
}

void ConsoleChoiceActions::answer(bool yes){
    //  Reads the cursor (it opens on YES) and answers; B answers NO.
    answer_yes_no(m_console, m_context, yes, BUTTON_B, 5000ms);
}

bool ConsoleChoiceActions::finish(){
    try{
        mash_until_overworld(m_console, m_context, 30000ms);
        return true;
    }catch (OperationFailedException& e){
        m_console.log("Choice: " + e.message(), COLOR_RED);
        return false;
    }
}

std::optional<Fossil> ConsoleChoiceActions::fossil_in_bag(){
    //  The item list reader has no names for the fossils yet.
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation

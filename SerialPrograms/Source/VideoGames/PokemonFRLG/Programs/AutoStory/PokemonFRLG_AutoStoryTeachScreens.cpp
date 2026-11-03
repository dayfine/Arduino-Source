/*  AutoStory Teach Screens
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonFRLG_AutoStoryTeachScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


const char* teach_press_name(TeachPress press){
    switch (press){
    case TeachPress::A:     return "A";
    case TeachPress::B:     return "B";
    case TeachPress::UP:    return "UP";
    case TeachPress::DOWN:  return "DOWN";
    case TeachPress::LEFT:  return "LEFT";
    case TeachPress::RIGHT: return "RIGHT";
    }
    return "?";
}


ScreenTeachActions::ScreenTeachActions(Logger& logger, TeachScreens& screens)
    : m_logger(logger)
    , m_screens(screens)
{}

bool ScreenTeachActions::open_item(std::string_view item, std::string_view){
    m_logger.log(std::string("Teach: no reader for the TM Case yet; not opening ") + std::string(item) + ".", COLOR_RED);
    return false;
}
TeachPrompt ScreenTeachActions::choose_member(size_t){
    return TeachPrompt::FAILED;
}
ForgetEnd ScreenTeachActions::forget(size_t){
    return ForgetEnd::FAILED;
}
void ScreenTeachActions::stop_teaching(){
    (void)m_screens;
}
bool ScreenTeachActions::leave(){
    //  Nothing was opened.
    return true;
}


ConsoleTeachScreens::ConsoleTeachScreens(ConsoleHandle& console, ProControllerContext& context, Language language)
    : m_console(console)
    , m_context(context)
    , m_language(language)
{}

bool ConsoleTeachScreens::open_bag(){
    (void)m_console;
    (void)m_context;
    (void)m_language;
    return false;
}
TeachView ConsoleTeachScreens::look(){
    return {};
}
void ConsoleTeachScreens::press(TeachPress){}
bool ConsoleTeachScreens::to_overworld(){
    return true;
}



}
}
}

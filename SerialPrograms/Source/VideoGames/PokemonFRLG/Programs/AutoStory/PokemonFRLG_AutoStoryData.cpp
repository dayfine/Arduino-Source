/*  Authored FRLG Checkpoint Data
 *  From: https://github.com/PokemonAutomation/
 */
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepJson.h"
#include "PokemonFRLG_AutoStoryExport.h"
#include "PokemonFRLG_AutoStoryData.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace{
void exact_fields(const JsonObject& value, std::initializer_list<std::string_view> names){
    if (value.size() != names.size()){
        throw ParseException("Authored checkpoint: missing or extra fields.");
    }
    for (std::string_view name : names){
        value.get_value_throw(std::string(name));
    }
}
} // namespace
StoryCheckpoint read_authored_checkpoint(const JsonValue& value, std::string_view source){
    const JsonObject& root = value.to_object_throw(std::string(source));
    exact_fields(root, {"format", "game", "checkpoint", "execution"});
    if (root.get_string_throw("format") != "pokesim-authored-checkpoint/1" || root.get_string_throw("game") != "frlg"){
        throw ParseException("Unsupported authored checkpoint format/game.");
    }
    const JsonObject& execution = root.get_object_throw("execution");
    exact_fields(execution, {"menu"});
    StoryCheckpoint result;
    static_cast<Pokemon::StoryCheckpoint&>(result) = Pokemon::read_story_checkpoint(root.get_value_throw("checkpoint"), source);
    const std::string& menu = execution.get_string_throw("menu");
    if (menu == "before_pokemon"){
        result.menu = StartMenu::BEFORE_POKEMON;
    }else if (menu == "before_pokedex"){
        result.menu = StartMenu::BEFORE_POKEDEX;
    }else if (menu == "full"){
        result.menu = StartMenu::FULL;
    }else{
        throw ParseException("Unsupported authored checkpoint menu.");
    }
    return result;
}
StoryCheckpoint load_authored_checkpoint(std::string_view file){
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == file){
            return read_authored_checkpoint(parse_json(text), file);
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Authored checkpoint is not embedded: " + std::string(file));
}
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
